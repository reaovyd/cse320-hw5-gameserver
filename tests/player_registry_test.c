#include <criterion/criterion.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "debug.h"
#include "player_registry.h"
#include "excludes.h"

/* Number of threads we create in multithreaded tests. */
#define NTHREAD (10)

/* Maximum number of players we register in long tests. */
#define NPLAYERS (1000)

/*
 * Register one player, then register again and check that the same PLAYER
 * object is returned.
 */
Test(player_registry_suite, one_registry_one_player, .timeout = 5) {
#ifdef NO_PLAYER_REGISTRY
    cr_assert_fail("Player registry was not implemented");
#endif
    char *user = "Jane";
    PLAYER_REGISTRY *pr = preg_init();
    cr_assert_not_null(pr);

    PLAYER *player = preg_register(pr, user);
    cr_assert_neq(player, NULL, "Returned value was NULL");
    PLAYER *p = preg_register(pr, user);
    cr_assert_eq(p, player, "Returned player (%p) was not the same as expected (%p)",
		 p, player);
}

/*
 * Register three players, then register one again and check that the same PLAYER
 * object is returned.
 */
Test(player_registry_suite, one_registry_three_players, .timeout = 5) {
#ifdef NO_PLAYER_REGISTRY
    cr_assert_fail("Player registry was not implemented");
#endif
    char *user1 = "Alice";
    char *user2 = "Bob";
    char *user3 = "Carol";
    PLAYER_REGISTRY *pr = preg_init();
    cr_assert_not_null(pr);

    PLAYER *player1 = preg_register(pr, user1);
    cr_assert_neq(player1, NULL, "Returned value was NULL");
    PLAYER *player2 = preg_register(pr, user2);
    cr_assert_neq(player2, NULL, "Returned value was NULL");
    PLAYER *player3 = preg_register(pr, user3);
    cr_assert_neq(player3, NULL, "Returned value was NULL");

    PLAYER *p = preg_register(pr, user2);
    cr_assert_eq(p, player2, "Returned player (%p) was not the same as expected (%p)",
		 p, player2);
}

/*
 * Create two registries, register different players in each, and check that
 * each player is found in the appropriate registry and not in the other.
 */
Test(player_registry_suite, two_registries_two_players, .timeout = 5) {
#ifdef NO_PLAYER_REGISTRY
    cr_assert_fail("Player registry was not implemented");
#endif
    char *user1 = "Alice";
    char *user2 = "Bob";
    PLAYER_REGISTRY *pr1 = preg_init();
    cr_assert_not_null(pr1);
    PLAYER_REGISTRY *pr2 = preg_init();
    cr_assert_not_null(pr2);

    PLAYER *player1 = preg_register(pr1, user1);
    cr_assert_neq(player1, NULL, "Returned value was NULL");
    PLAYER *player2 = preg_register(pr2, user2);
    cr_assert_neq(player2, NULL, "Returned value was NULL");

    PLAYER *p1 = preg_register(pr1, user1);
    cr_assert_eq(p1, player1, "Returned player (%p) was not the same as expected (%p)",
		 p1, player1);
    PLAYER *p2 = preg_register(pr2, user2);
    cr_assert_eq(p2, player2, "Returned player (%p) was not the same as expected (%p)",
		 p2, player2);
    p1 = preg_register(pr2, user1);
    cr_assert_neq(p1, player1, "Returned player (%p) should not be (%p)", p1, player1);
    p2 = preg_register(pr1, user2);
    cr_assert_neq(p2, player2, "Returned player (%p) should not be (%p)", p2, player2);
}

/*
 * Set of player objects that have been registered and a lock to protect it.
 */
static PLAYER *players[NPLAYERS];
static pthread_mutex_t players_lock;

/*
 * Randomly choose an index in the players array and register a player under
 * a corresponding name.  If the index was NULL, store the player in the array,
 * otherwise check that the correct existing PLAYER was returned.
 */
struct random_reg_args {
    PLAYER_REGISTRY *pr;
    int iters;
};

void *random_reg_thread(void *arg) {
    struct random_reg_args *ap = arg;
    unsigned int seed = 1;
    char name[32];
    pthread_mutex_init(&players_lock, NULL);
    for(int i = 0; i < ap->iters; i++) {
	int n = rand_r(&seed) % NPLAYERS;
	sprintf(name, "p%d", n);
	PLAYER *player = preg_register(ap->pr, name);
	pthread_mutex_lock(&players_lock);
	if(players[n] == NULL) {
	    players[n] = player;
	} else {
	    cr_assert_eq(player, players[n], "Returned player (%p) did not match expected (%p)",
			 player, players[n]);
	}
	pthread_mutex_unlock(&players_lock);
    }
    return NULL;
}

Test(player_registry_suite, many_threads_one_registry, .timeout = 15) {
#ifdef NO_PLAYER_REGISTRY
    cr_assert_fail("Player registry was not implemented");
#endif
    PLAYER_REGISTRY *pr = preg_init();
    cr_assert_not_null(pr);

    // Spawn threads to perform concurrent registrations.
    pthread_t tid[NTHREAD];
    struct random_reg_args *ap = calloc(1, sizeof(struct random_reg_args));
    ap->pr = pr;
    ap->iters = 10 * NPLAYERS;
    for(int i = 0; i < NTHREAD; i++)
	pthread_create(&tid[i], NULL, random_reg_thread, ap);

    // Wait for all threads to finish.
    for(int i = 0; i < NTHREAD; i++)
	pthread_join(tid[i], NULL);
}
