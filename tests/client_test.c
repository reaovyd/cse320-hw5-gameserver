#include <criterion/criterion.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "debug.h"
#include "game.h"
#include "protocol.h"
#include "client_registry.h"
#include "client.h"
#include "player_registry.h"
#include "jeux_globals.h"
#include "excludes.h"

/* Number of iterations for which we run the concurrency test. */
#define NITERS (100000)

/* Maximum number of invitations we issue in the concurrency test. */
#define NINVITATION (100)

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
 * These tests involve directly calling functions of the client module.
 * These functions require a CLIENT objects created by client_create
 * on an underlying file descriptor.  The test driver does not send packets
 * on the file descriptor; it is used only for receiving response packets
 * sent as a result of the functions called.  We will use a disk file to
 * store these packets so that they can be read back and checked.
 * This requires two file descriptors: one that is stored in the CLIENT,
 * and the other that is used by the test driver to read back packets.
 */

static void init() {
    client_registry = creg_init();
    player_registry = preg_init();
}

/*
 * Create a CLIENT, together with an associated file to which it can
 * send packets, and another file descriptor that can be used to read back
 * the packets.  The client is logged in under a specified username.
 */
static void setup_client(char *fname, char *uname, CLIENT **clientp, int *readfdp) {
    int writefd = open(fname, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    cr_assert(writefd >= 0, "Failed to open packet file for writing");
    int readfd = open(fname, O_RDONLY);
    cr_assert(readfd >= 0, "Failed to open packet file for reading");
    CLIENT *client = client_create(client_registry, writefd);
    cr_assert_not_null(client, "Error creating client");
    cr_assert_eq(client_get_fd(client), writefd, "Client has wrong file descriptor");
    PLAYER *player = preg_register(player_registry, uname);
    cr_assert_not_null(player, "Error registering player");
    int err = client_login(client, player);
    cr_assert_eq(err, 0, "Error logging in client");
    *clientp = client;
    *readfdp = readfd;
}

/*
 * Assert that there are currently no more packets to be read back.
 */
static void assert_no_packets(int fd) {
    JEUX_PACKET_HEADER pkt;
    void *data;
    int err = proto_recv_packet(fd, &pkt, &data);
    cr_assert_eq(err, -1, "There should be no packets to read");
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
 * Test that just sets up and logs in a client, checks that the
 * underlying PLAYER can be retrieved, and then logs the client out.
 * No packets are generated.
 */
Test(client_suite, login_logout, .init = init, .timeout = 5) {
#ifdef NO_CLIENT
    cr_assert_fail("Client module was not implemented");
#endif
    char *fname = "login_logout.pkts";
    char *uname = "Alice";
    int fd;
    CLIENT *client;
    setup_client(fname, uname, &client, &fd);
    PLAYER *player = client_get_player(client);
    cr_assert_not_null(player, "Error getting player from client");
    cr_assert(!strcmp(player_get_name(player), uname), "Player had wrong username");
    int err = client_logout(client);
    cr_assert_eq(err, 0, "Error logging out client");
}

/*
 * Test sending a single packet using client_send_packet and then
 * and reading it back to verify it.
 */
Test(client_suite, send_packet, .init = init, .timeout = 5) {
#ifdef NO_CLIENT
    cr_assert_fail("Client module was not implemented");
#endif
    char *fname = "send_packet.pkts";
    char *uname = "Alice";
    int fd;
    CLIENT *client;
    setup_client(fname, uname, &client, &fd);
    void *out_payload = "Hello";
    void *in_payload = NULL;
    JEUX_PACKET_HEADER out_pkt, in_pkt;
    proto_init_packet(&out_pkt, JEUX_ACK_PKT, strlen(out_payload));
    int err = client_send_packet(client, &out_pkt, out_payload);
    cr_assert_eq(err, 0, "Error sending packet");
    proto_recv_packet(fd, &in_pkt, &in_payload);
    cr_assert(!memcmp(&in_pkt, &out_pkt, sizeof(in_pkt)), "Packet header readback was incorrect");
    cr_assert(!memcmp(in_payload, out_payload, ntohs(in_pkt.size)), "Payload readback was incorrect");
}

/*
 * Test sending a single ACK packet using client_send_ack and then
 * and reading it back to verify it.
 */
Test(client_suite, send_ack, .init = init, .timeout = 5) {
#ifdef NO_CLIENT
    cr_assert_fail("Client module was not implemented");
#endif
    char *fname = "send_ack.pkts";
    char *uname = "Alice";
    int fd;
    CLIENT *client;
    setup_client(fname, uname, &client, &fd);
    void *out_payload = "Hello";
    void *in_payload = NULL;
    JEUX_PACKET_HEADER in_pkt;
    int err = client_send_ack(client, out_payload, strlen(out_payload));
    cr_assert_eq(err, 0, "Error sending ACK");
    check_packet(fd, JEUX_ACK_PKT, 3, -1, &in_pkt, &in_payload);
    cr_assert_eq(ntohs(in_pkt.size), strlen(out_payload), "Payload size readback was incorrect");
    cr_assert(!memcmp(in_payload, out_payload, ntohs(in_pkt.size)), "Payload readback was incorrect");
}

/*
 * Test sending a single NACK packet using client_send_nack and then
 * and reading it back to verify it.
 */
Test(client_suite, send_nack, .init = init, .timeout = 5) {
#ifdef NO_CLIENT
    cr_assert_fail("Client module was not implemented");
#endif
    char *fname = "send_nack.pkts";
    char *uname = "Alice";
    int fd;
    CLIENT *client;
    setup_client(fname, uname, &client, &fd);
    JEUX_PACKET_HEADER in_pkt;
    int err = client_send_nack(client);
    cr_assert_eq(err, 0, "Error sending ACK");
    check_packet(fd, JEUX_NACK_PKT, 3, -1, &in_pkt, NULL);
    cr_assert_eq(ntohs(in_pkt.size), 0, "Payload size readback was incorrect");
}

/*
 * Set up two clients, then make an invitation from one to the other
 * and check that the required INVITED packet was sent.
 */
Test(client_suite, invite, .init = init, .timeout = 5) {
#ifdef NO_CLIENT
    cr_assert_fail("Client module was not implemented");
#endif
    char *fname1 = "client_invite1.pkts";
    char *fname2 = "client_invite2.pkts";
    char *uname1 = "Alice";
    char *uname2 = "Bob";
    int fd1, fd2;
    CLIENT *client1, *client2;
    setup_client(fname1, uname1, &client1, &fd1);
    setup_client(fname2, uname2, &client2, &fd2);
    int id1 = client_make_invitation(client1, client2, FIRST_PLAYER_ROLE, SECOND_PLAYER_ROLE);
    cr_assert_eq(id1, 0, "Error making invitation");

    JEUX_PACKET_HEADER in_pkt2;
    assert_no_packets(fd1);
    check_packet(fd2, JEUX_INVITED_PKT, SECOND_PLAYER_ROLE, -1, &in_pkt2, NULL);
}

/*
 * Set up two clients, make an invitation from one to the other,
 * then have the target decline the invitation.  Verify the packets sent.
 */
Test(client_suite, invite_decline, .init = init, .timeout = 5) {
#ifdef NO_CLIENT
    cr_assert_fail("Client module was not implemented");
#endif
    char *fname1 = "client_invite_decline1.pkts";
    char *fname2 = "client_invite_decline2.pkts";
    char *uname1 = "Alice";
    char *uname2 = "Bob";
    int fd1, fd2;
    CLIENT *client1, *client2;
    setup_client(fname1, uname1, &client1, &fd1);
    setup_client(fname2, uname2, &client2, &fd2);
    int id1 = client_make_invitation(client1, client2, FIRST_PLAYER_ROLE, SECOND_PLAYER_ROLE);
    cr_assert_neq(id1, -1, "Error making invitation");

    JEUX_PACKET_HEADER in_pkt1, in_pkt2;
    check_packet(fd2, JEUX_INVITED_PKT, SECOND_PLAYER_ROLE, -1, &in_pkt2, NULL);

    int err = client_decline_invitation(client2, in_pkt2.id);
    cr_assert_eq(err, 0, "Error declining invitation");

    check_packet(fd1, JEUX_DECLINED_PKT, 3, id1, &in_pkt1, NULL);
    assert_no_packets(fd2);
}

/*
 * Set up two clients, make an invitation from one to the other,
 * then have the source revoke the invitation.  Verify the packets sent.
 */
Test(client_suite, invite_revoke, .init = init, .timeout = 5) {
#ifdef NO_CLIENT
    cr_assert_fail("Client module was not implemented");
#endif
    char *fname1 = "client_invite_revoke1.pkts";
    char *fname2 = "client_invite_revoke2.pkts";
    char *uname1 = "Alice";
    char *uname2 = "Bob";
    int fd1, fd2;
    CLIENT *client1, *client2;
    setup_client(fname1, uname1, &client1, &fd1);
    setup_client(fname2, uname2, &client2, &fd2);
    int id1 = client_make_invitation(client1, client2, FIRST_PLAYER_ROLE, SECOND_PLAYER_ROLE);
    cr_assert_neq(id1, -1, "Error making invitation");

    JEUX_PACKET_HEADER in_pkt2;
    check_packet(fd2, JEUX_INVITED_PKT, SECOND_PLAYER_ROLE, -1, &in_pkt2, NULL);
    int id2 = in_pkt2.id;

    int err = client_revoke_invitation(client1, id1);
    cr_assert_eq(err, 0, "Error revoking invitation");

    assert_no_packets(fd1);
    check_packet(fd2, JEUX_REVOKED_PKT, 3, id2, &in_pkt2, NULL);
}

/*
 * Set up two clients, make an invitation from one to the other,
 * then have the source attempt to decline the invitation.
 * Verify the packets sent.
 */
Test(client_suite, invite_decline_wrong, .init = init, .timeout = 5) {
#ifdef NO_CLIENT
    cr_assert_fail("Client module was not implemented");
#endif
    char *fname1 = "client_invite_decline_wrong1.pkts";
    char *fname2 = "client_invite_decline_wrong2.pkts";
    char *uname1 = "Alice";
    char *uname2 = "Bob";
    int fd1, fd2;
    CLIENT *client1, *client2;
    setup_client(fname1, uname1, &client1, &fd1);
    setup_client(fname2, uname2, &client2, &fd2);
    int id1 = client_make_invitation(client1, client2, FIRST_PLAYER_ROLE, SECOND_PLAYER_ROLE);
    cr_assert_neq(id1, -1, "Error making invitation");

    JEUX_PACKET_HEADER in_pkt2;
    check_packet(fd2, JEUX_INVITED_PKT, SECOND_PLAYER_ROLE, -1, &in_pkt2, NULL);

    int err = client_decline_invitation(client1, id1);
    cr_assert_eq(err, -1, "There should have been an error declining the invitation");

    assert_no_packets(fd1);
    assert_no_packets(fd2);
}

/*
 * Set up two clients, make an invitation from one to the other,
 * then have the target attempt to revoke the invitation.
 * Verify the packets sent.
 */
Test(client_suite, invite_revoke_wrong, .init = init, .timeout = 5) {
#ifdef NO_CLIENT
    cr_assert_fail("Client module was not implemented");
#endif
    char *fname1 = "client_invite_revoke_wrong1.pkts";
    char *fname2 = "client_invite_revoke_wrong2.pkts";
    char *uname1 = "Alice";
    char *uname2 = "Bob";
    int fd1, fd2;
    CLIENT *client1, *client2;
    setup_client(fname1, uname1, &client1, &fd1);
    setup_client(fname2, uname2, &client2, &fd2);
    int id1 = client_make_invitation(client1, client2, FIRST_PLAYER_ROLE, SECOND_PLAYER_ROLE);
    cr_assert_neq(id1, -1, "Error making invitation");

    JEUX_PACKET_HEADER in_pkt2;
    check_packet(fd2, JEUX_INVITED_PKT, SECOND_PLAYER_ROLE, -1, &in_pkt2, NULL);

    int err = client_revoke_invitation(client2, in_pkt2.id);
    cr_assert_eq(err, -1, "There should have been an error revoking the invitation");

    assert_no_packets(fd1);
    assert_no_packets(fd2);
}

/*
 * Set up two clients, make an invitation from one to the other,
 * then have the target attempt to resign the game.
 * Verify the packets sent.
 */
Test(client_suite, invite_resign_wrong, .init = init, .timeout = 5) {
#ifdef NO_CLIENT
    cr_assert_fail("Client module was not implemented");
#endif
    char *fname1 = "client_invite_resign_wrong1.pkts";
    char *fname2 = "client_invite_resign_wrong2.pkts";
    char *uname1 = "Alice";
    char *uname2 = "Bob";
    int fd1, fd2;
    CLIENT *client1, *client2;
    setup_client(fname1, uname1, &client1, &fd1);
    setup_client(fname2, uname2, &client2, &fd2);
    int id1 = client_make_invitation(client1, client2, FIRST_PLAYER_ROLE, SECOND_PLAYER_ROLE);
    cr_assert_neq(id1, -1, "Error making invitation");

    JEUX_PACKET_HEADER in_pkt2;
    check_packet(fd2, JEUX_INVITED_PKT, SECOND_PLAYER_ROLE, -1, &in_pkt2, NULL);

    int err = client_resign_game(client2, in_pkt2.id);
    cr_assert_eq(err, -1, "There should have been an error resigning the game");

    assert_no_packets(fd1);
    assert_no_packets(fd2);
}

/*
 * Set up two clients, make an invitation from one to the other,
 * then have the target accept the invitation.  Verify the packets sent.
 */
Test(client_suite, invite_accept, .init = init, .timeout = 5) {
#ifdef NO_CLIENT
    cr_assert_fail("Client module was not implemented");
#endif
    char *fname1 = "client_invite_accept1.pkts";
    char *fname2 = "client_invite_accept2.pkts";
    char *uname1 = "Alice";
    char *uname2 = "Bob";
    int fd1, fd2;
    CLIENT *client1, *client2;
    setup_client(fname1, uname1, &client1, &fd1);
    setup_client(fname2, uname2, &client2, &fd2);
    int id1 = client_make_invitation(client1, client2, FIRST_PLAYER_ROLE, SECOND_PLAYER_ROLE);
    cr_assert_neq(id1, -1, "Error making invitation");

    JEUX_PACKET_HEADER in_pkt1, in_pkt2;
    check_packet(fd2, JEUX_INVITED_PKT, SECOND_PLAYER_ROLE, -1, &in_pkt2, NULL);

    char *str;
    int err = client_accept_invitation(client2, in_pkt2.id, &str);
    cr_assert_eq(err, 0, "Error accepting invitation");

    check_packet(fd1, JEUX_ACCEPTED_PKT, 3, id1, &in_pkt1, NULL);
    assert_no_packets(fd2);
}

/*
 * Set up two clients, make an invitation from one to the other,
 * then have the target accept the invitation and the source resign.
 * Verify the packets sent.
 */
Test(client_suite, invite_accept_resign_source, .init = init, .timeout = 5) {
#ifdef NO_CLIENT
    cr_assert_fail("Client module was not implemented");
#endif
    char *fname1 = "client_invite_accept_resign_source1.pkts";
    char *fname2 = "client_invite_accept_resign_source2.pkts";
    char *uname1 = "Alice";
    char *uname2 = "Bob";
    int fd1, fd2;
    CLIENT *client1, *client2;
    setup_client(fname1, uname1, &client1, &fd1);
    setup_client(fname2, uname2, &client2, &fd2);
    int id1 = client_make_invitation(client1, client2, FIRST_PLAYER_ROLE, SECOND_PLAYER_ROLE);
    cr_assert_neq(id1, -1, "Error making invitation");

    JEUX_PACKET_HEADER in_pkt1, in_pkt2;
    check_packet(fd2, JEUX_INVITED_PKT, SECOND_PLAYER_ROLE, -1, &in_pkt2, NULL);
    int id2 = in_pkt2.id;

    char *str;
    int err = client_accept_invitation(client2, in_pkt2.id, &str);
    cr_assert_eq(err, 0, "Error accepting invitation");

    err = client_resign_game(client1, id1);
    cr_assert_eq(err, 0, "Error resigning game");

    check_packet(fd1, JEUX_ACCEPTED_PKT, 3, id1, &in_pkt1, NULL);
    check_packet(fd1, JEUX_ENDED_PKT, 3, id1, &in_pkt1, NULL);

    check_packet(fd2, JEUX_RESIGNED_PKT, 3, id2, &in_pkt2, NULL);
    check_packet(fd2, JEUX_ENDED_PKT, 3, id2, &in_pkt2, NULL);
}

/*
 * Set up two clients, make an invitation from one to the other,
 * then have the target accept the invitation and then resign.
 * Verify the packets sent.
 */
Test(client_suite, invite_accept_resign_target, .init = init, .timeout = 5) {
#ifdef NO_CLIENT
    cr_assert_fail("Client module was not implemented");
#endif
    char *fname1 = "client_invite_accept_resign_target1.pkts";
    char *fname2 = "client_invite_accept_resign_target2.pkts";
    char *uname1 = "Alice";
    char *uname2 = "Bob";
    int fd1, fd2;
    CLIENT *client1, *client2;
    setup_client(fname1, uname1, &client1, &fd1);
    setup_client(fname2, uname2, &client2, &fd2);
    int id1 = client_make_invitation(client1, client2, FIRST_PLAYER_ROLE, SECOND_PLAYER_ROLE);
    cr_assert_neq(id1, -1, "Error making invitation");

    JEUX_PACKET_HEADER in_pkt1, in_pkt2;
    check_packet(fd2, JEUX_INVITED_PKT, SECOND_PLAYER_ROLE, -1, &in_pkt2, NULL);
    int id2 = in_pkt2.id;

    char *str;
    int err = client_accept_invitation(client2, in_pkt2.id, &str);
    cr_assert_eq(err, 0, "Error accepting invitation");

    err = client_resign_game(client2, id2);
    cr_assert_eq(err, 0, "Error resigning game");

    check_packet(fd1, JEUX_ACCEPTED_PKT, 3, id1, &in_pkt1, NULL);
    check_packet(fd1, JEUX_RESIGNED_PKT, 3, id1, &in_pkt1, NULL);
    check_packet(fd1, JEUX_ENDED_PKT, 3, id1, &in_pkt1, NULL);
    check_packet(fd2, JEUX_ENDED_PKT, 3, id2, &in_pkt2, NULL);
}

/*
 * Set up two clients, make an invitation from one to the other,
 * then have the target accept the invitation and make a game move.
 * Verify the packets sent.
 */
Test(client_suite, invite_accept_move, .init = init, .timeout = 5) {
#ifdef NO_CLIENT
    cr_assert_fail("Client module was not implemented");
#endif
    char *fname1 = "client_invite_accept_move1.pkts";
    char *fname2 = "client_invite_accept_move2.pkts";
    char *uname1 = "Alice";
    char *uname2 = "Bob";
    int fd1, fd2;
    CLIENT *client1, *client2;
    setup_client(fname1, uname1, &client1, &fd1);
    setup_client(fname2, uname2, &client2, &fd2);
    int id1 = client_make_invitation(client1, client2, SECOND_PLAYER_ROLE, FIRST_PLAYER_ROLE);
    cr_assert_neq(id1, -1, "Error making invitation");

    JEUX_PACKET_HEADER in_pkt1, in_pkt2;
    check_packet(fd2, JEUX_INVITED_PKT, FIRST_PLAYER_ROLE, -1, &in_pkt2, NULL);
    int id2 = in_pkt2.id;

    char *str;
    int err = client_accept_invitation(client2, in_pkt2.id, &str);
    cr_assert_eq(err, 0, "Error accepting invitation");

    check_packet(fd1, JEUX_ACCEPTED_PKT, 3, id1, &in_pkt1, NULL);
    assert_no_packets(fd2);

    err = client_make_move(client2, id2, "5<-X");
    cr_assert_eq(err, 0, "Error making move");

    check_packet(fd1, JEUX_MOVED_PKT, 3, id1, &in_pkt1, NULL);
    assert_no_packets(fd2);
}

/*
 * Concurrency test:
 * Set up two clients with a thread for each that for many iterations
 * randomly invites the other client and revokes outstanding invitations.
 * Look for error return from invitation and revocation and crashes due
 * to corrupted lists, etc.
 */

struct random_inviter_args {
    CLIENT *source;
    CLIENT *target;
    int iters;
};

void *random_inviter_thread(void *args) {
    struct random_inviter_args *ap = args;
    unsigned int seed = 1;
    // We don't know what values the student code will use for invitations,
    // so we need a separate array to keep track of which ones are outstanding.
    int outstanding[NINVITATION] = { 0 };
    int ids[NINVITATION] = { 0 };
    int err;
    for(int i = 0; i < ap->iters; i++) {
	int n = rand_r(&seed) % NINVITATION;
	if(!outstanding[n]) {
	    ids[n] = client_make_invitation(ap->source, ap->target,
					    FIRST_PLAYER_ROLE, SECOND_PLAYER_ROLE);
	    cr_assert_neq(ids[n], -1, "Error making invitation");
	    outstanding[n] = 1;
	} else {
	    outstanding[n] = 0;
	    err = client_revoke_invitation(ap->source, ids[n]);
	    cr_assert_eq(err, 0, "Error revoking invitation");
	}
    }
    return NULL;
}

Test(client_suite, random_invite_revoke, .init = init, .timeout = 15) {
#ifdef NO_CLIENT
    cr_assert_fail("Client module was not implemented");
#endif
    char *fname1 = "/dev/null";
    char *fname2 = "/dev/null";
    char *uname1 = "Alice";
    char *uname2 = "Bob";
    int fd1, fd2;
    CLIENT *client1, *client2;
    setup_client(fname1, uname1, &client1, &fd1);
    setup_client(fname2, uname2, &client2, &fd2);
    struct random_inviter_args args1 = {
	.source = client1,
	.target = client2,
	.iters = NITERS
    };
    struct random_inviter_args args2 = {
	.source = client2,
	.target = client1,
	.iters = NITERS
    };
    pthread_t tid1, tid2;
    int err = pthread_create(&tid1, NULL, random_inviter_thread, &args1);
    cr_assert(err >= 0, "Failed to create test thread");
    err = pthread_create(&tid2, NULL, random_inviter_thread, &args2);
    cr_assert(err >= 0, "Failed to create test thread");

    pthread_join(tid1, NULL);
    pthread_join(tid2, NULL);
}
