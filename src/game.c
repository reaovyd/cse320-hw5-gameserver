#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include "game.h"
#include "debug.h"

#define GAME_RUNNING    0
#define GAME_TERMINATED 1

struct game {
    pthread_mutex_t board_mutex;
    char **board;

    pthread_mutex_t cur_turn_mutex;
    volatile GAME_ROLE cur_turn;

    pthread_mutex_t game_status_mutex;
    volatile int game_status; /* 0 if not done 1 if done*/

    pthread_mutex_t winner_mutex;
    volatile GAME_ROLE winner; 

    pthread_mutex_t mutex;
    volatile size_t ref_count;
};

struct game_move {
    GAME_ROLE gr;
    unsigned int i_coord;
    unsigned int j_coord;
    unsigned int original_num;
};

// winner setter/getter
static void set_winner(GAME *game, GAME_ROLE winner) {
    pthread_mutex_lock(&game->winner_mutex);
    game->winner = winner;
    pthread_mutex_unlock(&game->winner_mutex);
}

static GAME_ROLE get_winner(GAME *game) {
    pthread_mutex_lock(&game->winner_mutex);
    GAME_ROLE winner = game->winner;
    pthread_mutex_unlock(&game->winner_mutex);
    return winner;
}

// game_move setter/getter
static void set_game_move(GAME *game, GAME_ROLE gr) {
    pthread_mutex_lock(&game->cur_turn_mutex);
    game->cur_turn = gr;
    pthread_mutex_unlock(&game->cur_turn_mutex);
}

static GAME_ROLE get_game_move(GAME *game) {
    pthread_mutex_lock(&game->cur_turn_mutex);
    GAME_ROLE gr = game->cur_turn;
    pthread_mutex_unlock(&game->cur_turn_mutex);
    return gr;
}

// game_status setter/getter
static void set_game_status(GAME *game, int game_status) {
    pthread_mutex_lock(&game->game_status_mutex);
    game->game_status = game_status;
    pthread_mutex_unlock(&game->game_status_mutex);
}

static int get_game_status(GAME *game) {
    pthread_mutex_lock(&game->game_status_mutex);
    int gs = game->game_status;
    pthread_mutex_unlock(&game->game_status_mutex);

    return gs;
}


GAME *game_create() {
    GAME *new_game = malloc(sizeof(GAME));
    if(new_game == NULL) {
        debug("%ld: Initialized game failed.", pthread_self());
        return NULL;
    }
    new_game->board = malloc(sizeof(char *) * 5);
    if(new_game->board == NULL) {
        free(new_game);
        debug("%ld: Initialized game failed.", pthread_self());
        return NULL;
    }
    for(int i = 0; i < 5; ++i) {
        new_game->board[i] = calloc(6, sizeof(char *));
        if(new_game->board[i] == NULL) {
            for(int j = i - 1; j >= 0; --j) {
                free(new_game->board[j]);
            }
            debug("%ld: Initialized game failed.", pthread_self());
            free(new_game->board);
            free(new_game);
            return NULL;
        }
    }
    for(int i = 0; i < 5; i++){
        for(int j = 0; j < 5; j++) {
            new_game->board[i][j] = ' ';
        }
    }
    for(int i = 0; i < 5; ++i) {
        new_game->board[i][5] = '\n';
    }
    for(int i = 1; i < 5; i += 2) {
        for(int j = 0; j < 5; ++j) {
            new_game->board[i][j] = '-'; 
        }
    }
    for(int i = 0; i < 5; i += 2) {
        for(int j = 1; j < 5; j += 2) {
            new_game->board[i][j] = '|';
        }
    }

    new_game->game_status = GAME_RUNNING; 
    new_game->winner = NULL_ROLE; 
    new_game->cur_turn = FIRST_PLAYER_ROLE;

    pthread_mutex_init(&new_game->mutex, NULL);
    pthread_mutex_init(&new_game->board_mutex, NULL);
    pthread_mutex_init(&new_game->cur_turn_mutex, NULL);
    pthread_mutex_init(&new_game->game_status_mutex, NULL);
    pthread_mutex_init(&new_game->winner_mutex, NULL);

    new_game->ref_count = 0;

    game_ref(new_game, "because new game has been initialized");

    return new_game;
}

GAME *game_ref(GAME *game, char *why) {
    if(game == NULL) {
        debug("%ld: Invalid game object!", pthread_self());
        return NULL;
    }
    pthread_mutex_lock(&game->mutex);
    size_t old_ref = game->ref_count;
    game->ref_count = old_ref + 1;

    debug("%ld: Increase reference count on game %p (%lu -> %lu) %s", pthread_self(), game, old_ref, game->ref_count, why); 
    
    pthread_mutex_unlock(&game->mutex);
    return game;
}

void game_unref(GAME *game, char *why) {
    if(game == NULL) {
        debug("%ld: Invalid game object!", pthread_self());
        return;
    }

    pthread_mutex_lock(&game->mutex);
    size_t old_ref = game->ref_count;
    game->ref_count = old_ref - 1;

    debug("%ld: Decrease reference count on game %p (%lu -> %lu) %s", pthread_self(), game, old_ref, game->ref_count, why); 
    if(game->ref_count == 0) {
        pthread_mutex_lock(&game->board_mutex);
        for(int i = 0; i < 5; ++i) {
            free(game->board[i]);
            game->board[i] = NULL;
        }
        free(game->board);
        pthread_mutex_unlock(&game->board_mutex);
        pthread_mutex_destroy(&game->board_mutex);

        pthread_mutex_unlock(&game->mutex);
        pthread_mutex_destroy(&game->mutex);

        pthread_mutex_destroy(&game->cur_turn_mutex);
        pthread_mutex_destroy(&game->game_status_mutex);
        pthread_mutex_destroy(&game->winner_mutex);
        free(game);
        debug("%ld: Free game %p", pthread_self(), game);
        return;
    }

    
    pthread_mutex_unlock(&game->mutex);
}

static int verify_board(GAME *game) {
    for(int i = 0; i < 5; i += 2) {
        int co = 0;
        char c = game->board[i][0];
        if(c == ' ') {
            continue;
        }
        for(int j = 0; j < 5; j += 2) {
            if(game->board[i][j] == c) 
                co++;
        }
        if(co == 3) {
            return c;
        }
    }
    for(int j = 0; j < 5; j += 2) {
        int co = 0;
        char c = game->board[0][j];
        if(c == ' ') {
            continue;
        }
        for(int i = 0; i < 5; i += 2) {
            if(game->board[i][j] == c) {
                co++;
            }
        }
        if(co == 3) {
            return c;
        }
    }
    int co = 0;
    char c = game->board[0][0];
    if(c != ' ') {
        for(int i = 0; i < 5; i += 2) {
            if(game->board[i][i] == c) {
                co++;
            }
        }
        if(co == 3) {
            return c;
        }
    }
    co = 0;
    c = game->board[0][4];
    if(c != ' ') {
        for(int i = 0, j = 4; i < 5 && j >= 0; i += 2, j -= 2) {
            if(game->board[i][j] == c) {
                co++;
            }
        }
        if(co == 3) {
            return c;
        }
    }

    return 0;
}

static int apply_and_verify_safe(GAME *game, GAME_MOVE *move) {
    pthread_mutex_lock(&game->board_mutex);
    int space_exist = 0;
    for(int i = 0; i < 5; i += 2) {
        for(int j = 0; j < 5; j += 2) {
            if(game->board[i][j] == ' ') {
                space_exist = 1;
                break;
            }
        }
    }
    if(!space_exist) {
        pthread_mutex_unlock(&game->board_mutex);
        return 33; 
    }
    game->board[move->i_coord][move->j_coord] = move->gr == FIRST_PLAYER_ROLE ? 'X' : 'O'; 
    int ret;
    if((ret = verify_board(game))) {
        pthread_mutex_unlock(&game->board_mutex);
        return ret;
    }

    space_exist = 0;
    for(int i = 0; i < 5; i += 2) {
        for(int j = 0; j < 5; j += 2) {
            if(game->board[i][j] == ' ') {
                space_exist = 1;
                break;
            }
        }
    }
    if(!space_exist) {
        pthread_mutex_unlock(&game->board_mutex);
        return 33; 
    }

    pthread_mutex_unlock(&game->board_mutex);
    return 0;
}

int game_apply_move(GAME *game, GAME_MOVE *move) {
    if(move == NULL) {
        return -1;
    }
    if(game_is_over(game)) {
        return -1;
    }
    if(move->gr == NULL_ROLE) {
        return -1;
    }
    if(game->board[move->i_coord][move->j_coord] != ' ') {
        return -1;
    }
    int c = apply_and_verify_safe(game, move);

    set_game_move(game, FIRST_PLAYER_ROLE == move->gr ? SECOND_PLAYER_ROLE : FIRST_PLAYER_ROLE);

    if(c) {
        set_game_status(game, GAME_TERMINATED);
        if(c == 'X') {
            set_winner(game, FIRST_PLAYER_ROLE);
        } else if(c == 'O') {
            set_winner(game, SECOND_PLAYER_ROLE);
        }
    }
    return 0;
}

int game_resign(GAME *game, GAME_ROLE role) {
    if(game == NULL) {
        return -1;
    }
    if(game_is_over(game)) {
        return -1;
    }
    set_game_status(game, GAME_TERMINATED);
    set_winner(game, role == FIRST_PLAYER_ROLE ? SECOND_PLAYER_ROLE : FIRST_PLAYER_ROLE);
    return 0;
}

char *game_unparse_state(GAME *game) {
    if(game == NULL) {
        return NULL;
    }
    if(game->board == NULL) {
        return NULL;
    }
    char *str;
    size_t sz;

    FILE *fstream = open_memstream(&str, &sz);
    if(fstream == NULL) {
        return NULL;
    }
    pthread_mutex_lock(&game->board_mutex);
    for(int i = 0; i < 5; ++i) {
        for(int j = 0; j < 6; ++j) {
            fputc(game->board[i][j], fstream);
        }
    }
    pthread_mutex_unlock(&game->board_mutex);
    fprintf(fstream, "%c to move\n", get_game_move(game) == FIRST_PLAYER_ROLE ? 'X' : 'O');
    fclose(fstream);

    return str;
}

int game_is_over(GAME *game) {
    if(game == NULL) {
        return 0;
    }
    return get_game_status(game) == GAME_TERMINATED; 
}

GAME_ROLE game_get_winner(GAME *game) {
    return get_winner(game); // TODO winner
}

static int is_valid_move_str(GAME_ROLE cur_role, char *str) {
    if(str == NULL){
        return -1;
    }
    if(strlen(str) == 0) {
        return -1; 
    }
    char first_char = *str;
    if(first_char < '1' || first_char > '9') {
        return -1;
    }
    if(*(str + 1) == '\0') {
        return (first_char - '0');
    }
    if(*(str + 1) != '<') {
        return -1;
    }
    if(*(str + 2) != '-') {
        return -1;
    }
    if(*(str + 3) == 'X') {
        if((cur_role != FIRST_PLAYER_ROLE)) {
            return -1;
        }
        return (first_char - '0');
    }
    if(*(str + 3) == 'O') {
        if((cur_role != SECOND_PLAYER_ROLE)) {
            return -1;
        }
        return (first_char - '0');
    }
    return -1;
}
GAME_MOVE *game_parse_move(GAME *game, GAME_ROLE role, char *str) {
    if(game == NULL) {
        return NULL;
    }
    if(role != NULL_ROLE && role != get_game_move(game)) {
        debug("%ld: Game role turns were not the same!", pthread_self());
        return NULL;
    }
    int move = is_valid_move_str(role, str);
    if(move == -1) {
        return NULL;
    }
    GAME_MOVE *gm = malloc(sizeof(GAME_MOVE));
    if(gm == NULL) {
        debug("%ld: Failed to initialize game move object", pthread_self());
        return NULL;
    }
    gm->gr = role;
    gm->i_coord = ((move - 1) / 3) * 2;
    gm->j_coord = ((move - 1) % 3) * 2;
    gm->original_num = move; 

    return gm;
}
char *game_unparse_move(GAME_MOVE *move) {
    if(move == NULL) {
        return NULL;
    }
    char *buf;
    size_t sz;
    FILE *fstream = open_memstream(&buf, &sz);
    if(fstream == NULL) {
        return NULL;
    }
    fprintf(fstream, "%d<-%c\n", move->original_num, move->gr == FIRST_PLAYER_ROLE ? 'X' : 'O');
    fclose(fstream);

    return buf;
}
