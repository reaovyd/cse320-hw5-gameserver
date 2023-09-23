//#include <criterion/criterion.h>
//#include <fcntl.h>
//#include <pthread.h>
//#include <stdio.h>
//#include <unistd.h>
//#include <fcntl.h>
//#include <signal.h>
//#include <wait.h>
//#define TEST_OUTPUT "test_output/"
//#define SERVER_RUNNING_TIME 1800 
//
//static void init() {
//#ifndef NO_SERVER
//    int ret;
//    int i = 0;
//    do { // Wait for server to start
//	ret = system("netstat -an | fgrep '0.0.0.0:9999' > /dev/null");
//	sleep(1);
//    } while(++i < 30 && WEXITSTATUS(ret));
//#endif
//}
//static void fini() {
//}
//
////static void *system_thread(void *arg) {
////    long ret = system((char *)arg);
////    return (void *)ret;
////}
////
////Test(jeux_tests_mine_suite, jeux_test_1) {
////    int server_pid = 0;
////    fprintf(stderr, "Starting server...");
////    if((server_pid = fork()) == 0) {
////        execlp("valgrind", "jeux", "--leak-check=full", "--track-fds=yes",
////               "--error-exitcode=37", "--log-file="TEST_OUTPUT"valgrind.out", "bin/jeux", "-p", "9999", NULL);
////        fprintf(stderr, "Failed to exec server\n");
////        abort();
////    }
////    sleep(SERVER_RUNNING_TIME);
////
////    //char *cmd = "sleep 10";
////    //pthread_t tid;
////    //pthread_create(&tid, NULL, system_thread, cmd);
////    //pthread_join(tid, NULL);
////    kill(server_pid, SIGHUP);
////    sleep(3);
////}
//
//Test(jeux_tests_mine_suite, 01_connect, .init = init, .fini = fini, .timeout = SERVER_RUNNING_TIME + 5) {
//    pid_t status[10];
//    for(int i = 0; i < 10; ++i) {
//        pid_t status_value;
//        if((status_value = fork()) == 0) {
//            char *argv[] = {"./util/jclient", "-p", "9999", NULL};
//            execvp("./util/jclient", argv);
//        } else {
//            status[i] = status_value;
//        }
//    }
//    sleep(6);
//    for(int i = 0; i < 10; ++i) {
//        kill(status[i], SIGTERM);
//    }
//    pid_t pid;
//    int value = 0;
//    while((pid = waitpid(-1, NULL, 0)) > 0) {
//        value++;
//    }
//    cr_assert_eq(value, 10, "Did not find 10 connected clients!");
//}
//
//Test(jeux_tests_mine_suite, 02_connect_write, .init = init, .fini = fini, .timeout = SERVER_RUNNING_TIME + 5) {
//    pid_t status[5];
//    int pipes[5][2];
//    int pipes2[5][2];
//    for(int i = 0; i < 5; ++i) {
//        pipe(pipes[i]); 
//        pipe(pipes2[i]);
//
//        pid_t status_value;
//        if((status_value = fork()) == 0) {
//            close(pipes[i][1]);
//            close(pipes2[i][0]);
//            close(STDERR_FILENO);
//
//            dup2(pipes[i][0], STDIN_FILENO);
//            close(pipes[i][0]);
//
//            dup2(pipes2[i][1], STDOUT_FILENO);
//            close(pipes2[i][1]);
//
//            char *argv[] = {"./util/jclient", "-p", "9999", NULL};
//            execvp("./util/jclient", argv);
//        } else {
//            status[i] = status_value;
//            fcntl(pipes2[i][0], F_SETFL, O_ASYNC | O_NONBLOCK);
//            close(pipes[i][0]);
//            close(pipes2[i][1]);
//        }
//    }
//    sleep(5);
//    write(pipes[0][1], "login joe\n", 10);
//    write(pipes[1][1], "login aoe\n", 10);
//    write(pipes[2][1], "login doe\n", 10);
//    write(pipes[3][1], "login moe\n", 10);
//    write(pipes[4][1], "login poe\n", 10);
//    sleep(5);
//    char *buf; size_t sz;
//
//    FILE *fstream = open_memstream(&buf, &sz);
//    for(int i = 0; i < 5; ++i) {
//        char c;
//        ssize_t n_read;
//        while(1) {
//            n_read = read(pipes2[i][0], &c, 1);
//            if(n_read <= 0) {
//                break;
//            }
//            fputc(c, fstream);
//        }
//    }
//    fclose(fstream);
//    int co = 0;
//    for(int i = 1; i < sz; ++i) {
//        if(buf[i] == 'K' && buf[i - 1] == 'O') {
//            co++;
//        }
//    }
//
//    for(int i = 0; i < 5; ++i) {
//        kill(status[i], SIGTERM);
//    }
//    sleep(3);
//    cr_assert_eq(co, 5, "DID NOT FIND 5 OKS");
//}
//
//typedef struct i_fd_t {
//    int i;
//    int fd;
//} i_fd_t;
//
//void *run_multiple_cmd_invite(void *fd_ptr) {
//    char *names[9] = {"2joe", "2aoe", "2doe", "2moe", "2poe", "2gro", "2foe", "2loe", "2eal"};
//    i_fd_t ifd = *(i_fd_t *)fd_ptr;
//    for(int i = 0; i < 9; ++i) {
//        if(i != ifd.i) {
//            dprintf(ifd.fd, "invite %s 1\n", names[i]);
//        }
//    }
//    return NULL;
//}
//
//Test(jeux_tests_mine_suite, 03_connect_write, .init = init, .fini = fini, .timeout = SERVER_RUNNING_TIME + 5) {
//
//    pid_t status[9];
//    int pipes[9][2];
//    int pipes2[9][2];
//    for(int i = 0; i < 9; ++i) {
//        pipe(pipes[i]); 
//        pipe(pipes2[i]);
//
//        pid_t status_value;
//        if((status_value = fork()) == 0) {
//            close(pipes[i][1]);
//            close(pipes2[i][0]);
//            close(STDERR_FILENO);
//
//            dup2(pipes[i][0], STDIN_FILENO);
//            close(pipes[i][0]);
//
//            dup2(pipes2[i][1], STDOUT_FILENO);
//            close(pipes2[i][1]);
//
//            char *argv[] = {"./util/jclient", "-p", "9999", NULL};
//            execvp("./util/jclient", argv);
//        } else {
//            status[i] = status_value;
//            fcntl(pipes2[i][0], F_SETFL, O_ASYNC | O_NONBLOCK);
//            close(pipes[i][0]);
//            close(pipes2[i][1]);
//        }
//    }
//    char *names[9] = {"2joe", "2aoe", "2doe", "2moe", "2poe", "2gro", "2foe", "2loe", "2eal"};
//    sleep(5);
//    for(int i = 0; i < 9; ++i) {
//        dprintf(pipes[i][1], "login %s\n", names[i]);
//        sleep(1);
//    }
//    sleep(5);
//    char *buf; size_t sz;
//
//    FILE *fstream = open_memstream(&buf, &sz);
//    for(int i = 0; i < 9; ++i) {
//        char c;
//        ssize_t n_read;
//        while(1) {
//            n_read = read(pipes2[i][0], &c, 1);
//            if(n_read <= 0) {
//                break;
//            }
//            fputc(c, fstream);
//        }
//    }
//    fclose(fstream);
//    int co = 0;
//    for(int i = 1; i < sz; ++i) {
//        if(buf[i] == 'K' && buf[i - 1] == 'O') {
//            co++;
//        }
//    }
//    cr_assert_eq(co, 9, "DID NOT FIND 9 OKS");
//
//    pthread_t tids[9];
//
//    for(int i = 0; i < 9; ++i) {
//        i_fd_t *ifd = malloc(sizeof(i_fd_t));
//        ifd->i = i;
//        ifd->fd = pipes[i][1];
//
//        pthread_create(&tids[i], NULL, run_multiple_cmd_invite, ifd);
//    }
//    for(int i = 0; i < 9; ++i) {
//        pthread_join(tids[i], NULL);
//    }
//    sleep(5);
//
//    for(int i = 0; i < 9; ++i) {
//        kill(status[i], SIGTERM);
//    }
//    sleep(5);
//}
//
//
//void *run_multiple_cmd_accept(void *fd_ptr) {
//    int fd = *(int *)fd_ptr; 
//    for(int i = 0; i < 37; ++i) {
//        dprintf(fd, "accept %d\n", i);
//        sleep(1);
//    }
//
//    return NULL;
//}
//
//Test(jeux_tests_mine_suite, 04_connect_write_accept, .init = init, .fini = fini, .timeout = SERVER_RUNNING_TIME + 5) {
//
//    pid_t status[9];
//    int pipes[9][2];
//    int pipes2[9][2];
//    for(int i = 0; i < 9; ++i) {
//        pipe(pipes[i]); 
//        pipe(pipes2[i]);
//
//        pid_t status_value;
//        if((status_value = fork()) == 0) {
//            close(pipes[i][1]);
//            close(pipes2[i][0]);
//
//            dup2(pipes[i][0], STDIN_FILENO);
//            close(pipes[i][0]);
//
//            dup2(pipes2[i][1], STDOUT_FILENO);
//            close(pipes2[i][1]);
//
//            char *argv[] = {"./util/jclient", "-p", "9999", NULL};
//            execvp("./util/jclient", argv);
//        } else {
//            status[i] = status_value;
//            fcntl(pipes2[i][0], F_SETFL, O_ASYNC | O_NONBLOCK);
//            close(pipes[i][0]);
//            close(pipes2[i][1]);
//        }
//    }
//    char *names[9] = {"2joe", "2aoe", "2doe", "2moe", "2poe", "2gro", "2foe", "2loe", "2eal"};
//    sleep(5);
//    for(int i = 0; i < 9; ++i) {
//        dprintf(pipes[i][1], "login %s\n", names[i]);
//    }
//    sleep(5);
//    char *buf; size_t sz;
//
//    FILE *fstream = open_memstream(&buf, &sz);
//    for(int i = 0; i < 9; ++i) {
//        char c;
//        ssize_t n_read;
//        while(1) {
//            n_read = read(pipes2[i][0], &c, 1);
//            if(n_read <= 0) {
//                break;
//            }
//            fputc(c, fstream);
//        }
//    }
//    fclose(fstream);
//    int co = 0;
//    for(int i = 1; i < sz; ++i) {
//        if(buf[i] == 'K' && buf[i - 1] == 'O') {
//            co++;
//        }
//    }
//    cr_assert_eq(co, 9, "DID NOT FIND 9 OKS");
//
//    pthread_t tids[9];
//    for(int i = 0; i < 9; ++i) {
//        i_fd_t *ifd = malloc(sizeof(i_fd_t));
//        ifd->i = i;
//        ifd->fd = pipes[i][1];
//
//        pthread_create(&tids[i], NULL, run_multiple_cmd_invite, ifd);
//    }
//    sleep(5);
//    pthread_t tid_arr[9];
//    for(int i = 0; i < 9; ++i) {
//        pthread_create(&tid_arr[i], NULL, run_multiple_cmd_accept, &pipes[i][1]);
//    }
//    for(int i = 0; i < 9; ++i) {
//        pthread_join(tid_arr[i], NULL);
//    }
//
//    sleep(5);
//    char *buf2; size_t sz2;
//    FILE *fstream2 = open_memstream(&buf2, &sz2);
//    for(int i = 0; i < 9; ++i) {
//        char c;
//        ssize_t n_read;
//        while(1) {
//            n_read = read(pipes2[i][0], &c, 1);
//            if(n_read <= 0) {
//                break;
//            }
//            fputc(c, fstream2);
//        }
//    }
//    co = 0;
//    fclose(fstream2);
//    for(int i = 1; i < sz2; ++i) {
//        if(buf2[i] == 'K' && buf2[i - 1] == 'O') {
//            co++;
//        }
//    }
//    cr_assert_eq(co, 72, "Did not find correct count of OK. Found %d", co);
//
//
//    sleep(5);
//
//    for(int i = 0; i < 9; ++i) {
//        kill(status[i], SIGTERM);
//    }
//    sleep(5);
//}
