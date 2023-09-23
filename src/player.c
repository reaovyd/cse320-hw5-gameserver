#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <math.h>

#include "player.h"
#include "debug.h"

static int player_id = 0; /* need this for some kind of hierarchy on mutex locks */

struct player {
    pthread_mutex_t rating_mutex;
    double rating;
    int id;

    char *name;
    pthread_mutex_t mutex;
    size_t ref_count;
};

PLAYER* player_create(char *name) {
    if(name == NULL) {
        return NULL;
    }
    PLAYER *p = malloc(sizeof(PLAYER));
    if(p == NULL) {
        debug("%ld: Player failed to initialize", pthread_self());
        return NULL;
    }
    p->rating = PLAYER_INITIAL_RATING;
    p->name = strdup(name);
    p->ref_count = 0;
    p->id = player_id++;

    pthread_mutex_init(&p->rating_mutex, NULL);
    pthread_mutex_init(&p->mutex, NULL);

    player_ref(p, "for newly created player");

    return p;
}

PLAYER *player_ref(PLAYER *player, char *why) {
    if(player == NULL) {
        debug("%ld: Invalid player object!", pthread_self());
        return NULL;
    }

    pthread_mutex_lock(&player->mutex);
    size_t old_ref = player->ref_count;
    player->ref_count = old_ref + 1;

    debug("%ld: Increase reference count on player %p (%lu -> %lu) %s", pthread_self(), player, old_ref, player->ref_count, why); 
    
    pthread_mutex_unlock(&player->mutex);
    return player;
}


void player_unref(PLAYER *player, char *why) {
    if(player == NULL) {
        debug("%ld: Invalid player object!", pthread_self());
        return;
    }

    pthread_mutex_lock(&player->mutex);
    size_t old_ref = player->ref_count;
    player->ref_count = old_ref - 1;

    debug("%ld: Decrease reference count on player %p (%lu -> %lu) %s", pthread_self(), player, old_ref, player->ref_count, why); 

    if(player->ref_count == 0) {
        free(player->name);
        pthread_mutex_unlock(&player->mutex);
        pthread_mutex_destroy(&player->mutex);

        pthread_mutex_destroy(&player->rating_mutex);
        free(player);
        debug("%ld: Free player %p", pthread_self(), player);
        return;
    }
    pthread_mutex_unlock(&player->mutex);
}

char *player_get_name(PLAYER *player) {
    return player->name;
}

int player_get_rating(PLAYER *player) {
    pthread_mutex_lock(&player->rating_mutex);
    int rating = (int)round(player->rating);
    pthread_mutex_unlock(&player->rating_mutex);

    return rating;
}

// the 1 2 2 1 deadlock
void player_post_result(PLAYER *player1, PLAYER *player2, int result) {
    if(player1 == NULL || player2 == NULL) {
        return;
    }
    if(player1->id < player2->id) {
        pthread_mutex_lock(&player1->rating_mutex);
        pthread_mutex_lock(&player2->rating_mutex);
    } else {
        pthread_mutex_lock(&player2->rating_mutex);
        pthread_mutex_lock(&player1->rating_mutex);
    }

    double s1 = result == 0 ? 0.5 : result == 1 ? 1 : 0;
    double s2 = result == 0 ? 0.5 : result == 2 ? 1 : 0;

    double r1 = player1->rating;
    double r2 = player2->rating;

    double e1 = 1.0 / (1.0 + (pow(10.0, (r2 - r1) / 400.0))); 
    double e2 = 1.0 / (1.0 + (pow(10.0, (r1 - r2) / 400.0)));

    double player1_new_rating = r1 + 32 * (s1 - e1); 
    double player2_new_rating = r2 + 32 * (s2 - e2); 

    player1->rating = player1_new_rating;
    player2->rating = player2_new_rating;
    
    pthread_mutex_unlock(&player1->rating_mutex);
    pthread_mutex_unlock(&player2->rating_mutex);
}
