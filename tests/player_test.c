#include <criterion/criterion.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "debug.h"
#include "client_registry.h"
#include "player.h"
#include "excludes.h"

/* Maximum number of iterations performed for some tests. */
#define NITER (1000000)

/* Number of threads we create in multithreaded tests. */
#define NTHREAD (10)

/* Number of players we create. */
#define NPLAYER (100)

Test(player_suite, create, .timeout = 5) {
#ifdef NO_PLAYER
    cr_assert_fail("Player module was not implemented");
#endif
    char *name = "Alice";
    PLAYER *player = player_create(name);
    cr_assert_not_null(player, "Returned value was NULL");

    char *pn = player_get_name(player);
    cr_assert(!strcmp(pn, name), "Player name (%s) does not match expected (%s)",
	      pn, name);
    int r = player_get_rating(player);
    cr_assert_eq(r, PLAYER_INITIAL_RATING, "Player rating (%d) does not match expected (%d)",
		 r, PLAYER_INITIAL_RATING);
}

Test(player_suite, post_result_draw, .timeout = 5) {
#ifdef NO_PLAYER
    cr_assert_fail("Player module was not implemented");
#endif
    char *alice = "Alice";
    PLAYER *player_alice = player_create(alice);
    cr_assert_not_null(player_alice, "Returned value was NULL");
    char *bob = "Bob";
    PLAYER *player_bob = player_create(bob);
    cr_assert_not_null(player_bob, "Returned value was NULL");

    player_post_result(player_alice, player_bob, 0);

    // The player's rating should be unchanged.
    int r = player_get_rating(player_alice);
    cr_assert_eq(r, PLAYER_INITIAL_RATING, "Player rating (%d) does not match expected (%d)",
		 r, PLAYER_INITIAL_RATING);
    r = player_get_rating(player_bob);
    cr_assert_eq(r, PLAYER_INITIAL_RATING, "Player rating (%d) does not match expected (%d)",
		 r, PLAYER_INITIAL_RATING);
}

Test(player_suite, post_result_first, .timeout = 5) {
#ifdef NO_PLAYER
    cr_assert_fail("Player module was not implemented");
#endif
    char *alice = "Alice";
    PLAYER *player_alice = player_create(alice);
    cr_assert_not_null(player_alice, "Returned value was NULL");
    char *bob = "Bob";
    PLAYER *player_bob = player_create(bob);
    cr_assert_not_null(player_bob, "Returned value was NULL");

    player_post_result(player_alice, player_bob, 1);

    int r = player_get_rating(player_alice);
    cr_assert_eq(r, 1516, "Player rating (%d) does not match expected (%d)",
		 r, 1516);
    r = player_get_rating(player_bob);
    cr_assert_eq(r, 1484, "Player rating (%d) does not match expected (%d)",
		 r, 1484);
}

Test(player_suite, post_result_series, .timeout = 5) {
#ifdef NO_PLAYER
    cr_assert_fail("Player module was not implemented");
#endif
    char *alice = "Alice";
    PLAYER *player_alice = player_create(alice);
    cr_assert_not_null(player_alice, "Returned value was NULL");
    char *bob = "Bob";
    PLAYER *player_bob = player_create(bob);
    cr_assert_not_null(player_bob, "Returned value was NULL");
    char *carol = "Carol";
    PLAYER *player_carol = player_create(carol);
    cr_assert_not_null(player_carol, "Returned value was NULL");
    char *dan = "Dan";
    PLAYER *player_dan = player_create(dan);
    cr_assert_not_null(player_dan, "Returned value was NULL");

    player_post_result(player_alice, player_carol, 1);
    player_post_result(player_alice, player_bob, 2);
    player_post_result(player_bob, player_dan, 0);
    player_post_result(player_carol, player_dan, 1);
    player_post_result(player_alice, player_dan, 1);

    int r = player_get_rating(player_alice);
    cr_assert_eq(r, 1515, "Player rating (%d) does not match expected (%d)",
		 r, 1515);
    r = player_get_rating(player_bob);
    cr_assert_eq(r, 1516, "Player rating (%d) does not match expected (%d)",
		 r, 1516);
    r = player_get_rating(player_carol);
    cr_assert_eq(r, 1500, "Player rating (%d) does not match expected (%d)",
		 r, 1500);
    r = player_get_rating(player_dan);
    cr_assert_eq(r, 1469, "Player rating (%d) does not match expected (%d)",
		 r, 1469);
}

/*
 * Concurrency test: Create a number of players, then create a number of threads
 * to post random game results.  Once the posting of results has finished, check
 * the sum of the player's ratings to see if rating points have been conserved.
 * This test checks for deadlock (will result in timeout), rating points conservation,
 * and thread-safety of player_post_result.
 */

static PLAYER *players[NPLAYER];

void *post_thread(void *arg) {
    unsigned int seed = 1;
    for(int i = 0; i < NITER; i++) {
	PLAYER *player1 = players[rand_r(&seed) % NPLAYER];
	PLAYER *player2 = players[rand_r(&seed) % NPLAYER];
	if(player1 == player2)
	    continue;
	int result = rand_r(&seed) % 3;
	player_post_result(player1, player2, result);
    }
    return NULL;
}

Test(player_suite, concurrent_post, .timeout = 15) {
#ifdef NO_PLAYER
    cr_assert_fail("Player module was not implemented");
#endif
    char name[32];
    for(int i = 0; i < NPLAYER; i++) {
	sprintf(name, "p%d", i);
	players[i] = player_create(name);
	cr_assert(players[i] != NULL, "Player creation failed");
    }

    pthread_t tid[NTHREAD];
    for(int i = 0; i < NTHREAD; i++)
	pthread_create(&tid[i], NULL, post_thread, NULL);

    // Wait for all threads to finish.
    for(int i = 0; i < NTHREAD; i++)
	pthread_join(tid[i], NULL);

    // Compute the sum of the player ratings and check it.
    int sum = 0;
    for(int i = 0; i < NPLAYER; i++)
	sum += player_get_rating(players[i]);
    cr_assert_eq(sum, NPLAYER * PLAYER_INITIAL_RATING,
		 "The sum of player ratings (%d) did not match the expected value (%d)",
		 sum, NPLAYER * PLAYER_INITIAL_RATING);
}
