#include <criterion/criterion.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/time.h>

#include "protocol.h"
#include "excludes.h"

static void init() {
}

Test(protocol_suite, send_no_payload, .init = init, .timeout = 5) {
#ifdef NO_PROTOCOL
    cr_assert_fail("Protocol was not implemented");
#endif
    int fd;
    void *payload = NULL;
    JEUX_PACKET_HEADER pkt = {0};

    pkt.type = JEUX_ACK_PKT;
    pkt.size = htons(0);
    pkt.timestamp_sec = htonl(0x11223344);
    pkt.timestamp_nsec = htonl(0x55667788);

    fd = open("pkt_ack_no_payload", O_CREAT|O_TRUNC|O_RDWR, 0644);
    cr_assert(fd > 0, "Failed to create output file");
    int ret = proto_send_packet(fd, &pkt, payload);
    cr_assert_eq(ret, 0, "Returned value %d was not 0", ret);
    close(fd);

    ret = system("cmp pkt_ack_no_payload tests/rsrc/pkt_ack_no_payload");
    cr_assert_eq(ret, 0, "Packet sent did not match expected");
}

Test(protocol_suite, send_with_payload, .init = init, .timeout = 5) {
#ifdef NO_PROTOCOL
    cr_assert_fail("Protocol was not implemented");
#endif

    int fd;
    char *payloadp = "This is a test payload";
    JEUX_PACKET_HEADER pkt = {0};

    pkt.type = JEUX_ACK_PKT;
    pkt.size = htons(strlen(payloadp));
    pkt.timestamp_sec = htonl(0x11223344);
    pkt.timestamp_nsec = htonl(0x55667788);

    fd = open("pkt_ack_with_payload", O_CREAT|O_TRUNC|O_RDWR, 0644);
    cr_assert(fd > 0, "Failed to create output file");
    int ret = proto_send_packet(fd, &pkt, payloadp);
    cr_assert_eq(ret, 0, "Returned value was %d not 0", ret);
    close(fd);

    ret = system("cmp pkt_ack_with_payload tests/rsrc/pkt_ack_with_payload");
    cr_assert_eq(ret, 0, "Packet sent did not match expected");
}

Test(protocol_suite, send_error, .init = init, .timeout = 5) {
#ifdef NO_PROTOCOL
    cr_assert_fail("Protocol was not implemented");
#endif
    int fd;
    void *payload = NULL;
    JEUX_PACKET_HEADER pkt = {0};

    pkt.type = JEUX_ACK_PKT;
    pkt.size = htons(0);
    pkt.timestamp_sec = htonl(0x11223344);
    pkt.timestamp_nsec = htonl(0x55667788);

    fd = open("pkt_ack_no_payload", O_CREAT|O_TRUNC|O_RDWR, 0644);
    cr_assert(fd > 0, "Failed to create output file");
    // Here is the error.
    close(fd);
    int ret = proto_send_packet(fd, &pkt, payload);
    cr_assert_neq(ret, 0, "Returned value was zero", ret);
}

Test(protocol_suite, recv_no_payload, .init = init, .timeout = 5) {
#ifdef NO_PROTOCOL
    cr_assert_fail("Protocol was not implemented");
#endif
    int fd;
    void *payload = NULL;
    JEUX_PACKET_HEADER pkt = {0};

#ifdef CREATE_REFERENCE_FILES
    fd = open("pkt_revoke_no_payload", O_CREAT|O_TRUNC|O_RDWR, 0644);
    pkt.type = JEUX_REVOKE_PKT;
    pkt.id = 0x99;
    pkt.size = htons(0);
    pkt.timestamp_sec = htonl(0x11223344);
    pkt.timestamp_nsec = htonl(0x55667788);
    proto_send_packet(fd, &pkt, NULL);
    close(fd);
    memset(&pkt, 0, sizeof(pkt));
#endif

    fd = open("tests/rsrc/pkt_revoke_no_payload", O_RDONLY, 0);
    cr_assert(fd > 0, "Failed to open test input file");
    int ret = proto_recv_packet(fd, &pkt, &payload);
    cr_assert_eq(ret, 0, "Returned value was not 0");
    close(fd);

    cr_assert_eq(pkt.type, JEUX_REVOKE_PKT, "Received packet type %d did not match expected %d",
		 pkt.type, JEUX_REVOKE_PKT);
    cr_assert_eq(ntohs(pkt.size), 0, "Received payload size was %u not zero", ntohs(pkt.size));
    cr_assert_eq(ntohl(pkt.timestamp_sec), 0x11223344,
		 "Received message timestamp_sec 0x%x did not match expected 0x%x",
		 ntohl(pkt.timestamp_sec), 0x11223344);
    cr_assert_eq(ntohl(pkt.timestamp_nsec), 0x55667788,
		 "Received message timestamp_nsec 0x%x did not match expected 0x%x",
		 ntohl(pkt.timestamp_nsec), 0x55667788);
}

Test(protocol_suite, recv_with_payload, .init = init, .timeout = 5) {
#ifdef NO_PROTOCOL
    cr_assert_fail("Protocol was not implemented");
#endif
    int fd;
    char *exp_payload = "This_is_a_long_user_name";
    int exp_size = strlen(exp_payload);
    void *payload = NULL;
    JEUX_PACKET_HEADER pkt = {0};

#ifdef CREATE_REFERENCE_FILES
    fd = open("pkt_login", O_CREAT|O_TRUNC|O_RDWR, 0644);
    pkt.type = JEUX_LOGIN_PKT;
    pkt.size = htons(exp_size);
    pkt.timestamp_sec = htonl(0x11223344);
    pkt.timestamp_nsec = htonl(0x55667788);
    proto_send_packet(fd, &pkt, exp_payload);
    close(fd);
    memset(&pkt, 0, sizeof(pkt));
#endif

    fd = open("tests/rsrc/pkt_login", O_RDONLY, 0);
    cr_assert(fd > 0, "Failed to open test input file");
    int ret = proto_recv_packet(fd, &pkt, &payload);
    cr_assert_eq(ret, 0, "Returned value was not 0");
    close(fd);

    cr_assert_eq(pkt.type, JEUX_LOGIN_PKT, "Received packet type %d did not match expected %d",
		 pkt.type, JEUX_LOGIN_PKT);
    cr_assert_eq(ntohs(pkt.size), exp_size, "Received payload size was %u not %u", ntohs(pkt.size),
		 exp_size);
    cr_assert_eq(ntohl(pkt.timestamp_sec), 0x11223344,
		 "Received message timestamp_sec 0x%x did not match expected 0x%x",
		 ntohl(pkt.timestamp_sec), 0x11223344);
    cr_assert_eq(ntohl(pkt.timestamp_nsec), 0x55667788,
		 "Received message timestamp_nsec 0x%x did not match expected 0x%x",
		 ntohl(pkt.timestamp_nsec), 0x55667788);
    int n = strncmp(payload, exp_payload, exp_size);
    cr_assert_eq(n, 0, "Received message payload did not match expected");
}

Test(protocol_suite, recv_empty, .init = init, .timeout = 5) {
#ifdef NO_PROTOCOL
    cr_assert_fail("Protocol was not implemented");
#endif
    int fd;
    void *payload = NULL;
    JEUX_PACKET_HEADER pkt = {0};

    fd = open("tests/rsrc/pkt_empty", O_RDONLY, 0);
    cr_assert(fd > 0, "Failed to open test input file");
    int ret = proto_recv_packet(fd, &pkt, &payload);
    cr_assert_neq(ret, 0, "Returned value was 0");
    close(fd);
}

Test(protocol_suite, recv_short_header, .init = init, .timeout = 5) {
#ifdef NO_PROTOCOL
    cr_assert_fail("Protocol was not implemented");
#endif
    int fd;
    void *payload = NULL;
    JEUX_PACKET_HEADER pkt = {0};

    fd = open("tests/rsrc/pkt_short_header", O_RDONLY, 0);
    cr_assert(fd > 0, "Failed to open test input file");
    int ret = proto_recv_packet(fd, &pkt, &payload);
    cr_assert_neq(ret, 0, "Returned value was 0");
    close(fd);
}

Test(protocol_suite, recv_short_payload, .init = init, .signal = SIGALRM) {
#ifdef NO_PROTOCOL
    cr_assert_fail("Protocol was not implemented");
#endif
    int fd;
    void *payload = NULL;
    JEUX_PACKET_HEADER pkt = {0};
    struct itimerval itv = {{0, 0}, {1, 0}};

    fd = open("tests/rsrc/pkt_short_payload", O_RDONLY, 0);
    cr_assert(fd > 0, "Failed to open test input file");
    // On a network connection, reading will block until the specified
    // amount of payload has been received.  So we have to set an alarm
    // to terminate the test.  Because we are reading from a file here,
    // the underlying read() should return 0, indicating EOF, which
    // proto_recv_packet() should detect and set errno != EINTR.
    // In that case, we have to generate the expected signal manually.
    setitimer(ITIMER_REAL, &itv, NULL);
    int ret = proto_recv_packet(fd, &pkt, &payload);
    cr_assert_neq(ret, 0, "Returned value was 0");
    if(errno != EINTR)
	kill(getpid(), SIGALRM);
    close(fd);
}

Test(protocol_suite, recv_error, .init = init, .timeout = 5) {
#ifdef NO_PROTOCOL
    cr_assert_fail("Protocol was not implemented");
#endif
    int fd;
    void *payload = NULL;
    JEUX_PACKET_HEADER pkt = {0};

    fd = open("tests/rsrc/pkt_empty", O_RDONLY, 0);
    cr_assert(fd > 0, "Failed to open test input file");
    close(fd);
    int ret = proto_recv_packet(fd, &pkt, &payload);
    cr_assert_neq(ret, 0, "Returned value was zero");
}
