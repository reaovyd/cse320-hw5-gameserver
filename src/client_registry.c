#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/socket.h>

#include "client_registry.h"
#include "jeux_globals.h"
#include "debug.h"

struct client_registry {
    CLIENT *creg_arr[MAX_CLIENTS];
    pthread_mutex_t mutex;
    sem_t count_sem;
    volatile size_t len;
    size_t cap;
};

CLIENT_REGISTRY *creg_init() {
    CLIENT_REGISTRY *new_reg = malloc(sizeof(CLIENT_REGISTRY));
    if(new_reg == NULL) {
        return NULL;
    }
    memset(new_reg->creg_arr, 0x0, sizeof(new_reg->creg_arr[0]) * MAX_CLIENTS);
    new_reg->len = 0;
    new_reg->cap = MAX_CLIENTS;

    pthread_mutex_init(&new_reg->mutex, NULL);
    sem_init(&new_reg->count_sem, 0, 1);

    debug("%ld: Initialize client registry", pthread_self());
    return new_reg;
}

// TODO massive race condition here when attempting to terminate i think
void creg_fini(CLIENT_REGISTRY *cr) {
    if(cr == NULL) {
        return;
    }
    sem_destroy(&cr->count_sem);
    pthread_mutex_destroy(&cr->mutex);
    free(cr);
}

CLIENT *creg_register(CLIENT_REGISTRY *cr, int fd) {
    pthread_mutex_lock(&cr->mutex);

    const size_t cap_sz = cr->cap;
    size_t idx = -1;
    for(size_t reg_idx = 0; reg_idx < cap_sz; ++reg_idx) {
        if(cr->creg_arr[reg_idx] == NULL) {
            idx = reg_idx;
            break;
        }
    }
    if(idx == -1) {
        pthread_mutex_unlock(&cr->mutex);
        return NULL;
    }
    cr->creg_arr[idx] = client_create(cr, fd);
    if(cr->creg_arr[idx] == NULL) {
        pthread_mutex_unlock(&cr->mutex);
        return NULL;
    }
    if(cr->len == 0) {
        sem_wait(&cr->count_sem);
    }
    ++cr->len;
    debug("Register client fd %d (total connected: %lu)", fd, cr->len);
    pthread_mutex_unlock(&cr->mutex);
    return cr->creg_arr[idx];
}

int creg_unregister(CLIENT_REGISTRY *cr, CLIENT *client) {
    pthread_mutex_lock(&cr->mutex);
    const size_t cap_sz = cr->cap;
    size_t idx = -1;
    for(size_t reg_idx = 0; reg_idx < cap_sz; ++reg_idx) {
        if(cr->creg_arr[reg_idx] == client) {
            idx = reg_idx;
            break;
        }
    }
    if(idx == -1) {
        pthread_mutex_unlock(&cr->mutex);
        return -1;
    }

    int fd = client_get_fd(client);
    close(fd);
    client_unref(cr->creg_arr[idx], "unregistered");
    cr->creg_arr[idx] = NULL;
    --cr->len;
    if(cr->len == 0) {
        sem_post(&cr->count_sem);
    }
    debug("Unregister client fd %d (total connected: %lu)", fd, cr->len);

    pthread_mutex_unlock(&cr->mutex);
    return 0;
}

CLIENT *creg_lookup(CLIENT_REGISTRY *cr, char *user) {
    pthread_mutex_lock(&cr->mutex);

    size_t cap_sz = cr->cap;
    CLIENT *ret = NULL;

    for(size_t idx = 0; idx < cap_sz; idx++) {
        if(cr->creg_arr[idx] != NULL) {
            PLAYER *player = client_get_player(cr->creg_arr[idx]);
            if(player == NULL) {
                continue;
            } else {
                char *player_name = player_get_name(player);
                if(strcmp(player_name, user) == 0) {
                    ret = cr->creg_arr[idx];
                    client_ref(cr->creg_arr[idx], "for reference being returned by creg_lookup()");
                    break;
                }
            }
        }
    }
    pthread_mutex_unlock(&cr->mutex);
    return ret;
}

PLAYER **creg_all_players(CLIENT_REGISTRY *cr) {
    pthread_mutex_lock(&cr->mutex);

    PLAYER **p = NULL;
    size_t cap_sz = cr->cap;
    size_t len = 0;
    for(size_t idx = 0; idx < cap_sz; idx++) {
        if(cr->creg_arr[idx] != NULL) {
            if(client_get_player(cr->creg_arr[idx]) != NULL) {
                len++;
            }
        }
    }
    p = calloc(len + 1, sizeof(PLAYER *));
    len = 0;

    for(size_t idx = 0; idx < cap_sz; idx++) {
        if(cr->creg_arr[idx] != NULL) {
            PLAYER *player = client_get_player(cr->creg_arr[idx]);
            if(player != NULL) {
                p[len++] = player_ref(player, "for reference being added to player's list");
            }
        }
    }
    pthread_mutex_unlock(&cr->mutex);
    return p;
}

void creg_wait_for_empty(CLIENT_REGISTRY *cr) { 
    if(cr == NULL) {
        return;
    }
    sem_wait(&cr->count_sem);
    /* necessary if potentially other threads call this (e.g. creg_register)*/
    sem_post(&cr->count_sem); 
}

void creg_shutdown_all(CLIENT_REGISTRY *cr) {
    if(cr == NULL) {
        return;
    }
    pthread_mutex_lock(&cr->mutex);
    size_t cap_sz = cr->cap;
    for(size_t idx = 0; idx < cap_sz; idx++) {
        if(cr->creg_arr[idx] != NULL) {
            int fd = client_get_fd(cr->creg_arr[idx]);
            debug("%ld: Shutting down fd %d", pthread_self(), fd);
            shutdown(fd, SHUT_RD);
        }
    }
    // TODO might potentially need some kind of flag or signal or smth 
    // to ensure creg_registers don't actually run
    pthread_mutex_unlock(&cr->mutex);
}
