#include <criterion/criterion.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "debug.h"
#include "game.h"
#include "excludes.h"

/* Number of games played in the concurrency test. */
#define NGAMES (1000)

/* Maximum number of moves attempted by a thread. */
#define NMOVES (100)

/* Number of threads we create in multithreaded tests. */
#define NTHREAD (10)

/*
 * Create a game and check some things about its initial state.
 */
Test(game_suite, create, .timeout = 5) {
#ifdef NO_GAME
    cr_assert_fail("Game module was not implemented");
#endif
    GAME *game = game_create();
    cr_assert_not_null(game, "Returned value was NULL");
    cr_assert(!game_is_over(game), "Newly created game should not be over yet");
    cr_assert_eq(game_get_winner(game), NULL_ROLE, "Newly created game should not have a winner");
}

/*
 * Create a game and apply a few legal moves to it.
 */
Test(game_suite, legal_moves, .timeout = 5) {
#ifdef NO_GAME
    cr_assert_fail("Game module was not implemented");
#endif
    GAME *game = game_create();
    cr_assert_not_null(game, "Returned value was NULL");

    GAME_MOVE *move = game_parse_move(game, FIRST_PLAYER_ROLE, "5");
    cr_assert_not_null(move, "Returned move was NULL");
    int err = game_apply_move(game, move);
    cr_assert_eq(err, 0, "Returned value was not 0");

    move = game_parse_move(game, SECOND_PLAYER_ROLE, "1");
    cr_assert_not_null(move, "Returned move was NULL");
    err = game_apply_move(game, move);
    cr_assert_eq(err, 0, "Returned value was not 0");

    move = game_parse_move(game, FIRST_PLAYER_ROLE, "2");
    cr_assert_not_null(move, "Returned move was NULL");
    err = game_apply_move(game, move);
    cr_assert_eq(err, 0, "Returned value was not 0");

    cr_assert(!game_is_over(game), "Game should not be over yet");
    cr_assert_eq(game_get_winner(game), NULL_ROLE, "Game should not have a winner");
}

/*
 * Create a game and resign it.
 */
Test(game_suite, resign, .timeout = 5) {
#ifdef NO_GAME
    cr_assert_fail("Game module was not implemented");
#endif
    GAME *game = game_create();
    cr_assert_not_null(game, "Returned value was NULL");
    int err = game_resign(game, SECOND_PLAYER_ROLE);
    cr_assert_eq(err, 0, "Returned value was not 0");

    cr_assert(game_is_over(game), "Resigned game should be over");
    int winner = game_get_winner(game);
    cr_assert_eq(winner, FIRST_PLAYER_ROLE,
		 "Game winner (%d) does not match expected (%d)",
		 winner, FIRST_PLAYER_ROLE);
}

/*
 * Create a game and apply an illegal move sequence to it.
 */
Test(game_suite, illegal_move, .timeout = 5) {
#ifdef NO_GAME
    cr_assert_fail("Game module was not implemented");
#endif
    GAME *game = game_create();
    cr_assert_not_null(game, "Returned value was NULL");

    GAME_MOVE *move = game_parse_move(game, FIRST_PLAYER_ROLE, "5");
    cr_assert_not_null(move, "Returned move was NULL");
    int err = game_apply_move(game, move);
    cr_assert_eq(err, 0, "Returned value was not 0");

    move = game_parse_move(game, SECOND_PLAYER_ROLE, "1");
    cr_assert_not_null(move, "Returned move was NULL");
    err = game_apply_move(game, move);
    cr_assert_eq(err, 0, "Returned value was not 0");

    move = game_parse_move(game, FIRST_PLAYER_ROLE, "1");
    cr_assert_not_null(move, "Returned move was NULL");
    err = game_apply_move(game, move);
    cr_assert_eq(err, -1, "Returned value was not -1");

    cr_assert(!game_is_over(game), "Game should not be over yet");
    cr_assert_eq(game_get_winner(game), NULL_ROLE, "Game should not have a winner");
}

/*
 * Create a game, apply some moves, and then try to parse a move for
 * the wrong player.
 */
Test(game_suite, parse_move_wrong_player, .timeout = 5) {
#ifdef NO_GAME
    cr_assert_fail("Game module was not implemented");
#endif
    GAME *game = game_create();
    cr_assert_not_null(game, "Returned value was NULL");

    GAME_MOVE *move = game_parse_move(game, FIRST_PLAYER_ROLE, "5");
    cr_assert_not_null(move, "Returned move was NULL");
    int err = game_apply_move(game, move);
    cr_assert_eq(err, 0, "Returned value was not 0");

    move = game_parse_move(game, SECOND_PLAYER_ROLE, "1");
    cr_assert_not_null(move, "Returned move was NULL");
    err = game_apply_move(game, move);
    cr_assert_eq(err, 0, "Returned value was not 0");

    move = game_parse_move(game, FIRST_PLAYER_ROLE, "2");
    cr_assert_not_null(move, "Returned move was NULL");
    err = game_apply_move(game, move);
    cr_assert_eq(err, 0, "Returned value was not 0");

    move = game_parse_move(game, FIRST_PLAYER_ROLE, "3");
    cr_assert_null(move, "Returned move was not NULL");

    cr_assert(!game_is_over(game), "Game should not be over yet");
    cr_assert_eq(game_get_winner(game), NULL_ROLE, "Game should not have a winner");
}

/*
 * Create a game, try parsing and unparsing some moves.
 */
Test(game_suite, parse_unparse_move, .timeout = 5) {
#ifdef NO_GAME
    cr_assert_fail("Game module was not implemented");
#endif
    GAME *game = game_create();
    cr_assert_not_null(game, "Returned value was NULL");

    GAME_MOVE *move = game_parse_move(game, FIRST_PLAYER_ROLE, "5");
    cr_assert_not_null(move, "Returned move was NULL");
    char *str = game_unparse_move(move);
    cr_assert_not_null(str, "Returned string was NULL");
    char *exp = "5<-X";
    cr_assert(!strcmp(str, exp), "Unparsed move (%s) did not match expected (%s)",
	      str, exp);

    move = game_parse_move(game, NULL_ROLE, "5");
    cr_assert_not_null(move, "Returned move was NULL");
    str = game_unparse_move(move);
    cr_assert_not_null(str, "Returned string was NULL");
    exp = "5<-X";
    cr_assert(!strcmp(str, exp), "Unparsed move (%s) did not match expected (%s)",
	      str, exp);

    move = game_parse_move(game, NULL_ROLE, "5<-X");
    cr_assert_not_null(move, "Returned move was NULL");
    str = game_unparse_move(move);
    cr_assert_not_null(str, "Returned string was NULL");
    exp = "5<-X";
    cr_assert(!strcmp(str, exp), "Unparsed move (%s) did not match expected (%s)",
	      str, exp);

    move = game_parse_move(game, NULL_ROLE, "5<-O");
    cr_assert_null(move, "Returned value was not NULL");
}

/*
 * Create a game and apply moves to it to reach a won position.
 */
Test(game_suite, winning_sequence, .timeout = 5) {
#ifdef NO_GAME
    cr_assert_fail("Game module was not implemented");
#endif
    GAME *game = game_create();
    cr_assert_not_null(game, "Returned value was NULL");

    GAME_MOVE *move = game_parse_move(game, FIRST_PLAYER_ROLE, "5");
    cr_assert_not_null(move, "Returned move was NULL");
    int err = game_apply_move(game, move);
    cr_assert_eq(err, 0, "Returned value was not 0");

    move = game_parse_move(game, SECOND_PLAYER_ROLE, "1");
    cr_assert_not_null(move, "Returned move was NULL");
    err = game_apply_move(game, move);
    cr_assert_eq(err, 0, "Returned value was not 0");

    move = game_parse_move(game, FIRST_PLAYER_ROLE, "3");
    cr_assert_not_null(move, "Returned move was NULL");
    err = game_apply_move(game, move);
    cr_assert_eq(err, 0, "Returned value was not 0");

    move = game_parse_move(game, SECOND_PLAYER_ROLE, "2");
    cr_assert_not_null(move, "Returned move was NULL");
    err = game_apply_move(game, move);
    cr_assert_eq(err, 0, "Returned value was not 0");

    move = game_parse_move(game, FIRST_PLAYER_ROLE, "7");
    cr_assert_not_null(move, "Returned move was NULL");
    err = game_apply_move(game, move);
    cr_assert_eq(err, 0, "Returned value was not 0");

    cr_assert(game_is_over(game), "Game should be over");
    cr_assert_neq(game_get_winner(game), NULL_ROLE, "Game should have a winner");

    move = game_parse_move(game, SECOND_PLAYER_ROLE, "4");
    cr_assert_not_null(move, "Returned move was NULL");
    err = game_apply_move(game, move);
    cr_assert_eq(err, -1, "Returned value was not -1");
}

/*
 * Create a game and apply moves to it to reach a drawn position.
 */
Test(game_suite, drawing_sequence, .timeout = 5) {
#ifdef NO_GAME
    cr_assert_fail("Game module was not implemented");
#endif
    GAME *game = game_create();
    cr_assert_not_null(game, "Returned value was NULL");

    GAME_MOVE *move = game_parse_move(game, FIRST_PLAYER_ROLE, "5");
    cr_assert_not_null(move, "Returned move was NULL");
    int err = game_apply_move(game, move);
    cr_assert_eq(err, 0, "Returned value was not 0");

    move = game_parse_move(game, SECOND_PLAYER_ROLE, "1");
    cr_assert_not_null(move, "Returned move was NULL");
    err = game_apply_move(game, move);
    cr_assert_eq(err, 0, "Returned value was not 0");

    move = game_parse_move(game, FIRST_PLAYER_ROLE, "2");
    cr_assert_not_null(move, "Returned move was NULL");
    err = game_apply_move(game, move);
    cr_assert_eq(err, 0, "Returned value was not 0");

    move = game_parse_move(game, SECOND_PLAYER_ROLE, "3");
    cr_assert_not_null(move, "Returned move was NULL");
    err = game_apply_move(game, move);
    cr_assert_eq(err, 0, "Returned value was not 0");

    move = game_parse_move(game, FIRST_PLAYER_ROLE, "6");
    cr_assert_not_null(move, "Returned move was NULL");
    err = game_apply_move(game, move);
    cr_assert_eq(err, 0, "Returned value was not 0");

    move = game_parse_move(game, SECOND_PLAYER_ROLE, "4");
    cr_assert_not_null(move, "Returned move was NULL");
    err = game_apply_move(game, move);
    cr_assert_eq(err, 0, "Returned value was not 0");

    move = game_parse_move(game, FIRST_PLAYER_ROLE, "7");
    cr_assert_not_null(move, "Returned move was NULL");
    err = game_apply_move(game, move);
    cr_assert_eq(err, 0, "Returned value was not 0");

    move = game_parse_move(game, SECOND_PLAYER_ROLE, "8");
    cr_assert_not_null(move, "Returned move was NULL");
    err = game_apply_move(game, move);
    cr_assert_eq(err, 0, "Returned value was not 0");

    move = game_parse_move(game, FIRST_PLAYER_ROLE, "9");
    cr_assert_not_null(move, "Returned move was NULL");
    err = game_apply_move(game, move);
    cr_assert_eq(err, 0, "Returned value was not 0");

    cr_assert(game_is_over(game), "Game should be over");
    cr_assert_eq(game_get_winner(game), NULL_ROLE, "Game should have no winner");

    move = game_parse_move(game, SECOND_PLAYER_ROLE, "4");
    cr_assert_not_null(move, "Returned move was NULL");
    err = game_apply_move(game, move);
    cr_assert_eq(err, -1, "Returned value was not -1");
}

/*
 * Tic-tac-toe winner detection.
 */
static int calcSum(GAME_ROLE *board, int startRow, int startCol, int rowInc, int colInc) {
    int sum = 0;
    for(int row = startRow, col = startCol;
	row >= 0 && row < 3 && col >= 0 && col < 3;
	row += rowInc, col += colInc) {
	GAME_ROLE who = board[3*row + col];
	if(who == FIRST_PLAYER_ROLE)
	    sum += 4;
	else if(who == SECOND_PLAYER_ROLE)
	    sum += 1;
    }
    return sum;
}

static GAME_ROLE checkDirection(GAME_ROLE *board, int startRow, int startCol, int rowInc, int colInc) {
    int sum = calcSum(board, startRow, startCol, rowInc, colInc);
    if(sum / 4 >= 3)
	return FIRST_PLAYER_ROLE;
    else if(sum % 4 >= 3)
	return SECOND_PLAYER_ROLE;
    return NULL_ROLE;
}

static GAME_ROLE three_in_a_row(GAME_ROLE *board) {
    GAME_ROLE who = NULL_ROLE;

    // Check rows.
    for(int row = 0; row < 3; row++) {
	who = checkDirection(board, row, 0, 0, 1);
	if(who)
	    return who;
    }
    // Check columns.
    for(int col = 0; col < 3; col++) {
	who = checkDirection(board, 0, col, 1, 0);
	if(who)
	    return who;
    }
    // Check diagonals.
    who = checkDirection(board, 0, 0, 1, 1);
    if(who)
	return who;
    who = checkDirection(board, 2, 0, -1, 1);
    if(who)
	return who;

    return NULL_ROLE;
}

/*
 * Concurrency test: Create a game and give it to a number of threads.
 * The threads will try to make random moves.  Each successful move will
 * also be marked on a reference version of the game board.  About the
 * only thing that we can readily check is that two moves to the same
 * position are never allowed and that the game result is the same.
 */

struct random_moves_args {
    int trial;
    GAME *game;
    GAME_ROLE board[9];
    int moves;
    pthread_mutex_t mutex;
};

void *random_moves_thread(void *arg) {
    struct random_moves_args *ap = arg;
    GAME *game = ap->game;
    unsigned int seed = 1;
    for(int i = 0; i < ap->moves; i++) {
	int pos = (rand_r(&seed) % 9) + 1;
	int role = (rand_r(&seed) %2) + 1;
	char str[3];
	sprintf(str, "%d", pos);
	GAME_MOVE *move = game_parse_move(game, role, str);
	if(move == NULL)
	    continue;
	int err = game_apply_move(game, move);
	if(!err) {
	    pthread_mutex_lock(&ap->mutex);
	    cr_assert_eq(ap->board[pos-1], NULL_ROLE,
			 "Board position (%d) was already taken in game %d", pos, ap->trial);
	    ap->board[pos-1] = role;
	    pthread_mutex_unlock(&ap->mutex);
	}
    }
    return NULL;
}

Test(game_suite, random_moves, .timeout = 15) {
#ifdef NO_GAME
    cr_assert_fail("Game module was not implemented");
#endif
    // Only playing a single random game does not always reveal flaws in
    // the code being tested.  So we will run it several times.
    for(int n = 0; n < NGAMES; n++) {
	struct random_moves_args args;
	args.trial = n;
	args.game = game_create();
	memset(args.board, NULL_ROLE, sizeof(args.board));
	args.moves = NMOVES;
	pthread_mutex_init(&args.mutex, NULL);

	pthread_t tid[NTHREAD];
	for(int i = 0; i < NTHREAD; i++)
	    pthread_create(&tid[i], NULL, random_moves_thread, &args);

	// Wait for all threads to finish.
	for(int i = 0; i < NTHREAD; i++)
	    pthread_join(tid[i], NULL);

	// Check game result.
	int marked = 0;
	for(int i = 0; i < 9; i++) {
	    if(args.board[i] != NULL_ROLE)
		marked++;
	}
	int go = game_is_over(args.game);
	int winner = game_get_winner(args.game);
	int ref_go = (winner != NULL_ROLE || marked == 9);
	cr_assert_eq(go, ref_go, "Game over (%d) does not match expected (%d) in game %d",
		     go, ref_go, n);

	int ref_winner = three_in_a_row(args.board);
	cr_assert_eq(winner, ref_winner,
		     "Game winner (%d) does not match expected (%d) in game %d",
		     winner, ref_winner, n);
    }
}
