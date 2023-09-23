#include <criterion/criterion.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "debug.h"
#include "client_registry.h"
#include "player_registry.h"
#include "jeux_globals.h"
#include "excludes.h"

/* The maximum number of "file descriptors" we will use. */
#define NFD (64)

/* The maximum number of clients we will register. */
#define NCLIENT (64)

/* Number of threads we create in multithreaded tests. */
#define NTHREAD (10)

/* Number of iterations we use in several tests. */
#define NITER (1000000)

/*
 * Shared pool of "file descriptors".  Simulates file descriptors
 * that the system might have assigned to client threads.
 */
static int fdpool[NFD];
static pthread_mutex_t fdpool_lock;

/*
 * Client objects that have been registered.
 * These are needed in order to unregister.
 */
static CLIENT *clients[NFD];

/*
 * Get an unassigned "file descriptor", assign it to a particular
 * client ID, and return the index.
 */
static int getfd(int cid) {
    pthread_mutex_lock(&fdpool_lock);
    // Don't use possibly valid fds, because people try to close them.
    for(int i = 20; i < NFD; i++) {
	if(fdpool[i] == -1) {
	    fdpool[i] = cid;
	    pthread_mutex_unlock(&fdpool_lock);
	    return i;
	}
    }
    pthread_mutex_unlock(&fdpool_lock);
    return -1;
}

/*
 * Release a specified "file descriptor" if assigned to a particular
 * client ID.  If it is not assigned to that client ID, do nothing.
 */
static void relfd(int fd, int cid) {
    pthread_mutex_lock(&fdpool_lock);
    if(fdpool[fd] == cid) {
	fdpool[fd] = -1;
	clients[fd] = NULL;
    }
    pthread_mutex_unlock(&fdpool_lock);
}

static void init() {
    pthread_mutex_init(&fdpool_lock, NULL);
    for(int i = 0; i < NFD; i++)
	fdpool[i] = -1;
}

#define CLIENT_UP(cr, fds, cid) \
   do { \
     int fd = getfd(cid); \
     if(fd != -1) { \
       fds[cid] = fd; \
       clients[fd] = creg_register(cr, fd); \
     } else { \
       fds[cid] = -1; \
     } \
     cid++; \
   } while(0)

#define CLIENT_DOWN(cr, fds, cid) \
   do { \
     cid--; \
     int fd = fds[cid]; \
     if(fd != -1) { \
       creg_unregister(cr, clients[fd]); \
       relfd(fd, cid); \
     } \
   } while(0)

/*
 * Randomly register and unregister clients, then unregister
 * all remaining registered at the end.
 */
void random_reg_unreg(CLIENT_REGISTRY *cr, int n) {
    int cid = 0;
    unsigned int seed = 1; //pthread_self();
    // Array mapping client IDs to file descriptors.
    int fds[NCLIENT];
    for(int i = 0; i < NCLIENT; i++)
	fds[i] = -1;
    for(int i = 0; i < n; i++) {
	if(cid == 0) {
	    // No clients: only way to go is up!
	    CLIENT_UP(cr, fds, cid);
	} else if(cid == NCLIENT) {
	    // Clients maxxed out: only way to go is down!
	    CLIENT_DOWN(cr, fds, cid);
	} else {
	    if(rand_r(&seed) % 2) {
		CLIENT_UP(cr, fds, cid);
	    } else {
		CLIENT_DOWN(cr, fds, cid);
	    }
	}
    }
    // Unregister any remaining file descriptors at the end.
    while(cid > 0)
	CLIENT_DOWN(cr, fds, cid);
}

/*
 * Thread that calls wait_for_empty on a client registry, then checks a set of flags.
 * If all flags are nonzero, the test succeeds, otherwise return from wait_for_empty
 * was premature and the test fails.
 */
struct wait_for_empty_args {
    CLIENT_REGISTRY *cr;
    volatile int *flags;
    int nflags;
    int ret;
};

void *wait_for_empty_thread(void *arg) {
    struct wait_for_empty_args *ap = arg;
    creg_wait_for_empty(ap->cr);
    ap->ret = 1;
    for(int i = 0; i < ap->nflags; i++) {
	if(ap->flags[i] == 0) {
	    ap->ret = 0;
	}
    }
    return NULL;
}

/*
 * Thread that runs random register/unregister, then sets a flag.
 * The thread delays at the start of the test, to make it more likely
 * that other threads started at about the same time are active.
 */
struct random_reg_unreg_args {
    CLIENT_REGISTRY *cr;
    volatile int *done_flag;
    int iters;
    int start_delay;
};

void *random_reg_unreg_thread(void *arg) {
    struct random_reg_unreg_args *ap = arg;
    if(ap->start_delay)
	sleep(ap->start_delay);
    random_reg_unreg(ap->cr, ap->iters);
    if(ap->done_flag != NULL)
	*ap->done_flag = 1;
    return NULL;
}

/*
 * Test one registry, one thread doing random register/unregister,
 * and that thread calling creg_wait_for_empty does not block forever.
 */
Test(client_registry_suite, basic_one_registry, .init = init, .timeout = 5) {
#ifdef NO_CLIENT_REGISTRY
    cr_assert_fail("Client registry was not implemented");
#endif
    // Initialize a thread counter, randomly register and unregister,
    // ending with nothing registered, then call wait_for_zero.
    CLIENT_REGISTRY *cr = creg_init();
    cr_assert_not_null(cr);

    // Spawn a thread to run random increment/decrement.
    pthread_t tid;
    struct random_reg_unreg_args *ap = calloc(1, sizeof(struct random_reg_unreg_args));
    ap->cr = cr;
    ap->iters = 100;
    pthread_create(&tid, NULL, random_reg_unreg_thread, ap);

    // Wait for the increment/decrement to complete.
    pthread_join(tid, NULL);

    // Call wait_for_zero -- should not time out.
    creg_wait_for_empty(cr);
    cr_assert(1, "Timed out waiting for zero");
}

/*
 * Test two registries, two threads doing random increment/decrement,
 * and that thread calling creg_wait_for_empty does not block forever.
 */
Test(client_registry_suite, basic_two_registries, .init = init, .timeout = 5) {
#ifdef NO_CLIENT_REGISTRY
    cr_assert_fail("Client registry was not implemented");
#endif
    // Do the same test with two registries and two threads
    CLIENT_REGISTRY *cr1 = creg_init();
    cr_assert_not_null(cr1);
    CLIENT_REGISTRY *cr2 = creg_init();
    cr_assert_not_null(cr2);

    // Spawn a thread to run random register/unregister.
    pthread_t tid1;
    struct random_reg_unreg_args *ap1 = calloc(1, sizeof(struct random_reg_unreg_args));
    ap1->cr = cr1;
    ap1->iters = NITER;
    pthread_create(&tid1, NULL, random_reg_unreg_thread, ap1);

    // Spawn a thread to run random increment/decrement.
    pthread_t tid2;
    struct random_reg_unreg_args *ap2 = calloc(1, sizeof(struct random_reg_unreg_args));
    ap2->cr = cr2;
    ap2->iters = NITER;
    pthread_create(&tid2, NULL, random_reg_unreg_thread, ap2);

    // Wait for both threads to finish.
    pthread_join(tid1, NULL);
    pthread_join(tid2, NULL);

    // Call wait_for_empty -- should not time out.
    creg_wait_for_empty(cr1);
    creg_wait_for_empty(cr2);
    cr_assert(1);
}

/*
 * Test one registry, one thread doing random register/unregister,
 * check that thread calling creg_wait_for_empty does not return prematurely.
 */
Test(client_registry_suite, basic_one_registry_premature, .init = init, .timeout = 5) {
#ifdef NO_CLIENT_REGISTRY
    cr_assert_fail("Client registry was not implemented");
#endif
    // Initialize a client registry, randomly register and deregister,
    // then call wait_for_empty.
    CLIENT_REGISTRY *cr = creg_init();
    cr_assert_not_null(cr);

    // Register a client to temporarily prevent an empty situation.
    int fd = getfd(NCLIENT+1);  // Client ID out-of-range of random register/unregister.
    CLIENT *client = creg_register(cr, fd);

    // Create a flag to be set when random increment/decrement is finished.
    // This probably should be done more properly.
    volatile int flags[1] = { 0 };

    // Spawn a thread to wait for empty and then check the flag.
    pthread_t tid1;
    struct wait_for_empty_args *ap1 = calloc(1, sizeof(struct wait_for_empty_args));
    ap1->cr = cr;
    ap1->flags = flags;
    ap1->nflags = 1;
    pthread_create(&tid1, NULL, wait_for_empty_thread, ap1);

    // Spawn a thread to run a long random register/unregister test and set flag.
    pthread_t tid2;
    struct random_reg_unreg_args *ap2 = calloc(1, sizeof(struct random_reg_unreg_args));
    ap2->cr = cr;
    ap2->iters = NITER;
    ap2->done_flag = &flags[0];
    pthread_create(&tid2, NULL, random_reg_unreg_thread, ap2);

    // Wait for the increment/decrement to complete, then release the thread counter.
    pthread_join(tid2, NULL);
    creg_unregister(cr, client);

    // Get the result from the waiting thread, to see if it returned prematurely.
    pthread_join(tid1, NULL);

    // Assert that the flag was set when the wait was finished.
    cr_assert(ap1->ret, "Premature return from creg_wait_for_empty");
}

Test(client_registry_suite, many_threads_one_registry, .init = init, .timeout = 15) {
#ifdef NO_CLIENT_REGISTRY
    cr_assert_fail("Client registry was not implemented");
#endif
    CLIENT_REGISTRY *cr = creg_init();
    cr_assert_not_null(cr);

    // Spawn threads to run random increment/decrement.
    pthread_t tid[NTHREAD];
    for(int i = 0; i < NTHREAD; i++) {
	struct random_reg_unreg_args *ap = calloc(1, sizeof(struct random_reg_unreg_args));
	ap->cr = cr;
	ap->iters = NITER;
	pthread_create(&tid[i], NULL, random_reg_unreg_thread, ap);
    }

    // Wait for all threads to finish.
    for(int i = 0; i < NTHREAD; i++)
	pthread_join(tid[i], NULL);

    // Call wait_for_empty -- should not time out.
    creg_wait_for_empty(cr);
    cr_assert(1);
}

Test(client_registry_suite, many_threads_one_registry_premature, .init = init, .timeout = 15) {
#ifdef NO_CLIENT_REGISTRY
    cr_assert_fail("Client registry was not implemented");
#endif
    CLIENT_REGISTRY *cr = creg_init();
    cr_assert_not_null(cr);

    // Register a client to temporarily prevent an empty situation.
    int fd = getfd(NCLIENT+1);  // Client ID out-of-range of random register/unregister.
    CLIENT *client = creg_register(cr, fd);
 
    // Create flags to be set when random increment/decrement is finished.
    // This probably should be done more properly.
    volatile int flags[NTHREAD] = { 0 };

    // Spawn a thread to wait for empty and then check all the flags.
    pthread_t tid1;
    struct wait_for_empty_args *ap1 = calloc(1, sizeof(struct wait_for_empty_args));
    ap1->cr = cr;
    ap1->flags = flags;
    ap1->nflags = NTHREAD;
    pthread_create(&tid1, NULL, wait_for_empty_thread, ap1);

    // Spawn threads to run random register/unregister.
    pthread_t tid[NTHREAD];
    for(int i = 0; i < NTHREAD; i++) {
	struct random_reg_unreg_args *ap = calloc(1, sizeof(struct random_reg_unreg_args));
	ap->cr = cr;
	ap->iters = NITER;
	ap->done_flag = &flags[i];
	pthread_create(&tid[i], NULL, random_reg_unreg_thread, ap);
    }

    // Wait for all threads to finish, then release the thread counter.
    for(int i = 0; i < NTHREAD; i++)
	pthread_join(tid[i], NULL);
    creg_unregister(cr, client);

    // Get the result from the waiting thread, to see if it returned prematurely.
    pthread_join(tid1, NULL);

    // Assert that the flags were all set when the wait was finished.
    cr_assert(ap1->ret, "Premature return from creg_wait_for_empty");
}

Test(client_registry_suite, lookup_null, .init = init, .timeout = 15) {
#ifdef NO_CLIENT_REGISTRY
    cr_assert_fail("Client registry was not implemented");
#endif
    CLIENT_REGISTRY *cr = creg_init();
    cr_assert_not_null(cr);

    CLIENT *client = creg_lookup(cr, "Bob");
    cr_assert_eq(client, NULL, "Returned value (%p) was not NULL", client);
}

Test(client_registry_suite, register_login_lookup_null, .init = init, .timeout = 15) {
#ifdef NO_CLIENT_REGISTRY
    cr_assert_fail("Client registry was not implemented");
#endif
    CLIENT_REGISTRY *cr = client_registry = creg_init();
    cr_assert_not_null(cr);
    PLAYER_REGISTRY *pr = player_registry = preg_init();
    cr_assert_not_null(pr);

    CLIENT *alice_client = creg_register(cr, 10);
    PLAYER *alice_player = preg_register(pr, "Alice");
    int err = client_login(alice_client, alice_player);
    cr_assert_eq(err, 0, "Error logging in client");

    CLIENT *client = creg_lookup(cr, "Bob");
    cr_assert_eq(client, NULL, "Returned value (%p) was not NULL", client);
}

Test(client_registry_suite, register_login_lookup_not_null, .init = init, .timeout = 15) {
#ifdef NO_CLIENT_REGISTRY
    cr_assert_fail("Client registry was not implemented");
#endif
    CLIENT_REGISTRY *cr = client_registry = creg_init();
    cr_assert_not_null(cr);
    PLAYER_REGISTRY *pr = player_registry = preg_init();
    cr_assert_not_null(pr);

    CLIENT *alice_client = creg_register(cr, 10);
    PLAYER *alice_player = preg_register(pr, "Alice");
    int err = client_login(alice_client, alice_player);
    cr_assert_eq(err, 0, "Error logging in client");

    CLIENT *client = creg_lookup(cr, "Alice");
    cr_assert_eq(client, alice_client, "Returned value (%p) was not the expected value (%p)",
		  client, alice_client);
}

Test(client_registry_suite, all_players_nonempty, .init = init, .timeout = 15) {
    int err;
#ifdef NO_CLIENT_REGISTRY
    cr_assert_fail("Client registry was not implemented");
#endif
    CLIENT_REGISTRY *cr = client_registry = creg_init();
    cr_assert_not_null(cr);
    PLAYER_REGISTRY *pr = player_registry = preg_init();
    cr_assert_not_null(pr);

    CLIENT *alice_client = creg_register(cr, 10);
    PLAYER *alice_player = preg_register(pr, "Alice");
    err = client_login(alice_client, alice_player);
    cr_assert_eq(err, 0, "Error logging in client");

    CLIENT *bob_client = creg_register(cr, 11);
    PLAYER *bob_player = preg_register(pr, "Bob");
    err = client_login(bob_client, bob_player);
    cr_assert_eq(err, 0, "Error logging in client");

    CLIENT *carol_client = creg_register(cr, 12);
    PLAYER *carol_player = preg_register(pr, "Carol");
    err = client_login(carol_client, carol_player);
    cr_assert_eq(err, 0, "Error logging in client");

    PLAYER **players = creg_all_players(cr);
    PLAYER **player = players;
    while(*player != NULL && player - players <= 3) {
	// Refcount of player should be at least 4:
	//   One for PLAYER variable
	//   One for CLIENT
	//   One for player registry
	//   One for players array
	for(int i = 0; i < 4; i++)
	    player_unref(*player, "to check for increased refcount");
	player++;
    }
    cr_assert_eq(player - players, 3, "Number of players (%ld) was not the expected value (%ld)",
		 player-players, 3);
    cr_assert_eq(*player, NULL, "Too many players or players list not NULL-terminated");
}

Test(client_registry_suite, all_players_empty, .init = init, .timeout = 15) {
#ifdef NO_CLIENT_REGISTRY
    cr_assert_fail("Client registry was not implemented");
#endif
    CLIENT_REGISTRY *cr = creg_init();
    cr_assert_not_null(cr);

    PLAYER **players = creg_all_players(cr);
    cr_assert_eq(*players, NULL, "Players list not NULL-terminated");
}
