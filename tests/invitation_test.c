#include <criterion/criterion.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "debug.h"
#include "client_registry.h"
#include "invitation.h"
#include "excludes.h"

/* Maximum number of iterations performed for some tests. */
#define NITER (1000000)

/* Number of threads we create in multithreaded tests. */
#define NTHREAD (10)

Test(invitation_suite, create_same_client, .timeout = 5) {
#ifdef NO_INVITATION
    cr_assert_fail("Invitation module was not implemented");
#endif
    CLIENT_REGISTRY *cr = creg_init();
    cr_assert_not_null(cr);

    CLIENT *alice = creg_register(cr, 10);
    cr_assert_not_null(alice);

    INVITATION *inv = inv_create(alice, alice, FIRST_PLAYER_ROLE, SECOND_PLAYER_ROLE);
    cr_assert_null(inv, "Returned value was not NULL");
}

Test(invitation_suite, create_different_clients, .timeout = 5) {
#ifdef NO_INVITATION
    cr_assert_fail("Invitation module was not implemented");
#endif
    CLIENT_REGISTRY *cr = creg_init();
    cr_assert_not_null(cr);

    CLIENT *alice = creg_register(cr, 10);
    cr_assert_not_null(alice);
    GAME_ROLE alice_role = SECOND_PLAYER_ROLE;
    CLIENT *bob = creg_register(cr, 11);
    cr_assert_not_null(bob);
    GAME_ROLE bob_role = FIRST_PLAYER_ROLE;

    INVITATION *inv = inv_create(alice, bob, alice_role, bob_role);
    cr_assert_not_null(inv, "Returned value was NULL");

    // Check the accessors to see if they return the correct values.
    CLIENT *src = inv_get_source(inv);
    CLIENT *trg = inv_get_target(inv);
    GAME_ROLE src_role = inv_get_source_role(inv);
    GAME_ROLE trg_role = inv_get_target_role(inv);
    cr_assert_eq(src, alice, "Source (%p) did not match expected (%p)", src, alice);
    cr_assert_eq(trg, bob, "Target (%p) did not match expected (%p)", trg, bob);
    cr_assert_eq(src_role, alice_role, "Source role (%d) did not match expected (%d)",
		 src_role, alice_role);
    cr_assert_eq(trg_role, bob_role, "Target role (%d) did not match expected (%d)",
		 trg_role, bob_role);
}

Test(invitation_suite, create_close, .timeout = 5) {
#ifdef NO_INVITATION
    cr_assert_fail("Invitation module was not implemented");
#endif
    CLIENT_REGISTRY *cr = creg_init();
    cr_assert_not_null(cr);

    CLIENT *alice = creg_register(cr, 10);
    cr_assert_not_null(alice);
    GAME_ROLE alice_role = SECOND_PLAYER_ROLE;
    CLIENT *bob = creg_register(cr, 11);
    cr_assert_not_null(bob);
    GAME_ROLE bob_role = FIRST_PLAYER_ROLE;

    INVITATION *inv = inv_create(alice, bob, alice_role, bob_role);
    cr_assert_not_null(inv, "Returned value was NULL");

    int err = inv_close(inv, NULL_ROLE);
    cr_assert_eq(err, 0, "Returned value (%d) was not 0", err);
}

Test(invitation_suite, create_close_close, .timeout = 5) {
#ifdef NO_INVITATION
    cr_assert_fail("Invitation module was not implemented");
#endif
    CLIENT_REGISTRY *cr = creg_init();
    cr_assert_not_null(cr);

    CLIENT *alice = creg_register(cr, 10);
    cr_assert_not_null(alice);
    GAME_ROLE alice_role = SECOND_PLAYER_ROLE;
    CLIENT *bob = creg_register(cr, 11);
    cr_assert_not_null(bob);
    GAME_ROLE bob_role = FIRST_PLAYER_ROLE;

    INVITATION *inv = inv_create(alice, bob, alice_role, bob_role);
    cr_assert_not_null(inv, "Returned value was NULL");

    int err = inv_close(inv, NULL_ROLE);
    cr_assert_eq(err, 0, "Returned value (%d) was not 0", err);

    err = inv_close(inv, NULL_ROLE);
    cr_assert_eq(err, -1, "Returned value (%d) was not -1", err);
}

Test(invitation_suite, create_accept, .timeout = 5) {
#ifdef NO_INVITATION
    cr_assert_fail("Invitation module was not implemented");
#endif
    CLIENT_REGISTRY *cr = creg_init();
    cr_assert_not_null(cr);

    CLIENT *alice = creg_register(cr, 10);
    cr_assert_not_null(alice);
    GAME_ROLE alice_role = SECOND_PLAYER_ROLE;
    CLIENT *bob = creg_register(cr, 11);
    cr_assert_not_null(bob);
    GAME_ROLE bob_role = FIRST_PLAYER_ROLE;

    INVITATION *inv = inv_create(alice, bob, alice_role, bob_role);
    cr_assert_not_null(inv, "Returned value was NULL");

    int err = inv_accept(inv);
    cr_assert_eq(err, 0, "Returned value (%d) was not 0", err);

    // Check to see that a game has been created and is in progress.
    GAME *game = inv_get_game(inv);
    cr_assert_not_null(game, "No game was returned", err);
    cr_assert(!game_is_over(game), "The game should not be over when it has just been started");
}

Test(invitation_suite, create_accept_close, .timeout = 5) {
#ifdef NO_INVITATION
    cr_assert_fail("Invitation module was not implemented");
#endif
    CLIENT_REGISTRY *cr = creg_init();
    cr_assert_not_null(cr);

    CLIENT *alice = creg_register(cr, 10);
    cr_assert_not_null(alice);
    GAME_ROLE alice_role = SECOND_PLAYER_ROLE;
    CLIENT *bob = creg_register(cr, 11);
    cr_assert_not_null(bob);
    GAME_ROLE bob_role = FIRST_PLAYER_ROLE;

    INVITATION *inv = inv_create(alice, bob, alice_role, bob_role);
    cr_assert_not_null(inv, "Returned value was NULL");

    int err = inv_accept(inv);
    cr_assert_eq(err, 0, "Returned value (%d) was not 0", err);

    err = inv_close(inv, NULL_ROLE);
    cr_assert_eq(err, -1, "Returned value (%d) was not -1", err);
}

Test(invitation_suite, create_accept_resign, .timeout = 5) {
#ifdef NO_INVITATION
    cr_assert_fail("Invitation module was not implemented");
#endif
    CLIENT_REGISTRY *cr = creg_init();
    cr_assert_not_null(cr);

    CLIENT *alice = creg_register(cr, 10);
    cr_assert_not_null(alice);
    GAME_ROLE alice_role = SECOND_PLAYER_ROLE;
    CLIENT *bob = creg_register(cr, 11);
    cr_assert_not_null(bob);
    GAME_ROLE bob_role = FIRST_PLAYER_ROLE;

    INVITATION *inv = inv_create(alice, bob, alice_role, bob_role);
    cr_assert_not_null(inv, "Returned value was NULL");

    int err = inv_accept(inv);
    cr_assert_eq(err, 0, "Returned value (%d) was not 0", err);

    err = inv_close(inv, SECOND_PLAYER_ROLE);
    cr_assert_eq(err, 0, "Returned value (%d) was not 0", err);

    // Check that the game has been resigned.
    GAME *game = inv_get_game(inv);
    cr_assert_not_null(game, "No game was returned", err);
    cr_assert(game_is_over(game), "The game should be over after a player resigns");
}

/*
 * Concurrency test: An invitation is created and given to a number of threads.
 * The threads enter a loop in which they attempt to resign the invitation
 * (i.e. close it under a role other than NULL_ROLE).  All such attempts should
 * return an error, because the invitation has not yet been accepted.
 * The main thread will accept the invitation after some delay, after which exactly
 * one thread should succeed in resigning the invitation.
 */

static int successful_resignations;
static pthread_mutex_t successful_resignations_lock;

void *resign_thread(void *arg) {
    INVITATION *inv = arg;
    long n = NITER;
    while(n) {
	int err = inv_close(inv, FIRST_PLAYER_ROLE);
	pthread_mutex_lock(&successful_resignations_lock);
	if(successful_resignations)
	    n--;  // Limit iterations after first success.
	if(!err) {
	    successful_resignations++;
	    pthread_mutex_unlock(&successful_resignations_lock);
	    return NULL;
	} 
	pthread_mutex_unlock(&successful_resignations_lock);
    }
    return NULL;
}

Test(invitation_suite, concurrent_resign, .timeout = 15) {
#ifdef NO_INVITATION
    cr_assert_fail("Invitation module was not implemented");
#endif
    CLIENT_REGISTRY *cr = creg_init();
    cr_assert_not_null(cr);

    CLIENT *alice = creg_register(cr, 10);
    cr_assert_not_null(alice);
    GAME_ROLE alice_role = SECOND_PLAYER_ROLE;
    CLIENT *bob = creg_register(cr, 11);
    cr_assert_not_null(bob);
    GAME_ROLE bob_role = FIRST_PLAYER_ROLE;

    INVITATION *inv = inv_create(alice, bob, alice_role, bob_role);
    cr_assert_not_null(inv, "Returned value was NULL");

    // Spawn threads to perform concurrent resignation attempts.
    pthread_mutex_init(&successful_resignations_lock, NULL);
    pthread_t tid[NTHREAD];
    for(int i = 0; i < NTHREAD; i++)
	pthread_create(&tid[i], NULL, resign_thread, inv);

    // Accept the invitation, after a delay to ensure all the threads are running.
    sleep(1);
    inv_accept(inv);

    // Wait for all threads to finish.
    for(int i = 0; i < NTHREAD; i++)
	pthread_join(tid[i], NULL);

    // The number of threads that saw a successful resignation should
    // be exactly one.
    cr_assert_eq(successful_resignations, 1, "The number of successful resignations (%d) was not 1",
		 successful_resignations);
}
