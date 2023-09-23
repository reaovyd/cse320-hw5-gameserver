#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <ctype.h>

#include "debug.h"
#include "csapp.h"
#include "protocol.h"
#include "server.h"
#include "client_registry.h"
#include "player_registry.h"
#include "jeux_globals.h"

#ifdef DEBUG
int _debug_packets_ = 1;
#endif

static volatile sig_atomic_t terminate_flag = 0;
static pthread_t MAIN_THREAD_FLAG;
static int listenfd;

static void terminate(int status);

static void sighup_handler(int signum) {
    // TODO use a flag instead this is tsupid
    //terminate(EXIT_SUCCESS);
    terminate_flag = 1;
    if(pthread_self() != MAIN_THREAD_FLAG) {
        pthread_kill(MAIN_THREAD_FLAG, SIGHUP); 
    } else {
        close(listenfd);
    }
}

static void set_signals() {
    struct sigaction sa;
    memset(&sa, 0x0, sizeof(sa));

    sa.sa_handler = sighup_handler;
    sigaction(SIGHUP, &sa, NULL);

    sa.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sa, NULL);
}

static void print_usage_exit(char *prog) {
    fprintf(stderr, "Usage: %s -p <port>\n", prog);
    exit(EXIT_FAILURE);
}

static void run_server(char *port) {
    int *newfd;

    listenfd = open_listenfd(port);

    if(listenfd < 0) {
        terminate(EXIT_FAILURE);
    }
    debug("%ld: Jeux server listening on port %s", pthread_self(), port);

    struct sockaddr s_addr;
    socklen_t sl;

    MAIN_THREAD_FLAG = pthread_self();

    do {
        sl = sizeof(s_addr);
        newfd = malloc(sizeof(int));
        if(newfd == NULL)
            terminate(EXIT_FAILURE);
        if(terminate_flag) {
            free(newfd);
            break;
        }
        *newfd = accept(listenfd, &s_addr, &sl); 
        if(terminate_flag) {
            if(*newfd != -1)
                close(*newfd);
            free(newfd);
            break;
        }
        pthread_t ptt;
        pthread_create(&ptt, NULL, jeux_client_service, newfd);
    } while(!terminate_flag);

    terminate(EXIT_SUCCESS);
}

static int char_to_port_num(char *str) {
    if(str == NULL) {
        return -1;
    }
    int ret = 0;
    while(*str != '\0') {
        if(!isdigit(*str)) {
            return -1;
        }
        ret *= 10;
        ret += (*str - '0');
        str++;
    }
    return ret;
}

/*
 * "Jeux" game server.
 *
 * Usage: jeux <port>
 */
int main(int argc, char* argv[]){
    // Option processing should be performed here.
    // Option '-p <port>' is required in order to specify the port number
    // on which the server should listen.

    // Perform required initializations of the client_registry and
    // player_registry.
    if(argc != 3) {
        print_usage_exit(argv[0]);
    }
    if(strcmp(argv[1], "-p") != 0) {
        print_usage_exit(argv[0]);
    }

    set_signals();
    int port = char_to_port_num(argv[2]); 
    if(port == -1) {
        print_usage_exit(argv[0]);
    }
    client_registry = creg_init();
    player_registry = preg_init();

    // TODO: Set up the server socket and enter a loop to accept connections
    // on this socket.  For each connection, a thread should be started to
    // run function jeux_client_service().  In addition, you should install
    // a SIGHUP handler, so that receipt of SIGHUP will perform a clean
    // shutdown of the server.
    //while(1) { }
    run_server(argv[2]);
}

/*
 * Function called to cleanly shut down the server.
 */
void terminate(int status) {
    // Shutdown all client connections.
    // This will trigger the eventual termination of service threads.
    creg_shutdown_all(client_registry);
    
    debug("%ld: Waiting for service threads to terminate...", pthread_self());
    creg_wait_for_empty(client_registry);
    debug("%ld: All service threads terminated.", pthread_self());

    // Finalize modules.
    creg_fini(client_registry);
    preg_fini(player_registry);

    debug("%ld: Jeux server terminating", pthread_self());
    exit(status);
}
