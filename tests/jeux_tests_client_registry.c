//#include <criterion/criterion.h>
//#include <criterion/logging.h>
//#include <pthread.h>
//#include <stdio.h>
//#include <unistd.h>
//#include <fcntl.h>
//#include <signal.h>
//#include <wait.h>
//#include <semaphore.h>
//#include "client_registry.h"
//#include "jeux_globals.h"
//#include "client.h"
//#include "debug.h"
//
//struct client_registry {
//    CLIENT *creg_arr[MAX_CLIENTS];
//    pthread_mutex_t mutex;
//    sem_t count_sem;
//    size_t len;
//    size_t cap;
//};
//
//typedef struct registry_fd {
//    CLIENT_REGISTRY *cr; 
//    int fd;
//    CLIENT *cli;
//} registry_fd;
//
//Test(client_registry_suite, client_init) {
//    CLIENT_REGISTRY *cr = creg_init(); 
//    cr_assert_not_null(cr, "initialization failed!");
//
//    for(int i = 0; i < MAX_CLIENTS; ++i) {
//        cr_assert_null(cr->creg_arr[i], "must be null!");
//    }
//    cr_assert_eq(cr->len, 0, "must be 0");
//    cr_assert_eq(cr->cap, MAX_CLIENTS, "must be MAX_CLIENTS: %lu", MAX_CLIENTS);
//}
//
//Test(client_registry_suite, client_register_1) {
//    CLIENT_REGISTRY *cr = creg_init(); 
//    CLIENT *new_client = creg_register(cr, 3); 
//    cr_assert_not_null(new_client);
//    cr_assert_eq(cr->len, 1, "must be single elem");
//}
//
//void *new_thread_register(void *arg) {
//    registry_fd rfd = *((registry_fd *)arg);
//    CLIENT_REGISTRY *cr = rfd.cr;
//    int fd = rfd.fd;
//
//    creg_register(cr, fd);
//
//    return NULL;
//}
//
//Test(client_registry_suite, client_register_2) {
//    registry_fd rfd1, rfd2, rfd3; 
//    rfd3.cr = rfd2.cr = rfd1.cr = creg_init();
//
//    rfd1.fd = 3;
//    rfd2.fd = 4;
//    rfd3.fd = 5;
//
//    pthread_t tid1, tid2, tid3;
//    pthread_create(&tid1, NULL, new_thread_register, &rfd1);
//    pthread_create(&tid2, NULL, new_thread_register, &rfd2);
//    pthread_create(&tid3, NULL, new_thread_register, &rfd3);
//
//    pthread_join(tid1, NULL);
//    pthread_join(tid2, NULL);
//    pthread_join(tid3, NULL);
//
//    cr_assert_eq(rfd1.cr->len, 3, "length not valid!\n");
//}
//
//void *new_thread_register_1(void *arg) {
//    registry_fd rfd = *((registry_fd *)arg);
//    CLIENT_REGISTRY *cr = rfd.cr;
//    int fd = rfd.fd;
//
//    ((registry_fd *)arg)->cli = creg_register(cr, fd);
//
//    return NULL;
//}
//
//void *new_thread_unregister_1(void *arg) {
//    registry_fd rfd = *((registry_fd *)arg);
//    CLIENT_REGISTRY *cr = rfd.cr;
//
//    creg_unregister(cr, rfd.cli);
//
//    return NULL;
//}
//
//Test(client_registry_suite, client_register_unregister_1) {
//
//    registry_fd rfd1, rfd2, rfd3; 
//    rfd3.cr = rfd2.cr = rfd1.cr = creg_init();
//
//    rfd1.fd = open("./test_input/new_file", O_CREAT | O_RDWR);
//    rfd2.fd = open("./test_input/new_file", O_CREAT | O_RDWR);
//    rfd3.fd = open("./test_input/new_file", O_CREAT | O_RDWR);
//
//    pthread_t tid1, tid2, tid3;
//    pthread_create(&tid1, NULL, new_thread_register_1, &rfd1);
//    pthread_create(&tid2, NULL, new_thread_register_1, &rfd2);
//    pthread_create(&tid3, NULL, new_thread_register_1, &rfd3);
//
//    pthread_join(tid1, NULL);
//    pthread_join(tid2, NULL);
//    pthread_join(tid3, NULL);
//
//    pthread_t tid11, tid22, tid33;
//
//    pthread_create(&tid11, NULL, new_thread_unregister_1, &rfd1);
//    pthread_create(&tid22, NULL, new_thread_unregister_1, &rfd2);
//    pthread_create(&tid33, NULL, new_thread_unregister_1, &rfd3);
//
//    pthread_join(tid11, NULL);
//    pthread_join(tid22, NULL);
//    pthread_join(tid33, NULL);
//    cr_assert_eq(rfd1.cr->len, 0, "length not valid!\n");
//}
//
//void *new_thread_register_more_thread(void *arg) {
//    pthread_detach(pthread_self());
//    registry_fd rfd = *((registry_fd *)arg);
//    CLIENT_REGISTRY *cr = rfd.cr;
//    int fd = rfd.fd;
//
//    ((registry_fd *)arg)->cli = creg_register(cr, fd);
//    sleep(3);
//    creg_unregister(cr, ((registry_fd *)arg)->cli);
//
//
//    return NULL;
//}
//
//Test(client_registry_suite, client_register_unregister_2, .timeout=10) {
//    CLIENT_REGISTRY *cr = creg_init();
//
//    registry_fd rfd_arr[20] = {0};
//    for(int fd = 3; fd < 23; fd++) {
//        rfd_arr[fd - 3].cr = cr; 
//        rfd_arr[fd - 3].fd = open("./tests/test_input/new_file", O_CREAT | O_RDWR); 
//    }
//
//    pthread_t tids[20];
//    for(int rfd = 0; rfd < 20; rfd++) {
//        pthread_create(&tids[rfd], NULL, new_thread_register_more_thread, &rfd_arr[rfd]);
//    }
//    creg_wait_for_empty(cr);
//    creg_fini(cr);
//}
