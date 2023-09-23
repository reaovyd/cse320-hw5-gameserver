#include <stdlib.h>
#include <pthread.h> 

#include "jeux_globals.h"
#include "invitation.h"
#include "debug.h"

struct invitation {
    CLIENT *src;
    CLIENT *target;
    GAME_ROLE src_role;
    GAME_ROLE target_role;

    pthread_mutex_t game_mutex;
    GAME *game;

    pthread_mutex_t state_mutex;
    volatile INVITATION_STATE state;

    pthread_mutex_t mutex;
    volatile size_t ref_count; /* NOTE:XXX: might need a mutex */
};

static void set_game(INVITATION *inv, GAME *game) {
    pthread_mutex_lock(&inv->game_mutex);
    inv->game = game;
    pthread_mutex_unlock(&inv->game_mutex);
}

INVITATION *
inv_create(CLIENT *source, CLIENT *target, 
        GAME_ROLE source_role, GAME_ROLE target_role) {
    if(source == NULL || target == NULL || 
            source_role == NULL_ROLE || target_role == NULL_ROLE) {
        debug("%ld: Found invalidity", pthread_self());
        return NULL;
    }
    if(source == target) {
        debug("%ld: Cannot be same client", pthread_self());
        return NULL;
    }

    INVITATION *inv = malloc(sizeof(INVITATION));
    if(inv == NULL) {
        perror("malloc");
        return NULL;
    }
    inv->src = source;
    inv->target = target;
    inv->src_role = source_role; 
    inv->target_role = target_role;
    inv->game = NULL;
    inv->state = INV_OPEN_STATE;
    pthread_mutex_init(&inv->state_mutex, NULL);
    pthread_mutex_init(&inv->mutex, NULL);
    pthread_mutex_init(&inv->game_mutex, NULL);

    inv->ref_count = 0;

    inv_ref(inv, "for newly created invitation");
    client_ref(inv->src, "as source of new invitation");
    client_ref(inv->target, "as target of new invitation");


    return inv;
}

INVITATION *inv_ref(INVITATION *inv, char *why) {
    if(inv == NULL) {
        debug("%ld: Invalid invitation object!\n", pthread_self());
        return NULL;
    }

    pthread_mutex_lock(&inv->mutex);
    size_t old_ref = inv->ref_count;
    inv->ref_count = old_ref + 1;

    debug("%ld: Increase reference count on invitation %p (%lu -> %lu) %s", pthread_self(), inv, old_ref, inv->ref_count, why); 
    
    pthread_mutex_unlock(&inv->mutex);
    return inv;
}
void inv_unref(INVITATION *inv, char *why) {
    if(inv == NULL) {
        debug("%ld: Invalid invitation object!", pthread_self());
        return;
    }
    pthread_mutex_lock(&inv->mutex);
    size_t old_ref = inv->ref_count;
    inv->ref_count = old_ref - 1;

    debug("%ld: Decrease reference count on invitation %p (%lu -> %lu) %s", pthread_self(), inv, old_ref, inv->ref_count, why); 

    if(inv->ref_count == 0) {
        pthread_mutex_unlock(&inv->mutex);
        pthread_mutex_destroy(&inv->mutex);

        pthread_mutex_destroy(&inv->state_mutex);

        client_unref(inv->src, "because invitation is being freed");
        client_unref(inv->target, "because invitation is being freed");
        if(inv_get_game(inv) != NULL) {
            game_unref(inv_get_game(inv), "because invitation is being freed");
            set_game(inv, NULL);
        }
        pthread_mutex_destroy(&inv->game_mutex);
        debug("%ld: Free invitation %p", pthread_self(), inv);
        free(inv);
        return;
    }
    pthread_mutex_unlock(&inv->mutex);
}

CLIENT *inv_get_source(INVITATION *inv) {
    if(inv == NULL) {
        return NULL;
    }
    return inv->src;
}
CLIENT *inv_get_target(INVITATION *inv) {
    if(inv == NULL) {
        return NULL;
    }
    return inv->target;
}
GAME_ROLE inv_get_source_role(INVITATION *inv) {
    if(inv == NULL) {
        return NULL_ROLE;
    }
    return inv->src_role;
}
GAME_ROLE inv_get_target_role(INVITATION *inv) {
    if(inv == NULL) {
        return NULL_ROLE;
    }
    return inv->target_role;
}
GAME *inv_get_game(INVITATION *inv) {
    if(inv == NULL) {
        return NULL;
    }
    pthread_mutex_lock(&inv->game_mutex);
    GAME *game = inv->game;
    pthread_mutex_unlock(&inv->game_mutex);
    return game;
}
int inv_accept(INVITATION *inv) {
    if(inv == NULL) {
        return -1;
    }
    pthread_mutex_lock(&inv->state_mutex);
    if(inv->state != INV_OPEN_STATE) {
        pthread_mutex_unlock(&inv->state_mutex);
        return -1;
    }
    set_game(inv, game_create());
    if(inv_get_game(inv) == NULL) {
        return -1;
    }
    inv->state = INV_ACCEPTED_STATE;
    pthread_mutex_unlock(&inv->state_mutex);
    return 0;
}
int inv_close(INVITATION *inv, GAME_ROLE role) {
    if(inv == NULL) {
        return -1;
    }
    pthread_mutex_lock(&inv->state_mutex);
    if(inv->state != INV_OPEN_STATE && inv->state != INV_ACCEPTED_STATE) {
        pthread_mutex_unlock(&inv->state_mutex);
        return -1;
    }
    if(role == NULL_ROLE) {
        if(inv_get_game(inv) == NULL) {
            inv->state = INV_CLOSED_STATE;
            pthread_mutex_unlock(&inv->state_mutex);
            return 0;
        } else {
            pthread_mutex_unlock(&inv->state_mutex);
            return -1;
        }
    }
    int status = game_resign(inv_get_game(inv), role);
    if(status != -1) {
        inv->state = INV_CLOSED_STATE;
    }
    pthread_mutex_unlock(&inv->state_mutex);
    return status;
}
