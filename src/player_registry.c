#include <pthread.h>
#include <string.h>
#include <stdlib.h>

#include "jeux_globals.h"
#include "debug.h"
#include "player_registry.h"

struct player_registry {
    PLAYER **preg_arr;
    pthread_mutex_t mutex; 
    volatile size_t len;
    size_t cap;
};

PLAYER_REGISTRY *preg_init() {
    PLAYER_REGISTRY *pr = malloc(sizeof(PLAYER_REGISTRY));
    if(pr == NULL) {
        debug("%ld: Failed to initiailize player registry", pthread_self());
        return NULL;
    }
    pr->len = 0;
    pr->cap = MAX_CLIENTS;

    pr->preg_arr = calloc(pr->cap, sizeof(PLAYER *));
    if(pr->preg_arr == NULL) {
        free(pr);
        debug("%ld: Failed to initiailize player registry", pthread_self());
        return NULL;
    }

    debug("%ld: Initialize player registry", pthread_self());
    pthread_mutex_init(&pr->mutex, NULL);
    return pr;
}

void preg_fini(PLAYER_REGISTRY *preg) {
    if(preg == NULL) {
        return;
    }

    pthread_mutex_lock(&preg->mutex);
    for(int i = 0; i < preg->cap; ++i) {
        if(preg->preg_arr[i] != NULL) {
            player_unref(preg->preg_arr[i], "because player registry is being freed");
        }
    }
    if(preg->preg_arr != NULL) {
        free(preg->preg_arr);
        preg->preg_arr = NULL;
    }
    pthread_mutex_unlock(&preg->mutex);
    pthread_mutex_destroy(&preg->mutex);
    free(preg);
}

PLAYER *preg_register(PLAYER_REGISTRY *preg, char *name) {
    pthread_mutex_lock(&preg->mutex);
    if(preg->len >= preg->cap) {
        preg->preg_arr = reallocarray(preg->preg_arr, preg->cap * 2, sizeof(PLAYER *));

        for(int i = preg->cap; i < preg->cap * 2; ++i) {
            preg->preg_arr[i] = NULL;
        }
        preg->cap *= 2;
    }
    PLAYER *player = NULL;
    for(int i = 0; i < preg->cap; ++i) {
        if(preg->preg_arr[i] != NULL) {
            char *othername = player_get_name(preg->preg_arr[i]);
            if(strcmp(name, othername) == 0) {
                player = preg->preg_arr[i];
                break;
            }
        }
    }
    if(player != NULL) {
        pthread_mutex_unlock(&preg->mutex);
        return player_ref(player, "existing player found");
    }
    for(int i = 0; i < preg->cap; ++i) {
        if(preg->preg_arr[i] == NULL) {
            preg->preg_arr[i] = player_create(name);
            player = preg->preg_arr[i];
            player_ref(player, "because being added to player registry");
            break;
        }
    }
    preg->len++;
    pthread_mutex_unlock(&preg->mutex);
    return player;
}
