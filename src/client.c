#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "jeux_globals.h"
#include "client.h"
#include "debug.h"
#include "packet_common.h"

struct client {
    pthread_mutex_t player_mutex;
    PLAYER *player; /* if null then is logged out*/
    pthread_mutex_t fd_mutex;
    int fd;
    struct invitation_lst_t {
        INVITATION **lst; 
        pthread_mutex_t inv_mutex;
        volatile size_t len;
        size_t cap;
    } invs;
    volatile size_t ref_count;
    pthread_mutex_t mutex;
};

static int search_inv_lst(CLIENT *cli, INVITATION *inv) {
    if(cli == NULL) {
        return -1;
    }
    if(cli->invs.lst == NULL) {
        return -1;
    }
    if(cli->invs.len == 0) {
        return -1;
    }
    int ret_idx = -1;
    for(int idx = 0; idx < cli->invs.cap; idx++) {
        if(cli->invs.lst[idx] == inv) {
            ret_idx = idx;
            break;
        }
    }
    return ret_idx;
}

static int search_inv_lst_safe(CLIENT *cli, INVITATION *inv) {
    if(cli == NULL) {
        return -1;
    }
    pthread_mutex_lock(&cli->invs.inv_mutex);
    int id = search_inv_lst(cli, inv);
    pthread_mutex_unlock(&cli->invs.inv_mutex);
    return id;
}

static int insert_into_inv_lst(CLIENT *cli, INVITATION *inv) {
    if(cli == NULL) {
        return -1;
    }
    if(cli->invs.lst == NULL) {
        return -1;
    }
    if(cli->invs.len == cli->invs.cap) {
        return -1;
    }
    int ret_idx = -1;
    for(int idx = 0; idx < cli->invs.cap; idx++) {
        if(cli->invs.lst[idx] == NULL) {
            cli->invs.lst[idx] = inv_ref(inv, "for invitation being added to client's list");
            ret_idx = idx;
            break;
        }
    }
    cli->invs.len++;
    return ret_idx;
}

static int remove_from_inv_lst(CLIENT *cli, INVITATION *inv) {
    if(cli == NULL) {
        return -1;
    }
    if(cli->invs.lst == NULL) {
        return -1;
    }
    if(cli->invs.len == 0) {
        return -1;
    }
    for(int idx = 0; idx < cli->invs.cap; idx++) {
        if(cli->invs.lst[idx] == inv) {
            inv_unref(cli->invs.lst[idx], "for invitation being removed from client's list");
            cli->invs.lst[idx] = NULL;
            cli->invs.len--;
            return idx;
        }
    }
    return -1;
}

static void destroy_inv_lst_safe(CLIENT *cli) {
    if(cli == NULL) {
        return;
    }
    pthread_mutex_lock(&cli->invs.inv_mutex);
    if(cli->invs.lst == NULL) {
        pthread_mutex_unlock(&cli->invs.inv_mutex);
        return;
    }
    for(int idx = 0; idx < cli->invs.cap; idx++) {
        if(cli->invs.lst[idx] != NULL) {
            inv_unref(cli->invs.lst[idx], "for invitation being removed from client's list");
        }
    }
    free(cli->invs.lst);
    cli->invs.lst = NULL;
    pthread_mutex_unlock(&cli->invs.inv_mutex);
    pthread_mutex_destroy(&cli->invs.inv_mutex);
}

static void client_set_player_safe(CLIENT *cli, PLAYER *player) {
    pthread_mutex_lock(&cli->player_mutex);
    cli->player = player;
    pthread_mutex_unlock(&cli->player_mutex);
}

CLIENT *client_create(CLIENT_REGISTRY *creg, int fd) {
    CLIENT *cli = malloc(sizeof(CLIENT));
    if(cli == NULL) {
        debug("%ld: Failed to initialize new client", pthread_self());
        return NULL;
    }

    cli->player = NULL; 
    cli->fd = fd;
    cli->invs.lst = calloc((MAX_CLIENTS * 128), sizeof(INVITATION *));
    if(cli->invs.lst == NULL) {
        free(cli);
        debug("%ld: Failed to initialize new client", pthread_self());
        return NULL;
    }
    cli->invs.len = 0;
    cli->invs.cap = MAX_CLIENTS * 128;

    pthread_mutexattr_t attr;

    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);

    pthread_mutex_init(&(cli->invs.inv_mutex), &attr);

    cli->ref_count = 0;
    pthread_mutex_init(&cli->mutex, NULL);
    pthread_mutex_init(&cli->player_mutex, NULL);
    pthread_mutex_init(&cli->fd_mutex, NULL);

    return client_ref(cli, "for newly created client");
}

CLIENT *client_ref(CLIENT *client, char *why) {
    if(client == NULL) {
        return NULL;
    }
    pthread_mutex_lock(&client->mutex);
    size_t old_ref = client->ref_count;
    client->ref_count = old_ref + 1;

    debug("%ld: Increase reference count on client %p (%lu -> %lu) %s", pthread_self(), client, old_ref, client->ref_count, why); 
    pthread_mutex_unlock(&client->mutex);

    return client;
}

void client_unref(CLIENT *client, char *why) {
    if(client == NULL) {
        debug("%ld: Invalid client object!", pthread_self());
        return;
    }
    pthread_mutex_lock(&client->mutex);

    size_t old_ref = client->ref_count;
    client->ref_count = old_ref - 1;

    debug("%ld: Decrease reference count on client %p (%lu -> %lu) %s", pthread_self(), client, old_ref, client->ref_count, why); 

    if(client->ref_count == 0) {
        destroy_inv_lst_safe(client);

        pthread_mutex_unlock(&client->mutex);
        pthread_mutex_destroy(&client->mutex);
        pthread_mutex_destroy(&client->fd_mutex);
        client_set_player_safe(client, NULL);
        pthread_mutex_destroy(&client->player_mutex);


        free(client);
        debug("%ld: Free client %p", pthread_self(), client);
        return;
    }
    pthread_mutex_unlock(&client->mutex);
}

int client_login(CLIENT *client, PLAYER *player) {
    if(client == NULL) {
        debug("%ld: Invalid client object!", pthread_self());
        return -1;
    }
    if(client_get_player(client) != NULL) {
        debug("%ld: [%d] Already logged in", pthread_self(), client->fd);
        return -1;
    }

    CLIENT *lookup_cli = creg_lookup(client_registry, player_get_name(player));
    if(lookup_cli != NULL) {
        debug("%ld: Some other client already logged into this player", pthread_self());
        client_unref(lookup_cli, "because client was already logged in and removing ref from lookup");
        return -1;
    }
    // no need to unref since creg_lookup failed
    client_set_player_safe(client, 
            player_ref(player, "for client keeping reference to player"));
    return 0;
}

// TODO rewrite revoke and decline???
int client_logout(CLIENT *client) {
    if(client == NULL) {
        debug("%ld: Invalid client object!", pthread_self());
        return -1;
    }
    if(client_get_player(client) == NULL) {
        debug("%ld: [%d] Client was not logged in", pthread_self(), client->fd);
        return -1;
    }

    for(int i = 0; i < client->invs.cap; ++i) {
        if(client_resign_game(client, i) == -1) {
            if(client_revoke_invitation(client, i) == -1) {
                client_decline_invitation(client, i);
            }
        }
    }
    player_unref(client_get_player(client), "because client is logging out");
    client_set_player_safe(client, NULL);
    return 0;
}


PLAYER *client_get_player(CLIENT *client) {
    if(client == NULL) {
        return NULL;
    }
    pthread_mutex_lock(&client->player_mutex);
    PLAYER *ret = client->player;
    pthread_mutex_unlock(&client->player_mutex);
    return ret;
}

int client_get_fd(CLIENT *client) {
    return client->fd;
}

int client_send_packet(CLIENT *player, JEUX_PACKET_HEADER *pkt, void *data) {
    if(player == NULL) {
        return -1;
    }
    if(pkt == NULL) {
        return -1;
    }
    pthread_mutex_lock(&player->fd_mutex);
    int fd = player->fd;
    int status = proto_send_packet(fd, pkt, data);
    pthread_mutex_unlock(&player->fd_mutex);

    return status;
}

int client_send_ack(CLIENT *client, void *data, size_t datalen) {
    if(client == NULL) {
        return -1;
    }
    JEUX_PACKET_HEADER jph = {0};
    pack_header(&jph, JEUX_ACK_PKT, 0, 0, datalen);
    return client_send_packet(client, &jph, data);
}

int client_send_nack(CLIENT *client) {
    if(client == NULL) {
        return -1;
    }
    JEUX_PACKET_HEADER jph = {0};
    pack_header(&jph, JEUX_NACK_PKT, 0, 0, 0);
    return client_send_packet(client, &jph, NULL);
}

int client_add_invitation(CLIENT *client, INVITATION *inv) {
    if(client == NULL) {
        return -1;
    }
    pthread_mutex_lock(&client->invs.inv_mutex);
    int id = insert_into_inv_lst(client, inv);
    pthread_mutex_unlock(&client->invs.inv_mutex);

    return id;
}

int client_remove_invitation(CLIENT *client, INVITATION *inv) {
    if(client == NULL) {
        return -1;
    }
    pthread_mutex_lock(&client->invs.inv_mutex);
    int id = remove_from_inv_lst(client, inv);
    pthread_mutex_unlock(&client->invs.inv_mutex);
    return id;
}

int client_make_invitation(CLIENT *source, CLIENT *target, GAME_ROLE source_role, GAME_ROLE target_role) {
    INVITATION *new_inv = inv_create(source, target, source_role, target_role);
    // must unref
    if(new_inv == NULL) {
        return -1;
    }
    int src_id, dst_id;
    src_id = client_add_invitation(source, new_inv);
    if(src_id == -1) {
        inv_unref(new_inv, "because source invitation failed.");
        return src_id;
    }
    dst_id = client_add_invitation(target, new_inv);
    if(dst_id == -1) {
        inv_unref(new_inv, "because target invitation failed.");
        return client_remove_invitation(source, new_inv);
    }
    inv_unref(new_inv, "because pointer to invite is being discarded in function");

    PLAYER *player_src = client_get_player(source); 
    player_ref(player_src, "because pointer to player is being referenced in invitation maker");
    JEUX_PACKET_HEADER jph = {0};
    pack_header(&jph, JEUX_INVITED_PKT, dst_id, inv_get_target_role(new_inv), strlen(player_get_name(player_src)));
    if(client_send_packet(target, &jph, player_get_name(player_src)) == -1) {
        player_unref(player_src, "because pointer to player is being discarded in invitation maker");
        return -1;
    }
    player_unref(player_src, "because pointer to player is being discarded in invitation maker");

    return src_id;
}

int client_revoke_invitation(CLIENT *client, int id) {
    if(client == NULL) {
        return -1;
    }
    pthread_mutex_lock(&client->invs.inv_mutex);
    INVITATION *inv = client->invs.lst[id];
    pthread_mutex_unlock(&client->invs.inv_mutex);
    if(inv == NULL) {
        return -1;
    }
    inv_ref(inv, "because being indexed by revoker");
    if(client != inv_get_source(inv)) {
        debug("%ld: ERROR- Source role is not equivalent to client", pthread_self());
        inv_unref(inv, "because pointer is being discarded by revoker");
        return -1;
    }
    if((inv_close(inv, NULL_ROLE)) == -1) {
        inv_unref(inv, "because pointer is being discarded by revoker");
        debug("%ld: Failed to close invitation %p", pthread_self(), inv);
        return -1;
    }
    if(client_remove_invitation(client, inv) == -1) {
        inv_unref(inv, "because pointer is being discarded by revoker");
        return -1;
    }
    CLIENT *client_target = inv_get_target(inv);
    if(client_ref(client_target, "because getting target to remove") == NULL) {
        inv_unref(inv, "because pointer is being discarded by revoker");
        return -1;
    }
    int target_id;
    if((target_id = client_remove_invitation(client_target, inv)) == -1) {
        client_unref(client_target, "because getting target to remove");
        inv_unref(inv, "because pointer is being discarded by revoker");
        return -1;
    }
    inv_unref(inv, "because pointer is being discarded by revoker");

    JEUX_PACKET_HEADER jph = {0};
    pack_header(&jph, JEUX_REVOKED_PKT, target_id, 0, 0);
    int status_ret = client_send_packet(client_target, &jph, NULL);

    client_unref(client_target, "because getting target to remove");
    return status_ret;
}

int client_decline_invitation(CLIENT *client, int id) {
    if(client == NULL) {
        return -1;
    }
    pthread_mutex_lock(&client->invs.inv_mutex);
    INVITATION *inv = client->invs.lst[id];
    pthread_mutex_unlock(&client->invs.inv_mutex);
    if(inv == NULL) {
        return -1;
    }
    inv_ref(inv, "because being indexed by decliner");
    if(client != inv_get_target(inv)) {
        debug("%ld: ERROR- Target role is not equivalent to client", pthread_self());
        inv_unref(inv, "because pointer is being discarded by decliner");
        return -1;
    }
    if((inv_close(inv, NULL_ROLE)) == -1) {
        debug("%ld: ERROR- Client Target Failed To Reference", pthread_self());
        inv_unref(inv, "because pointer is being discarded by decliner");
        return -1;
    }

    if(client_remove_invitation(client, inv) == -1) {
        inv_unref(inv, "because pointer is being discarded by decliner");
        return -1;
    }
    CLIENT *client_src = inv_get_source(inv);
    if(client_ref(client_src, "because getting source to remove") == NULL) {
        debug("%ld: ERROR- Client Source Failed To Reference", pthread_self());
        inv_unref(inv, "because pointer is being discarded by decliner");
        return -1;
    }
    debug("AWDWADAWDWADAW\n");
    int src_id;
    if((src_id = client_remove_invitation(client_src, inv)) == -1) {
        debug("%ld: ERROR- Client Target Failed To Reference", pthread_self());
        client_unref(client_src, "because getting src to remove");
        inv_unref(inv, "because pointer is being discarded by decliner");
        return -1;
    }
    debug("AWDWADAWDWADAW 2 2 2 2\n");
    inv_unref(inv, "because pointer is being discarded by decliner");

    JEUX_PACKET_HEADER jph = {0};
    pack_header(&jph, JEUX_DECLINED_PKT, src_id, 0, 0);
    int status_ret = client_send_packet(client_src, &jph, NULL);
    client_unref(client_src, "because getting target to remove");
    return status_ret;
}

int client_accept_invitation(CLIENT *client, int id, char **strp) {
    if(client == NULL) {
        return -1;
    }
    pthread_mutex_lock(&client->invs.inv_mutex);
    INVITATION *inv = client->invs.lst[id];
    pthread_mutex_unlock(&client->invs.inv_mutex);
    if(inv == NULL) {
        return -1;
    }
    inv_ref(inv, "because being indexed by accepter");
    if(client != inv_get_target(inv)) {
        debug("%ld: ERROR- Target role is not equivalent to client", pthread_self());
        inv_unref(inv, "because pointer is being discarded by accepter");
        return -1;
    }
    if(inv_accept(inv) == -1) {
        debug("%ld: ERROR- Failed to accept invitation", pthread_self());
        inv_unref(inv, "because pointer is being discarded by accepter");
        return -1;
    }
    CLIENT *client_src = inv_get_source(inv);
    if(client_ref(client_src, "because getting source to remove") == NULL) {
        debug("%ld: ERROR- Client Source Failed To Reference", pthread_self());
        inv_unref(inv, "because pointer is being discarded by accepter");
        return -1;
    }
    GAME_ROLE src_role = inv_get_source_role(inv);
    size_t datalen = 0;
    char *str_to_send = NULL;

    GAME *game = game_ref(inv_get_game(inv), "because need to unparse state");
    if(game == NULL) {
        client_unref(client_src, "because source needs to be unrefed");
        inv_unref(inv, "because pointer is being discarded by accepter");
        return -1;
    }
    if(src_role == FIRST_PLAYER_ROLE) {
        str_to_send = game_unparse_state(game); 
        game_unref(game, "because game needs to be dereferenced by accepter");
        if(str_to_send == NULL) {
            client_unref(client_src, "because source needs to be unrefed");
            inv_unref(inv, "because pointer is being discarded by accepter");
            return -1;
        }
        datalen = strlen(str_to_send);
    } else {
        *strp = game_unparse_state(game);
        game_unref(game, "because game needs to be dereferenced by accepter");
        if(*strp == NULL) {
            client_unref(client_src, "because source needs to be unrefed");
            inv_unref(inv, "because pointer is being discarded by accepter");
            return -1;
        }
    }
    int src_id = search_inv_lst_safe(client_src, inv);
    if(src_id == -1) {
        if(str_to_send != NULL) {
            free(str_to_send);
        }
        client_unref(client_src, "because source needs to be unrefed");
        inv_unref(inv, "because pointer is being discarded by accepter");
        return -1;
    }
    inv_unref(inv, "because pointer is being discarded by accepter");
    JEUX_PACKET_HEADER jph = {0};
    pack_header(&jph, JEUX_ACCEPTED_PKT, src_id, src_role, datalen);
    int status_ret = client_send_packet(client_src, &jph, str_to_send);  
    if(str_to_send != NULL) {
        free(str_to_send);
    }
    client_unref(client_src, "because source needs to be unrefed");
    return status_ret;
}

int client_resign_game(CLIENT *client, int id) {
    if(client == NULL) {
        debug("%ld: ERROR- Client Source Failed To Reference", pthread_self());
        return -1;
    }
    pthread_mutex_lock(&client->invs.inv_mutex);
    INVITATION *inv = client->invs.lst[id];
    pthread_mutex_unlock(&client->invs.inv_mutex);
    if(inv == NULL) {
        return -1;
    }
    inv_ref(inv, "because pointer is being indexed by resigner");

    if(inv_get_source(inv) == client) {
        if(inv_close(inv, inv_get_source_role(inv)) == -1) {
            debug("%ld: ERROR- Was not in ACCEPTED STATE", pthread_self());
            inv_unref(inv, "because pointer is being discarded by resigner");
            return -1;
        }
        if(client_remove_invitation(client, inv) == -1) {
            debug("%ld: ERROR- Failed to remove invitation from SOURCE", pthread_self());
            inv_unref(inv, "because pointer is being discarded by resigner");
            return -1;
        }
        CLIENT *cli_target = inv_get_target(inv);
        if(client_ref(cli_target, "because target being referenced by resigner") == NULL) {
            debug("%ld: ERROR- Failed to reference cli_target", pthread_self());
            inv_unref(inv, "because pointer is being discarded by resigner");
            return -1;
        }
        int target_id;
        if((target_id = client_remove_invitation(cli_target, inv)) == -1) {
            debug("%ld: ERROR- Failed to remove invitation from TARGET", pthread_self());
            client_unref(cli_target, "because target being discarded by resigner");
            inv_unref(inv, "because pointer is being discarded by resigner");
            return -1;
        }
        debug("%ld: Sending RESIGNED packet to TARGET (%p)", pthread_self(), cli_target);
        JEUX_PACKET_HEADER jph = {0};
        pack_header(&jph, JEUX_RESIGNED_PKT, target_id, 0, 0);
        client_send_packet(cli_target, &jph, NULL);

        debug("%ld: Sending ENDED packet to SOURCE (%p)", pthread_self(), client);
        JEUX_PACKET_HEADER jph1 = {0};
        pack_header(&jph1, 
                JEUX_ENDED_PKT, 
                id,
                game_get_winner(inv_get_game(inv)), 
                0);
        client_send_packet(client, &jph1, NULL);

        debug("%ld: Sending ENDED packet to TARGET (%p)", pthread_self(), cli_target);
        JEUX_PACKET_HEADER jph2 = {0};
        pack_header(&jph2, 
                JEUX_ENDED_PKT, 
                target_id,
                game_get_winner(inv_get_game(inv)), 
                0);
        int status = client_send_packet(cli_target, &jph2, NULL);

        // client here is source
        GAME_ROLE gr = inv_get_source_role(inv);
        PLAYER *p1 = NULL, *p2 = NULL;
        if(gr == FIRST_PLAYER_ROLE) {
            p1 = client_get_player(inv_get_source(inv));
            p2 = client_get_player(inv_get_target(inv));
        } else {
            p1 = client_get_player(inv_get_target(inv));
            p2 = client_get_player(inv_get_source(inv));
        }
        player_post_result(p1, p2, game_get_winner(inv_get_game(inv)));

        client_unref(cli_target, "because target being discarded by resigner");
        inv_unref(inv, "because pointer is being discarded by resigner");

        return status;

    } else if(inv_get_target(inv) == client) {
        if(inv_close(inv, inv_get_target_role(inv)) == -1) {
            debug("%ld: ERROR- Was not in ACCEPTED STATE", pthread_self());
            inv_unref(inv, "because pointer is being discarded by resigner");
            return -1;
        }
        if(client_remove_invitation(client, inv) == -1) {
            debug("%ld: ERROR- Failed to remove invitation from TARGET", pthread_self());
            inv_unref(inv, "because pointer is being discarded by resigner");
            return -1;
        }
        CLIENT *cli_source = inv_get_source(inv);
        if(client_ref(cli_source, "because source being referenced by resigner") == NULL) {
            debug("%ld: ERROR- Failed to reference cli_source", pthread_self());
            inv_unref(inv, "because pointer is being discarded by resigner");
            return -1;
        }
        int src_id;
        if((src_id = client_remove_invitation(cli_source, inv)) == -1) {
            debug("%ld: ERROR- Failed to remove invitation from SOURCE", pthread_self());
            client_unref(cli_source, "because source being discarded by resigner");
            inv_unref(inv, "because pointer is being discarded by resigner");
            return -1;
        }
        debug("%ld: Sending RESIGNED packet to SOURCE (%p)", pthread_self(), cli_source);

        JEUX_PACKET_HEADER jph = {0};
        pack_header(&jph, JEUX_RESIGNED_PKT, src_id, 0, 0);
        client_send_packet(cli_source, &jph, NULL);

        debug("%ld: Sending ENDED packet to TARGET (%p)", pthread_self(), client);
        JEUX_PACKET_HEADER jph1 = {0};
        pack_header(&jph1, 
                JEUX_ENDED_PKT, 
                id,
                game_get_winner(inv_get_game(inv)), 
                0);
        client_send_packet(client, &jph1, NULL);

        debug("%ld: Sending ENDED packet to SOURCE (%p)", pthread_self(), cli_source);
        JEUX_PACKET_HEADER jph2 = {0};
        pack_header(&jph2, 
                JEUX_ENDED_PKT, 
                src_id,
                game_get_winner(inv_get_game(inv)), 
                0);
        int status = client_send_packet(cli_source, &jph2, NULL);

        // client here is target 
        GAME_ROLE gr = inv_get_target_role(inv);
        PLAYER *p1 = NULL, *p2 = NULL;
        if(gr == FIRST_PLAYER_ROLE) {
            p1 = client_get_player(inv_get_target(inv));
            p2 = client_get_player(inv_get_source(inv));
        } else {
            p1 = client_get_player(inv_get_source(inv));
            p2 = client_get_player(inv_get_target(inv));
        }
        player_post_result(p1, p2, game_get_winner(inv_get_game(inv)));

        client_unref(cli_source, "because target being discarded by resigner");
        inv_unref(inv, "because pointer is being discarded by resigner");
        return status;
    } else {
        debug("%ld: ERROR- Client does not match to either source or target", pthread_self());
        inv_unref(inv, "because pointer is being discarded by resigner");
    }
    return -1;
}

int client_make_move(CLIENT *client, int id, char *move) {
    if(client == NULL) {
        return -1;
    }
    pthread_mutex_lock(&client->invs.inv_mutex);
    INVITATION *inv = client->invs.lst[id];
    // must unref
    pthread_mutex_unlock(&client->invs.inv_mutex);
    if(inv == NULL) {
        debug("%ld: ERROR- Invite could not be found", pthread_self());
        return -1;
    }
    inv_ref(inv, "because pointer is being indexed by mover");
    GAME *game = inv_get_game(inv);
    // must unref 

    if(game == NULL) {
        inv_unref(inv, "because invite is being discarded by mover");
        debug("%ld: ERROR- GAME could not be found", pthread_self());
        return -1;
    }
    /*
     *
     * It is an error
     * if the ID does not refer to an INVITATION containing a GAME in progress,
     * if the move cannot be parsed, or if the move is not legal in the current
     * GAME state.
     *
     */
    if(game_ref(game, "because move is being made here.") == NULL) {
        inv_unref(inv, "because invite is being discarded by mover");
        debug("%ld: ERROR- GAME ref failed", pthread_self());
        return -1;
    }
    GAME_ROLE gr = NULL_ROLE;
    if(inv_get_source(inv) == client) {
        gr = inv_get_source_role(inv);
    }
    if(inv_get_target(inv) == client) {
        gr = inv_get_target_role(inv);
    }
    GAME_MOVE *gm = game_parse_move(game, gr, move);
    // must be freed
    if(gm == NULL) {
        inv_unref(inv, "because invite is being discarded by mover");
        game_unref(game, "because game is being discarded by mover");
        debug("%ld: ERROR- GAME MOVE could not be parsed", pthread_self());
        return -1;
    }
    if(game_apply_move(game, gm) == -1) {
        free(gm);
        inv_unref(inv, "because invite is being discarded by mover");
        game_unref(game, "because game is being discarded by mover");
        debug("%ld: ERROR- GAME MOVE was INVALID", pthread_self());
        return -1;
    }
    free(gm);

    CLIENT *client_opponent = NULL;
    client_opponent = inv_get_source(inv) == client ? inv_get_target(inv) : inv_get_source(inv);
    if(client_opponent == NULL) {
        inv_unref(inv, "because invite is being discarded by mover");
        game_unref(game, "because game is being discarded by mover");
        debug("%ld: ERROR- client opponent could not be referenced", pthread_self());
        return -1;
    }
    if(client_ref(client_opponent, "because reference to client opponent is being made by mover") == NULL) {
        inv_unref(inv, "because invite is being discarded by mover");
        game_unref(game, "because game is being discarded by mover");
        debug("%ld: ERROR- client opponent increased by reference", pthread_self());
        return -1;
    }
    int inv_id; 
    if((inv_id = search_inv_lst_safe(client_opponent, inv)) == -1) {
        client_unref(client_opponent, "because opponent is being discarded by mover");
        inv_unref(inv, "because invite is being discarded by mover");
        game_unref(game, "because game is being discarded by mover");
        debug("%ld: ERROR- client opponent increased by reference", pthread_self());
        return -1;
    }

    char *cur_game_state = game_unparse_state(game);
    JEUX_PACKET_HEADER jph = {0};
    pack_header(&jph, 
            JEUX_MOVED_PKT, 
            inv_id,
            0, 
            strlen(cur_game_state));
    client_send_packet(client_opponent, &jph, cur_game_state);
    free(cur_game_state);


    if(game_is_over(game)) {
        // send to client here
        JEUX_PACKET_HEADER jph1 = {0};
        pack_header(&jph1, 
                JEUX_ENDED_PKT, 
                id,
                game_get_winner(game), 
                0);
        client_send_packet(client, &jph1, NULL);

        JEUX_PACKET_HEADER jph2 = {0};
        pack_header(&jph2, 
                JEUX_ENDED_PKT, 
                inv_id,
                game_get_winner(game), 
                0);
        client_send_packet(client_opponent, &jph2, NULL);

        client_remove_invitation(client, inv);
        client_remove_invitation(client_opponent, inv);

        PLAYER *client_player = client_get_player(client); 
        player_ref(client_player, "because referenced by mover");
        PLAYER *client_opponent_player = client_get_player(client_opponent); 
        player_ref(client_opponent_player, "because referenced by mover");

        // gr refers to client role
        if(gr == FIRST_PLAYER_ROLE) {
            player_post_result(client_player, client_opponent_player, game_get_winner(game));
        } else if(gr == SECOND_PLAYER_ROLE) {
            player_post_result(client_opponent_player, client_player, game_get_winner(game));
        }
        player_unref(client_player, "because pointer being discarded by mover");
        player_unref(client_opponent_player, "because pointer being discarded by mover");
    }
    inv_unref(inv, "because invite is being discarded by mover");
    client_unref(client_opponent, "because opponent is being discarded by mover");
    game_unref(game, "because game is being discarded by mover");
    return 0;
}
