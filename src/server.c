#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "server.h"
#include "player.h"
#include "protocol.h"
#include "jeux_globals.h"
#include "debug.h"
#include "packet_common.h"

static void user_handler(CLIENT *new_client) {
    PLAYER **all_players = creg_all_players(client_registry); 
    int player_idx = 0;

    char *string_print = NULL;
    size_t sz;

    FILE *stream = open_memstream(&string_print, &sz);
    while(all_players[player_idx] != NULL) {
        char *player_name = player_get_name(all_players[player_idx]);
        int player_rating = player_get_rating(all_players[player_idx]);
        fprintf(stream, "%s\t%d\n", player_name, player_rating);
        player_unref(all_players[player_idx], "for player removed from player's list");
        player_idx++;
    }
    fclose(stream);
    free(all_players);
    // need to free string_print

    JEUX_PACKET_HEADER new_pkt = {0};
    pack_header(&new_pkt, JEUX_ACK_PKT, 0, 0, strlen(string_print));
    client_send_packet(new_client, &new_pkt, string_print);
    free(string_print);
}

static int invite_handler(CLIENT *new_client, void *payloadp, 
        JEUX_PACKET_HEADER *pkt_hdr) {
    if(payloadp == NULL) {
        return -1;
    }
    unpack_header(pkt_hdr);
    size_t payload_size = pkt_hdr->size;
    char *name = calloc(payload_size + 1, sizeof(uint8_t));
    if(name == NULL) {
        perror("calloc");
        return -1;
    }
    memcpy(name, payloadp, payload_size);
    CLIENT *client = creg_lookup(client_registry, name);
    // must unref client
    free(name);
    if(client == NULL) {
        return -1;
    }
    GAME_ROLE target_role = 
        pkt_hdr->role == FIRST_PLAYER_ROLE ? FIRST_PLAYER_ROLE : SECOND_PLAYER_ROLE;
    GAME_ROLE src_role = 
        target_role == FIRST_PLAYER_ROLE ? SECOND_PLAYER_ROLE : FIRST_PLAYER_ROLE;
    int id = client_make_invitation(new_client, client, src_role, target_role);
    client_unref(client, "after invitation attempt");
    if(id == -1) {
        return -1;
    }
    JEUX_PACKET_HEADER new_pkt = {0};
    pack_header(&new_pkt, JEUX_ACK_PKT, id, 0, 0);
    client_send_packet(new_client, &new_pkt, NULL);
    return id; 
}

static int revoke_handler(CLIENT *new_client, JEUX_PACKET_HEADER *pkt_hdr) {
    unpack_header(pkt_hdr);
    uint8_t id = pkt_hdr->id;
    int status = client_revoke_invitation(new_client, id);
    if(status == -1) {
        return -1;
    }
    return client_send_ack(new_client, NULL, 0);
}

static int decline_handler(CLIENT *new_client, JEUX_PACKET_HEADER *pkt_hdr) {
    unpack_header(pkt_hdr);
    uint8_t id = pkt_hdr->id;
    int status = client_decline_invitation(new_client, id);
    if(status == -1) {
        return -1;
    }
    return client_send_ack(new_client, NULL, 0);
}

static int accept_handler(CLIENT *new_client, JEUX_PACKET_HEADER *pkt_hdr) {
    unpack_header(pkt_hdr);
    uint8_t id = pkt_hdr->id;
    char *str_ptr = NULL;
    int status = client_accept_invitation(new_client, id, &str_ptr); 
    if(status == -1) {
        if(str_ptr != NULL) {
            free(str_ptr);
        }
        return status;
    }
    if(str_ptr != NULL) {
        status = client_send_ack(new_client, str_ptr, strlen(str_ptr));
        free(str_ptr);
        return status;
    }
    return client_send_ack(new_client, NULL, 0);
}

static int move_handler(CLIENT *new_client, void *payloadp, JEUX_PACKET_HEADER *pkt_hdr) {
    unpack_header(pkt_hdr);
    uint8_t id = pkt_hdr->id;

    size_t payload_size = pkt_hdr->size;
    char *move_name = calloc(payload_size + 1, sizeof(uint8_t));
    if(move_name == NULL) {
        perror("calloc");
        return -1;
    }
    memcpy(move_name, payloadp, payload_size);
    int move_status = client_make_move(new_client, id, move_name);
    free(move_name);
    if(move_status == -1) {
        return -1;
    }
    return client_send_ack(new_client, NULL, 0);
}

static int resign_handler(CLIENT *new_client, JEUX_PACKET_HEADER *pkt_hdr) {
    unpack_header(pkt_hdr);
    uint8_t id = pkt_hdr->id;
    int resign_status = client_resign_game(new_client, id);
    if(resign_status == -1) {
        return -1;
    }
    return client_send_ack(new_client, NULL, 0);
}

static PLAYER *login_handler(CLIENT *new_client, void *payloadp, 
        JEUX_PACKET_HEADER *pkt_hdr) {
    if(payloadp == NULL) {
        client_send_nack(new_client);
        return NULL;
    }
    unpack_header(pkt_hdr);

    size_t payload_size = pkt_hdr->size;
    char *name = calloc(payload_size + 1, sizeof(uint8_t));
    if(name == NULL) {
        perror("calloc");
        client_send_nack(new_client);
        return NULL;
    }
    memcpy(name, payloadp, payload_size);
    PLAYER *new_player = preg_register(player_registry, name); 
    if(new_player == NULL) {
        free(name);
        client_send_nack(new_client);
        return NULL;
    }
    free(name);
    if(client_login(new_client, new_player) == -1) {
        player_unref(new_player, "failed login");
        client_send_nack(new_client);
        return NULL;
    }
    int status = client_send_ack(new_client, NULL, 0);
    if(status == -1) {
        return NULL;
    }
    return new_player;
}

void *jeux_client_service(void *arg) {
    int fd = *((int *)arg);
    free(arg);
    int status;
    if((status = pthread_detach(pthread_self())) != 0) {
        perror("pthread_detach");
        exit(status);
    }
    CLIENT *new_client = NULL;
    // potentially need to ref here? 
    if((new_client = creg_register(client_registry, fd)) == NULL) {
        return NULL;
    }

    PLAYER *new_player = NULL;
    do {
        JEUX_PACKET_HEADER jph = {0};
        void *payloadp = NULL;
        int status = proto_recv_packet(fd, &jph, &payloadp);
        if(status == -1) {
            if(payloadp != NULL) {
                free(payloadp);
            }
            if(new_player != NULL) {
                player_unref(new_player, "because server thread is discarding reference to logged in player");
                client_logout(new_client);
            }
            if(creg_unregister(client_registry, new_client) == 0) {
                debug("%lu: [%d] Ending client service", pthread_self(), fd);
            }
            return NULL;
        }
        if(jph.type == JEUX_LOGIN_PKT) {
            if(new_player != NULL) {
                client_send_nack(new_client);
                debug("[%d] Already logged in (player %p [%s])", fd, 
                        new_player, player_get_name(new_player));
                if(payloadp != NULL) {
                    free(payloadp);
                }
            } else {
                new_player = login_handler(new_client, payloadp, &jph);
                free(payloadp);
            }
        } else {
            if(new_player == NULL) {
                client_send_nack(new_client);
            } else {
                switch(jph.type) {
                    case JEUX_USERS_PKT:
                        if(payloadp != NULL) {
                            free(payloadp);
                        }
                        user_handler(new_client);
                        break;
                    case JEUX_INVITE_PKT:
                        int invite_status = invite_handler(new_client, payloadp, &jph);
                        if(payloadp != NULL) {
                            free(payloadp);
                        }
                        if(invite_status == -1) {
                            client_send_nack(new_client);
                        }
                        break;
                    case JEUX_REVOKE_PKT:
                        if(payloadp != NULL) {
                            free(payloadp);
                        }
                        int revoke_status = revoke_handler(new_client, &jph);
                        if(revoke_status == -1) {
                            client_send_nack(new_client);
                        }
                        break;
                    case JEUX_DECLINE_PKT:
                        if(payloadp != NULL) {
                            free(payloadp);
                        }
                        int decline_status = decline_handler(new_client, &jph);
                        if(decline_status == -1) {
                            client_send_nack(new_client);
                        }
                        break;
                    case JEUX_ACCEPT_PKT:
                        if(payloadp != NULL) {
                            free(payloadp);
                        }
                        int accept_status = accept_handler(new_client, &jph); 
                        if(accept_status == -1) {
                            client_send_nack(new_client);
                        }
                        break;
                    case JEUX_MOVE_PKT:
                        int move_status = move_handler(new_client, payloadp, &jph); 
                        if(payloadp != NULL) {
                            free(payloadp);
                        }
                        if(move_status == -1) {
                            client_send_nack(new_client);
                        }
                        break;
                    case JEUX_RESIGN_PKT:
                        if(payloadp != NULL) {
                            free(payloadp);
                        }
                        int resign_status = resign_handler(new_client, &jph);
                        if(resign_status == -1) {
                            client_send_nack(new_client);
                        }
                        break;
                    default:
                        break;
                }
            }
        }
    } while(1);

    return NULL;
}
