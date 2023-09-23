//#include <criterion/criterion.h>
//#include <criterion/logging.h>
//#include <pthread.h>
//
//#include "player.h"
//#include "debug.h"
//#include "game.h"
//
//struct pstruct {
//    PLAYER *p1;
//    PLAYER *p2;
//};
//
//void *result_thread(void *p) {
//    struct pstruct ps = *((struct pstruct *)p);
//    PLAYER *p1 = ps.p1;
//    PLAYER *p2 = ps.p2;
//
//    player_post_result(p1, p2, 1);
//
//    return NULL;
//}
//
//Test(player_suite, player_deadlock_test, .timeout = 5) {
//    PLAYER *p1 = player_create("joe");
//    PLAYER *p2 = player_create("moe");
//
//    struct pstruct ps1;
//    struct pstruct ps2;
//
//    ps2.p2 = ps1.p1 = p1;
//    ps2.p1 = ps1.p2 = p2;
//
//    pthread_t tid[2];
//    pthread_create(&tid[0], NULL, result_thread, &ps1);
//    pthread_create(&tid[1], NULL, result_thread, &ps2);
//    pthread_join(tid[0], NULL);
//    pthread_join(tid[1], NULL);
//
//    //int res1 = player_get_rating(p1);
//    //int res2 = player_get_rating(p2);
//    
//    //cr_assert_eq(res1, 1498, "wrong rating! Got %d but expected %d", res1, 1498);
//    //cr_assert_eq(res2, 1501, "wrong rating! Got %d but expected %d", res2, 1501);
//}
//
//Test(game_suite, game_test) {
//    GAME *newgame = game_create(); 
//    game_apply_move(newgame, game_parse_move(newgame, FIRST_PLAYER_ROLE, "1"));
//    game_apply_move(newgame, game_parse_move(newgame, SECOND_PLAYER_ROLE, "4"));
//    game_apply_move(newgame, game_parse_move(newgame, FIRST_PLAYER_ROLE, "2"));
//    game_apply_move(newgame, game_parse_move(newgame, SECOND_PLAYER_ROLE, "7"));
//    game_apply_move(newgame, game_parse_move(newgame, FIRST_PLAYER_ROLE, "3"));
//
//    printf("%s\n", game_unparse_state(newgame));
//    printf("%d\n", game_get_winner(newgame));
//}
