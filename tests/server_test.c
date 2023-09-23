#include <criterion/criterion.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "debug.h"
#include "game.h"
#include "protocol.h"
#include "server.h"
#include "client_registry.h"
#include "player_registry.h"
#include "jeux_globals.h"
#include "excludes.h"

/* Number of threads we create in multithreaded tests. */
#define NTHREAD (15)

static char *jeux_packet_type_names[] = {
    [JEUX_NO_PKT]       "NONE",
    [JEUX_LOGIN_PKT]    "LOGIN",
    [JEUX_USERS_PKT]    "USERS",
    [JEUX_INVITE_PKT]   "INVITE",
    [JEUX_REVOKE_PKT]   "REVOKE",
    [JEUX_ACCEPT_PKT]   "ACCEPT",
    [JEUX_DECLINE_PKT]  "DECLINE",
    [JEUX_MOVE_PKT]     "MOVE",
    [JEUX_RESIGN_PKT]   "RESIGN",
    [JEUX_ACK_PKT]      "ACK",
    [JEUX_NACK_PKT]     "NACK",
    [JEUX_INVITED_PKT]  "INVITED",
    [JEUX_REVOKED_PKT]  "REVOKED",
    [JEUX_ACCEPTED_PKT] "ACCEPTED",
    [JEUX_DECLINED_PKT] "DECLINED",
    [JEUX_MOVED_PKT]    "MOVED",
    [JEUX_RESIGNED_PKT] "RESIGNED",
    [JEUX_ENDED_PKT]    "ENDED"
};

static void init() {
    client_registry = creg_init();
    player_registry = preg_init();

    // Sending packets to disconnected clients will cause termination by SIGPIPE
    // unless we take steps to ignore it.
    struct sigaction sact;
    sact.sa_handler = SIG_IGN;
    sigemptyset(&sact.sa_mask);
    sact.sa_flags = 0;
    sigaction(SIGPIPE, &sact, NULL);
}

static void proto_init_packet(JEUX_PACKET_HEADER *pkt, JEUX_PACKET_TYPE type, size_t size) {
    memset(pkt, 0, sizeof(*pkt));
    pkt->type = type;
    struct timespec ts;
    if(clock_gettime(CLOCK_MONOTONIC, &ts) == -1) {
	perror("clock_gettime");
    }
    pkt->timestamp_sec = htonl(ts.tv_sec);
    pkt->timestamp_nsec = htonl(ts.tv_nsec);
    pkt->size = htons(size);
}

/*
 * Read a packet and check the header fields.
 * The packet and payload are returned.
 */
static void check_packet(int fd, JEUX_PACKET_TYPE type, GAME_ROLE role, int id,
			 JEUX_PACKET_HEADER *pktp, void **payloadp) {
    void *data;
    int err = proto_recv_packet(fd, pktp, &data);
    if(payloadp)
        *payloadp = data;
    cr_assert_eq(err, 0, "Error reading back packet");
    cr_assert_eq(pktp->type, type, "Packet type (%s) was not the expected type (%s)",
		 jeux_packet_type_names[pktp->type], jeux_packet_type_names[type]);
    if(role <= SECOND_PLAYER_ROLE) {
	cr_assert_eq(pktp->role, role, "Role in packet (%d) does not match expected (%d)",
		     pktp->role, role);
    }
    if(id >= 0) {
	cr_assert_eq(pktp->id, id, "ID in packet (%d) does not match expected (%d)",
		     pktp->id, id);
    }
}

/*
 * For these tests, we will set up a connection betwen a test driver thread
 * and a server thread using a socket.  The driver thread will create and
 * bind the socket, then accept a connection.  The server thread will
 * connect and then hand off the file descriptor to the jeux_client_service
 * function, as if the connection had been made over the network.
 * Communication over the connection will be done using whatever protocol
 * functions are linked, so if those don't work then the present tests will
 * likely also fail.
 */

/*
 * Thread function that connects to a socket with a specified name,
 * then hands off the resulting file descriptor to jeux_client_service.
 * Errors cause the invoking test to fail.
 */
static void *server_thread(void *args) {
    char *name = args;  // socket name
    struct sockaddr_un sa;
    sa.sun_family = AF_LOCAL;
    snprintf(sa.sun_path, sizeof(sa.sun_path), "%s", name);
    int sockfd = socket(AF_LOCAL, SOCK_STREAM, 0);
    cr_assert(sockfd >= 0, "Failed to create socket");
    int err = connect(sockfd, (struct sockaddr *)&sa, sizeof(struct sockaddr_un));
    cr_assert(err == 0, "Failed to connect to socket");
    int *connfdp = malloc(sizeof(int));
    *connfdp = sockfd;
    jeux_client_service(connfdp);
    return NULL;
}

/*
 * Set up a connection to a server thread, via a socket with a specified name.
 * The file descriptor to be used to communicate with the server is returned.
 * Errors cause the invoking test to fail.
 */
static int setup_connection(char *name) {
    // Set up socket to receive connection from server thread.
    int listen_fd = socket(AF_LOCAL, SOCK_STREAM, 0);
    cr_assert(listen_fd >= 0, "Failed to create socket");
    struct sockaddr_un sa;
    sa.sun_family = AF_LOCAL;
    snprintf(sa.sun_path, sizeof(sa.sun_path), "%s", name);
    unlink((char *)sa.sun_path);
    int err = bind(listen_fd, (struct sockaddr *)&sa, sizeof(struct sockaddr_un));
    cr_assert(err >= 0, "Failed to bind socket");
    err = listen(listen_fd, 0);
    cr_assert(err >= 0, "Failed to listen on socket");

    // Create server thread, passing the name of the socket.
    pthread_t tid;
    err = pthread_create(&tid, NULL, server_thread, name);
    cr_assert(err >= 0, "Failed to create server thread");

    // Accept connection from server thread.
    int connfd = accept(listen_fd, NULL, NULL);
    cr_assert(connfd >= 0, "Failed to accept connection");
    return connfd;
}    

/*
 * Perform a login operation on a specified connection, for a specified
 * user name.  Nothing is returned; errors cause the invoking test to fail.
 */
static void login_func(int connfd, char *uname) {
    JEUX_PACKET_HEADER pkt;
    proto_init_packet(&pkt, JEUX_LOGIN_PKT, strlen(uname));
    int err = proto_send_packet(connfd, &pkt, uname);
    cr_assert_eq(err, 0, "Send packet returned an error");
    memset(&pkt, 0, sizeof(pkt));
    check_packet(connfd, JEUX_ACK_PKT, 3, -1, &pkt, NULL);
}

/*
 * Test driver thread that sends a packet other than LOGIN over the connection
 * and checks that NACK is received.
 */
static void *ping_thread(void *arg) {
    return NULL;
}

/*
 * Create a connection and then "ping" it to elicit a NACK.
 */
Test(server_suite, ping, .init = init, .timeout = 5) {
#ifdef NO_SERVER
    cr_assert_fail("Server module was not implemented");
#endif
    char *sockname = "ping.sock";
    int connfd = setup_connection(sockname);

    JEUX_PACKET_HEADER pkt;
    proto_init_packet(&pkt, JEUX_USERS_PKT, 0);
    int err = proto_send_packet(connfd, &pkt, NULL);
    cr_assert_eq(err, 0, "Send packet returned an error");
    check_packet(connfd, JEUX_NACK_PKT, 3, -1, &pkt, NULL);
    close(connfd);
}

/*
 * Create a connection, log in, then close the connection.
 */
Test(server_suite, valid_login, .init = init, .timeout = 5) {
#ifdef NO_SERVER
    cr_assert_fail("Server module was not implemented");
#endif
    char *sockname = "valid_login.sock";
    char *username = "Alice";
    int connfd = setup_connection(sockname);
    login_func(connfd, username);
    close(connfd);
}

/*
 * I would test a LOGIN with no payload, except that my server
 * allocates an empty payload and goes ahead and does a login with
 * an empty username.  So probably not fair.
 */

/*
 * I would also test attempting to LOGIN twice with the same username,
 * except that my server allows that and probably cannot avoid this
 * without some amount of design change.
 */

/*
 * The following tests have some redundancy with the tests for the
 * lower-level client module; however, the present tests verify the
 * proper dispatching of internal functions in response to incoming packets.
 * They also verifies ACK/NACK which is not sent at the lower level.
 */

/*
 * Set up two clients, then make an invitation from one to the other
 * and check that the required INVITED and ACK packets are sent.
 * Finally, close the connections to cause the invitation to be revoked.
 */
Test(server_suite, invite_disconnect, .init = init, .timeout = 5) {
#ifdef NO_SERVER
    cr_assert_fail("Server module was not implemented");
#endif
    char *sockname1 = "server_invite_disconnect1.pkts";
    char *sockname2 = "server_invite_disconnect2.pkts";
    char *username1 = "Alice";
    char *username2 = "Bob";
    int fd1 = setup_connection(sockname1);
    int fd2 = setup_connection(sockname2);
    login_func(fd1, username1);
    login_func(fd2, username2);

    JEUX_PACKET_HEADER pkt;
    proto_init_packet(&pkt, JEUX_INVITE_PKT, strlen(username2));
    pkt.role = SECOND_PLAYER_ROLE;
    int err = proto_send_packet(fd1, &pkt, username2);
    cr_assert_eq(err, 0, "Send packet returned an error");

    JEUX_PACKET_HEADER in_pkt1, in_pkt2;
    check_packet(fd1, JEUX_ACK_PKT, 3, -1, &in_pkt1, NULL);
    check_packet(fd2, JEUX_INVITED_PKT, SECOND_PLAYER_ROLE, -1, &in_pkt2, NULL);

    close(fd1);
    close(fd2);
}

/*
 * Set up two clients, then make an invitation from one to the other
 * and check that the required INVITED and ACK packets are sent.
 * Then, the target declines the invitation and we check that the
 * required DECLINED and ACK packets are sent.
 */
Test(server_suite, invite_decline, .init = init, .timeout = 5) {
#ifdef NO_SERVER
    cr_assert_fail("Server module was not implemented");
#endif
    char *sockname1 = "server_invite_decline1.pkts";
    char *sockname2 = "server_invite_decline2.pkts";
    char *username1 = "Alice";
    char *username2 = "Bob";
    int fd1 = setup_connection(sockname1);
    int fd2 = setup_connection(sockname2);
    login_func(fd1, username1);
    login_func(fd2, username2);

    JEUX_PACKET_HEADER pkt;
    proto_init_packet(&pkt, JEUX_INVITE_PKT, strlen(username2));
    pkt.role = SECOND_PLAYER_ROLE;
    int err = proto_send_packet(fd1, &pkt, username2);
    cr_assert_eq(err, 0, "Send packet returned an error");

    JEUX_PACKET_HEADER in_pkt1, in_pkt2;
    check_packet(fd1, JEUX_ACK_PKT, 3, -1, &in_pkt1, NULL);
    check_packet(fd2, JEUX_INVITED_PKT, SECOND_PLAYER_ROLE, -1, &in_pkt2, NULL);
    int id1 = in_pkt1.id;
    int id2 = in_pkt2.id;

    proto_init_packet(&pkt, JEUX_DECLINE_PKT, 0);
    pkt.id = id2;
    err = proto_send_packet(fd2, &pkt, NULL);
    cr_assert_eq(err, 0, "Send packet returned an error");

    check_packet(fd1, JEUX_DECLINED_PKT, 3, id1, &in_pkt1, NULL);
    check_packet(fd2, JEUX_ACK_PKT, 3, -1, &in_pkt2, NULL);

    close(fd1);
    close(fd2);
}

/*
 * Set up two clients, then make an invitation from one to the other
 * and check that the required INVITED and ACK packets are sent.
 * Then, the source revokes the invitation and we check that the
 * required REVOKED and ACK packets are sent.
 */
Test(server_suite, invite_revoke, .init = init, .timeout = 5) {
#ifdef NO_SERVER
    cr_assert_fail("Server module was not implemented");
#endif
    char *sockname1 = "server_invite_revoke1.pkts";
    char *sockname2 = "server_invite_revoke2.pkts";
    char *username1 = "Alice";
    char *username2 = "Bob";
    int fd1 = setup_connection(sockname1);
    int fd2 = setup_connection(sockname2);
    login_func(fd1, username1);
    login_func(fd2, username2);

    JEUX_PACKET_HEADER pkt;
    proto_init_packet(&pkt, JEUX_INVITE_PKT, strlen(username2));
    pkt.role = SECOND_PLAYER_ROLE;
    int err = proto_send_packet(fd1, &pkt, username2);
    cr_assert_eq(err, 0, "Send packet returned an error");

    JEUX_PACKET_HEADER in_pkt1, in_pkt2;
    check_packet(fd1, JEUX_ACK_PKT, 3, -1, &in_pkt1, NULL);
    check_packet(fd2, JEUX_INVITED_PKT, SECOND_PLAYER_ROLE, -1, &in_pkt2, NULL);
    int id1 = in_pkt1.id;
    int id2 = in_pkt2.id;

    proto_init_packet(&pkt, JEUX_REVOKE_PKT, 0);
    pkt.id = id1;
    err = proto_send_packet(fd1, &pkt, NULL);
    cr_assert_eq(err, 0, "Send packet returned an error");

    check_packet(fd1, JEUX_ACK_PKT, 3, -1, &in_pkt1, NULL);
    check_packet(fd2, JEUX_REVOKED_PKT, 3, id2, &in_pkt2, NULL);

    close(fd1);
    close(fd2);
}

/*
 * Set up two clients, then make an invitation from one to the other
 * and check that the required INVITED and ACK packets are sent.
 * Then, the target accepts the invitation and we check that the
 * required ACCEPTED and ACK packets are sent.
 * In this version, the target has the role of the second player,
 * so the ACCEPTED packet should contain the payload with the initial
 * game state.
 */
Test(server_suite, invite_accept_second, .init = init, .timeout = 5) {
#ifdef NO_SERVER
    cr_assert_fail("Server module was not implemented");
#endif
    char *sockname1 = "server_invite_accept_second1.pkts";
    char *sockname2 = "server_invite_accept_second2.pkts";
    char *username1 = "Alice";
    char *username2 = "Bob";
    int fd1 = setup_connection(sockname1);
    int fd2 = setup_connection(sockname2);
    login_func(fd1, username1);
    login_func(fd2, username2);

    JEUX_PACKET_HEADER pkt;
    proto_init_packet(&pkt, JEUX_INVITE_PKT, strlen(username2));
    pkt.role = SECOND_PLAYER_ROLE;
    int err = proto_send_packet(fd1, &pkt, username2);
    cr_assert_eq(err, 0, "Send packet returned an error");

    JEUX_PACKET_HEADER in_pkt1, in_pkt2;
    check_packet(fd1, JEUX_ACK_PKT, 3, -1, &in_pkt1, NULL);
    check_packet(fd2, JEUX_INVITED_PKT, SECOND_PLAYER_ROLE, -1, &in_pkt2, NULL);
    int id1 = in_pkt1.id;
    int id2 = in_pkt2.id;

    proto_init_packet(&pkt, JEUX_ACCEPT_PKT, 0);
    pkt.id = id2;
    err = proto_send_packet(fd2, &pkt, NULL);
    cr_assert_eq(err, 0, "Send packet returned an error");

    check_packet(fd1, JEUX_ACCEPTED_PKT, 3, id1, &in_pkt1, NULL);
    check_packet(fd2, JEUX_ACK_PKT, 3, -1, &in_pkt2, NULL);
    cr_assert(ntohs(in_pkt1.size) > 0, "The ACCEPTED packet had no payload");

    close(fd1);
    close(fd2);
}

/*
 * Set up two clients, then make an invitation from one to the other
 * and check that the required INVITED and ACK packets are sent.
 * Then, the target accepts the invitation and we check that the
 * required ACCEPTED and ACK packets are sent.
 * In this version, the target has the role of the first player,
 * so the ACK packet should contain the payload with the initial
 * game state.
 */
Test(server_suite, invite_accept_first, .init = init, .timeout = 5) {
#ifdef NO_SERVER
    cr_assert_fail("Server module was not implemented");
#endif
    char *sockname1 = "server_invite_accept_first1.pkts";
    char *sockname2 = "server_invite_accept_first2.pkts";
    char *username1 = "Alice";
    char *username2 = "Bob";
    int fd1 = setup_connection(sockname1);
    int fd2 = setup_connection(sockname2);
    login_func(fd1, username1);
    login_func(fd2, username2);

    JEUX_PACKET_HEADER pkt;
    proto_init_packet(&pkt, JEUX_INVITE_PKT, strlen(username2));
    pkt.role = FIRST_PLAYER_ROLE;
    int err = proto_send_packet(fd1, &pkt, username2);
    cr_assert_eq(err, 0, "Send packet returned an error");

    JEUX_PACKET_HEADER in_pkt1, in_pkt2;
    check_packet(fd1, JEUX_ACK_PKT, 3, -1, &in_pkt1, NULL);
    check_packet(fd2, JEUX_INVITED_PKT, FIRST_PLAYER_ROLE, -1, &in_pkt2, NULL);
    int id1 = in_pkt1.id;
    int id2 = in_pkt2.id;

    proto_init_packet(&pkt, JEUX_ACCEPT_PKT, 0);
    pkt.id = id2;
    err = proto_send_packet(fd2, &pkt, NULL);
    cr_assert_eq(err, 0, "Send packet returned an error");

    check_packet(fd1, JEUX_ACCEPTED_PKT, 3, id1, &in_pkt1, NULL);
    check_packet(fd2, JEUX_ACK_PKT, 3, -1, &in_pkt2, NULL);
    cr_assert(ntohs(in_pkt2.size) > 0, "The ACK packet had no payload");

    close(fd1);
    close(fd2);
}

/*
 * Set up two clients, then make an invitation from one to the other
 * and check that the required INVITED and ACK packets are sent.
 * Then, the target accepts the invitation and we check that the
 * required ACCEPTED and ACK packets are sent.
 * Finally, the source resigns the game and we check that the required
 * RESIGNED, ACK, and ENDED packets are sent.
 */
Test(server_suite, invite_accept_resign, .init = init, .timeout = 5) {
#ifdef NO_SERVER
    cr_assert_fail("Server module was not implemented");
#endif
    char *sockname1 = "server_invite_accept_resign1.pkts";
    char *sockname2 = "server_invite_accept_resign2.pkts";
    char *username1 = "Alice";
    char *username2 = "Bob";
    int fd1 = setup_connection(sockname1);
    int fd2 = setup_connection(sockname2);
    login_func(fd1, username1);
    login_func(fd2, username2);

    JEUX_PACKET_HEADER pkt;
    proto_init_packet(&pkt, JEUX_INVITE_PKT, strlen(username2));
    pkt.role = SECOND_PLAYER_ROLE;
    int err = proto_send_packet(fd1, &pkt, username2);
    cr_assert_eq(err, 0, "Send packet returned an error");

    JEUX_PACKET_HEADER in_pkt1, in_pkt2;
    check_packet(fd1, JEUX_ACK_PKT, 3, -1, &in_pkt1, NULL);
    check_packet(fd2, JEUX_INVITED_PKT, SECOND_PLAYER_ROLE, -1, &in_pkt2, NULL);
    int id1 = in_pkt1.id;
    int id2 = in_pkt2.id;

    proto_init_packet(&pkt, JEUX_ACCEPT_PKT, 0);
    pkt.id = id2;
    err = proto_send_packet(fd2, &pkt, NULL);
    cr_assert_eq(err, 0, "Send packet returned an error");

    check_packet(fd1, JEUX_ACCEPTED_PKT, 3, id1, &in_pkt1, NULL);
    check_packet(fd2, JEUX_ACK_PKT, 3, -1, &in_pkt2, NULL);
    cr_assert(ntohs(in_pkt1.size) > 0, "The ACCEPTED packet had no payload");

    proto_init_packet(&pkt, JEUX_RESIGN_PKT, 0);
    pkt.id = id1;
    err = proto_send_packet(fd1, &pkt, NULL);
    cr_assert_eq(err, 0, "Send packet returned an error");

    check_packet(fd1, JEUX_ENDED_PKT, 3, id1, &in_pkt1, NULL);
    check_packet(fd1, JEUX_ACK_PKT, 3, -1, &in_pkt1, NULL);
    check_packet(fd2, JEUX_RESIGNED_PKT, 3, id2, &in_pkt2, NULL);
    check_packet(fd2, JEUX_ENDED_PKT, 3, id2, &in_pkt2, NULL);

    close(fd1);
    close(fd2);
}

/*
 * Set up two clients, then make an invitation from one to the other
 * and check that the required INVITED and ACK packets are sent.
 * Then, the target accepts the invitation and we check that the
 * required ACCEPTED and ACK packets are sent.
 * Finally, the first player makes a move we check that the required
 * MOVED and ACK packets are sent.
 */
Test(server_suite, invite_accept_move, .init = init, .timeout = 5) {
#ifdef NO_SERVER
    cr_assert_fail("Server module was not implemented");
#endif
    char *sockname1 = "server_invite_accept_move1.pkts";
    char *sockname2 = "server_invite_accept_move2.pkts";
    char *username1 = "Alice";
    char *username2 = "Bob";
    int fd1 = setup_connection(sockname1);
    int fd2 = setup_connection(sockname2);
    login_func(fd1, username1);
    login_func(fd2, username2);

    JEUX_PACKET_HEADER pkt;
    proto_init_packet(&pkt, JEUX_INVITE_PKT, strlen(username2));
    pkt.role = SECOND_PLAYER_ROLE;
    int err = proto_send_packet(fd1, &pkt, username2);
    cr_assert_eq(err, 0, "Send packet returned an error");

    JEUX_PACKET_HEADER in_pkt1, in_pkt2;
    check_packet(fd1, JEUX_ACK_PKT, 3, -1, &in_pkt1, NULL);
    check_packet(fd2, JEUX_INVITED_PKT, SECOND_PLAYER_ROLE, -1, &in_pkt2, NULL);
    int id1 = in_pkt1.id;
    int id2 = in_pkt2.id;

    proto_init_packet(&pkt, JEUX_ACCEPT_PKT, 0);
    pkt.id = id2;
    err = proto_send_packet(fd2, &pkt, NULL);
    cr_assert_eq(err, 0, "Send packet returned an error");

    check_packet(fd1, JEUX_ACCEPTED_PKT, 3, id1, &in_pkt1, NULL);
    check_packet(fd2, JEUX_ACK_PKT, 3, -1, &in_pkt2, NULL);
    cr_assert(ntohs(in_pkt1.size) > 0, "The ACCEPTED packet had no payload");

    char *move = "5";
    proto_init_packet(&pkt, JEUX_MOVE_PKT, strlen(move));
    pkt.id = id1;
    err = proto_send_packet(fd1, &pkt, move);
    cr_assert_eq(err, 0, "Send packet returned an error");

    check_packet(fd1, JEUX_ACK_PKT, 3, -1, &in_pkt1, NULL);
    check_packet(fd2, JEUX_MOVED_PKT, 3, id2, &in_pkt2, NULL);
    cr_assert(ntohs(in_pkt2.size) > 0, "The MOVED packet had no payload");

    close(fd1);
    close(fd2);
}

/*
 * Create a connection, log in a single user and then send USERS.
 * Check for an ACK with the correct payload.
 * Then close the connection.
 */
Test(server_suite, login_users, .init = init, .timeout = 5) {
#ifdef NO_SERVER
    cr_assert_fail("Server module was not implemented");
#endif
    char *sockname = "login_users.sock";
    char *username = "Alice";
    int connfd = setup_connection(sockname);
    login_func(connfd, username);

    JEUX_PACKET_HEADER pkt;
    proto_init_packet(&pkt, JEUX_USERS_PKT, 0);
    int err = proto_send_packet(connfd, &pkt, NULL);
    cr_assert_eq(err, 0, "Send packet returned an error");
    memset(&pkt, 0, sizeof(pkt));
    void *data = NULL;
    check_packet(connfd, JEUX_ACK_PKT, 3, -1, &pkt, &data);
    char *str = calloc(ntohs(pkt.size)+1, 1);
    strncpy(str, data, ntohs(pkt.size));
    char *exp = "Alice\t1500\n";
    cr_assert(!strcmp(str, exp), "Returned payload (%s) was not the expected (%s)",
	      str, data);

    close(connfd);
}

/*
 * Concurrently create many connections and log in a different
 * user on each one.  Then send USERS and check the payload that
 * is returned.
 */

struct login_thread_args {
    char sockname[32];
    char username[32];
};

void *login_thread(void *args) {
    struct login_thread_args *ap = args;
    int connfd = setup_connection(ap->sockname);
    login_func(connfd, ap->username);
    return (void *)(long)connfd;
}

Test(server_suite, login_many_users, .init = init, .timeout = 5) {
#ifdef NO_SERVER
    cr_assert_fail("Server module was not implemented");
#endif
    char *sockname = "login_many_users.sock";

    pthread_t tids[NTHREAD];
    // The first connection will be used later to issue USERS.
    int connfd = setup_connection(sockname);
    login_func(connfd, "u0");

    // The rest of the connections are made concurrently.
    for(int i = 1; i < NTHREAD; i++) {
	struct login_thread_args *args = malloc(sizeof(struct login_thread_args));
	snprintf(args->sockname, sizeof(args->sockname), "%s.%d", sockname, i);
	snprintf(args->username, sizeof(args->username), "u%d", i);
	int err = pthread_create(&tids[i], NULL, login_thread, args);
	cr_assert(err >= 0, "Failed to create test thread");
    }
    // Wait for all the threads to finish.
    int fds[NTHREAD];
    for(int i = 1; i < NTHREAD; i++)
	fds[i] = (int)pthread_join(tids[i], NULL);

    // Send USERS over the first connection and get the response.
    JEUX_PACKET_HEADER pkt;
    proto_init_packet(&pkt, JEUX_USERS_PKT, 0);
    int err = proto_send_packet(connfd, &pkt, NULL);
    cr_assert_eq(err, 0, "Send packet returned an error");
    void *data = NULL;
    check_packet(connfd, JEUX_ACK_PKT, 3, -1, &pkt, &data);
    char *str = calloc(ntohs(pkt.size)+1, 1);
    strncpy(str, data, ntohs(pkt.size));
    
    // Check the response
    //fprintf(stderr, "\n%s\n", str);
    FILE *f = fmemopen(str, strlen(str), "r");
    int nlines = 0;
    char *ln = NULL;
    size_t sz = 0;
    while(getline(&ln, &sz, f) > 0) {
	nlines++;
	int count = 0;
	for(int i = 0; i < NTHREAD; i++) {
	    char line[64];
	    snprintf(line, sizeof(line), "u%d\t1500\n", i);
	    if(!strcmp(ln, line))
		count++;
	}
	cr_assert_eq(count, 1, "USERS output was incorrect: \n%s\n", str);
	free(ln);
	sz = 0; ln = NULL;
    }
    free(ln);
    fclose(f);
    cr_assert_eq(nlines, NTHREAD, "Number of lines (%d) did not match expected (%d)",
		 nlines, NTHREAD);

    // Close all the connections.
    for(int i = 1; i < NTHREAD; i++)
	close(fds[i]);
    close(connfd);
}
