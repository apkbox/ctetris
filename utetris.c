/* utetris.c */
#include "utetris.h"

#ifndef UT_ASSERT
#define UT_ASSERT(e)
#endif

#ifndef UT_RND
#define UT_RND()            (1)
#endif

#ifndef UT_RND_INIT
#define UT_RND_INIT()
#endif

#ifndef UT_SLEEP_MS
#define UT_SLEEP_MS(ms)
#endif

typedef enum PlayCycleResultTag {
    QUIT_GAME,
    END_OF_GAME,
    CONTINUE_PLAY
} PlayCycleResult;

#define NEED_RENDER             0x01
#define NEW_TETRIMINO_SPAWNED   0x02
#define QUIT_REQUESTED          0x04

Tetrimino tetriminos[7] = {
    /* I */
    { 0x0F00, 4 },
    /* J */
    { 0x0170, 3 },
    /* L */
    { 0x0470, 3 },
    /* O */
    { 0x0660, 2 },
    /* S */
    { 0x0630, 3 },
    /* T (upside-down) */
    { 0x0270, 3 },
    /* Z */
    { 0x0360, 3 },
};

#if SHOW_NEXT
utt_int8 next_tetrimino = -1;
#endif

/*
    Each word represents a single row with equal padding of at least 1 bit
    on either side.
*/
utt_word gameboard[HEIGHT];

#define ROW_LEFT_PADDING    ((UTT_WORD_WIDTH - WIDTH) >> 1)
#define WIDTH_MASK  ((1U << WIDTH) - 1)
#define ROW_MASK    (WIDTH_MASK << ROW_LEFT_PADDING)

/* A single tetrimino with bottom left being bit zero. */
utt_word tetrimino[4];
Tetrimino *ttm_def;
utt_int8 ttm_pos_x, ttm_pos_y;

utt_word timer_counter;
utt_bit time_is_up;

void reset_game_timer() {
    timer_counter = 0;
    time_is_up = 0;
}

void run_game_timer() {
    if (++timer_counter >= TIMER_TICKS_PER_CYCLE) {
        time_is_up = 1;
        timer_counter = 0;
    }
}

#ifdef NOCLIB
#pragma intrinsic(memset)
#pragma function(memset)
void *memset(void *ptr, int v, size_t s) {
    char *p = (char *)ptr;
    while (p < (((char *)ptr) + s))
        *p++ = v;
    return ptr;
}
#endif

void collapse_rows(utt_int8 rl, utt_int8 rh) {
    utt_int8 i;
    utt_int8 size = HEIGHT - rh;

    UT_ASSERT(rl < rh);

    for (i = 0; i < size; ++i)
        gameboard[rl + i] = gameboard[rh + i];

    for (i = HEIGHT - (rh - rl); i < HEIGHT; ++i)
        gameboard[i] = 0;
}

void check_and_collapse_rows() {
    utt_int8 r;
    utt_int8 rl = -1, rh = -1;

    for (r = 0; r < HEIGHT; ++r) {
        utt_bit full = gameboard[r] == ROW_MASK;
        utt_bit empty = gameboard[r] == 0;

        if (full) {
            if (rl == -1)
                rl = r;
            else
                rh = r;
        } else if (rl != -1) {
            /* [rl, rh) - interval */
            if (rh == -1)
                rh = r;
            collapse_rows(rl, rh);
            r = rl - 1;
            rl = rh = -1;
        }

        if (!empty)
            break;
    }
}

int check_collision(utt_bit landing) {
    utt_int8 r;

    /*
        Normalize as it is used as an row offset to detect landing collision.
        Whether the piece is landed is checked by offsetting it down one row
        and checking for collisions.
    */
    landing = landing != 0;

    for (r = 0; r < 4; ++r) {
        utt_int8 row = ttm_pos_y - landing + r;

        /*
            If tetramino is landing on the floor or there is a rotation
            that causes it be appear below the floor and there is no stack,
            then just check is any of the tetramino's bits are set in rows
            below the floor and if so, we hit the floor.
        */
        if (row < 0) {
            if (tetrimino[r])
                return 1;
        }

        if (row < 0 || row >= HEIGHT)
            continue;

        if (tetrimino[r] & (~ROW_MASK))
            return 1;

        if (gameboard[row] & tetrimino[r])
            return 1;
    }

    return 0;
}

int check_landing() {
    return check_collision(1);
}

/*
    Advances tetrimino towards the stack. If tetrimino is
    in landed position then no advance preformed.

    Returns 1 if tetrimino already in landed position before
    advance was attempted or 0 otherwise.
*/
int advance_tetrimino() {
    int landed = check_landing();
    if (!landed)
        --ttm_pos_y;

    return landed;
}

void transpose4x4(utt_word *m, utt_byte offset) {
    utt_word m0 = m[0] >> offset;
    utt_word m1 = m[1] >> offset;
    utt_word m2 = m[2] >> offset;
    utt_word m3 = m[3] >> offset;

    m[0] = ((m0 & 0x1) | (m1 << 1 & 0x2) | (m2 << 2 & 0x4) | (m3 << 3 & 0x8)) << offset;
    m[1] = ((m0 >> 1 & 0x1) | (m1 & 0x2) | (m2 << 1 & 0x4) | (m3 << 2 & 0x8)) << offset;
    m[2] = ((m0 >> 2 & 0x1) | (m1 >> 1 & 0x2) | (m2 & 0x4) | (m3 << 1 & 0x8)) << offset;
    m[3] = ((m0 >> 3 & 0x1) | (m1 >> 2 & 0x2) | (m2 >> 1 & 0x4) | (m3 & 0x8)) << offset;
}

void transpose3x3(utt_word *m, utt_byte offset) {
    utt_word m0 = m[0] >> offset;
    utt_word m1 = m[1] >> offset;
    utt_word m2 = m[2] >> offset;

    m[0] = ((m0 & 0x1) | (m1 << 1 & 0x2) | (m2 << 2 & 0x4)) << offset;
    m[1] = ((m0 >> 1 & 0x1) | (m1 & 0x2) | (m2 << 1 & 0x4)) << offset;
    m[2] = ((m0 >> 2 & 0x1) | (m1 >> 1 & 0x2) | (m2 & 0x4)) << offset;
}

/*
    Transposes a square matrix.
*/
void transpose(utt_word *m, utt_byte offset, utt_byte matrix_size) {
    switch (matrix_size) {
        case 4:
            transpose4x4(m, offset);
            break;

        case 3:
            transpose3x3(m, offset);
            break;
    }
}

void mirror_y4x4(utt_word *m, utt_byte offset) {
    utt_word m0 = m[0] >> offset;
    utt_word m1 = m[1] >> offset;
    utt_word m2 = m[2] >> offset;
    utt_word m3 = m[3] >> offset;

    m[0] = ((m0 << 3 & 0x8) | (m0 << 1 & 0x4) | (m0 >> 1 & 0x2) | (m0 >> 3 & 0x1)) << offset;
    m[1] = ((m1 << 3 & 0x8) | (m1 << 1 & 0x4) | (m1 >> 1 & 0x2) | (m1 >> 3 & 0x1)) << offset;
    m[2] = ((m2 << 3 & 0x8) | (m2 << 1 & 0x4) | (m2 >> 1 & 0x2) | (m2 >> 3 & 0x1)) << offset;
    m[3] = ((m3 << 3 & 0x8) | (m3 << 1 & 0x4) | (m3 >> 1 & 0x2) | (m3 >> 3 & 0x1)) << offset;
}

void mirror_y3x3(utt_word *m, utt_byte offset) {
    utt_word m0 = m[0] >> offset;
    utt_word m1 = m[1] >> offset;
    utt_word m2 = m[2] >> offset;

    m[0] = ((m0 << 2 & 0x4) | (m0 & 0x2) | (m0 >> 2 & 0x1)) << offset;
    m[1] = ((m1 << 2 & 0x4) | (m1 & 0x2) | (m1 >> 2 & 0x1)) << offset;
    m[2] = ((m2 << 2 & 0x4) | (m2 & 0x2) | (m2 >> 2 & 0x1)) << offset;
}

void mirror_y(utt_word *m, utt_byte offset, utt_byte matrix_size) {
    switch (matrix_size) {
        case 4:
            mirror_y4x4(m, offset);
            break;

        case 3:
            mirror_y3x3(m, offset);
            break;
    }
}

void rotate_cw(utt_word *m, utt_byte offset, utt_byte matrix_size) {
    transpose(m, offset, matrix_size);
    mirror_y(m, offset, matrix_size);
}

void rotate_ccw(utt_word *m, utt_byte offset, utt_byte matrix_size) {
    mirror_y(m, offset, matrix_size);
    transpose(m, offset, matrix_size);
}

void rotate_tetrimino(utt_int8 direction) {
    switch (direction) {
        case ROTATE_CW:
            rotate_cw(tetrimino, ttm_pos_x, ttm_def->box);
            break;

        case ROTATE_CCW:
            rotate_ccw(tetrimino, ttm_pos_x, ttm_def->box);
            break;
    }
}

void move_tetrimino(utt_int8 offset) {
    utt_int8 i;

    if (offset < 0) {
        for (i = 0; i < 4; ++i)
            tetrimino[i] >>= 1;
        --ttm_pos_x;
    } else if (offset > 0) {
        for (i = 0; i < 4; ++i)
            tetrimino[i] <<= 1;
        ++ttm_pos_x;
    }
}

void spawn_new_tetrimino() {
    utt_int8 i;
    utt_byte ttm_index;
#if RANDOM_ROTATE
    utt_byte rotation;
#endif

#if SHOW_NEXT
    if (next_tetrimino < 0) {
        ttm_index = UT_RND() % 7;
        next_tetrimino = UT_RND() % 7;
    } else {
        ttm_index = next_tetrimino;
        next_tetrimino = UT_RND() % 7;
    }
#else
    ttm_index = UT_RND() % 7;
#endif

    ttm_def = &tetriminos[ttm_index];

    for (i = 0; i < 4; ++i)
        tetrimino[i] = 0;

    ttm_pos_y = HEIGHT - 2;
    ttm_pos_x = (WIDTH - ttm_def->box) / 2 + ROW_LEFT_PADDING;

    for (i = 0; i < 4; ++i)
        tetrimino[i] = ((ttm_def->def >> (i * 4)) & 0x000F) << ttm_pos_x;

#if RANDOM_ROTATE
    rotation = UT_RND() % 3;
    while (rotation--)
        rotate_tetrimino(ROTATE_CW);
#endif
}

void place_tetrimino() {
    utt_int8 r, c;
    for (r = 0; r < 4; ++r) {
        /*
            ttm_pos_y may be such that places tetrimino outside bounds
            of the gameboard completely or partially. This is the case
            for spawning, where spawn position is in two rows above the
            visible gameboard. We do not need to allocate these rows
            as they never rendered, thus the check.
        */
        utt_int8 row = ttm_pos_y + r;
        if (row >= HEIGHT)
            break;

        /*
            Use XOR instead or OR so we can use it for rendering.
            Normally there should be no collisions between
            stack and tetrimino.
        */
        gameboard[row] ^= tetrimino[r];
    }
}

/*
    Returns flags that indicate result of processing of user input.
        NEED_RENDER
            The user input changed the gameboard and it needs
            to be re-rendered.

        NEW_TETRIMINO_SPAWNED
            A new tetrimino is spawned, so the timer needs to be reset
            and collision check should be performed.

        QUIT_REQUESTED
            User requested to quit application.
*/
int process_user_input() {
    utt_byte flags = 0;
    UserCommand cmd = ut_read_command_callback();

    /* TODO:
        For now just skip the whole wall/floor kick thing as it is
        non-trivial.
        And yet also see https://tetris.wiki/SRS
        (How Guidline SDS Really Works).
    */
    switch (cmd) {
        case ROTATE_CW:
            rotate_tetrimino(ROTATE_CW);
            if (check_collision(0))
                rotate_tetrimino(ROTATE_CCW);
            else
                flags |= NEED_RENDER;
            break;

        case ROTATE_CCW:
            rotate_tetrimino(ROTATE_CCW);
            if (check_collision(0))
                rotate_tetrimino(ROTATE_CW);
            else
                flags |= NEED_RENDER;
            break;

        case MOVE_LEFT:
            move_tetrimino(-1);
            if (check_collision(0))
                move_tetrimino(1);
            else
                flags |= NEED_RENDER;
            break;

        case MOVE_RIGHT:
            move_tetrimino(1);
            if (check_collision(0))
                move_tetrimino(-1);
            else
                flags |= NEED_RENDER;
            break;

        case SPEEDUP:
            if (advance_tetrimino()) {
                place_tetrimino();
                check_and_collapse_rows();
                spawn_new_tetrimino();
                flags |= NEW_TETRIMINO_SPAWNED;
            }
            flags |= NEED_RENDER;
            break;

        case DROP:
            /*
                1. Find the first non-empty row.
                2. Move down checking for landing iteratively.
            */
            while (1) {
                if (advance_tetrimino()) {
                    place_tetrimino();
                    check_and_collapse_rows();
                    spawn_new_tetrimino();
                    flags |= NEW_TETRIMINO_SPAWNED;
                    break;
                }
            }
            flags |= NEED_RENDER;
            break;

        case QUIT:
            flags |= QUIT_REQUESTED;
            break;

        case NOTHING:
        default:
            break;
    }

    return flags;
}

void render_gameboard() {
    /*
        1. Place the tetramino on the gameboard
        2. Call render callback with gameboard address, width and height.
        3. Remove the teramino from the gameboard by XORing the tetramino
           matrix with the gameboard.
    */
#ifndef NO_RENDER
    place_tetrimino();
    ut_render_callback(gameboard, WIDTH, HEIGHT, ROW_LEFT_PADDING);
    place_tetrimino();
#endif
}

PlayCycleResult run_cycle() {
    PlayCycleResult result = CONTINUE_PLAY;
    utt_int8 flags;

    flags = process_user_input();
    if ((flags & NEW_TETRIMINO_SPAWNED)) {
        if (check_landing())
            result = END_OF_GAME;

        /* Here we need to reset timer, because new tetrimino was generated. */
        reset_game_timer();
    } else if (time_is_up) {
        if (advance_tetrimino()) {
            place_tetrimino();
            check_and_collapse_rows();
            spawn_new_tetrimino();
            if (check_landing())
                result = END_OF_GAME;
        }

        flags |= NEED_RENDER;
        reset_game_timer();
    }

    if (flags & NEED_RENDER)
        render_gameboard();

    if (flags & QUIT_REQUESTED)
        result = QUIT_GAME;

    return result;
}

void init_game() {
    utt_int8 i;

    for (i = 0; i < HEIGHT; ++i)
        gameboard[i] = 0;

    spawn_new_tetrimino();

    UT_ASSERT(!check_collision(0));

    reset_game_timer();
}

PlayCycleResult play_loop() {
    PlayCycleResult result;

    init_game();

    while (1) {
        run_game_timer();
        result = run_cycle();
        if (result != CONTINUE_PLAY)
            break;

        UT_SLEEP_MS(20);  // 20ms (50Hz)
    }

    return result;
}

void game_loop() {
    UT_RND_INIT();

    while (1) {
        while (1) {
            UserCommand cmd = ut_read_command_callback();
            if (cmd == QUIT)
                return;
            else if (cmd != NOTHING)
                break;

            UT_SLEEP_MS(300);
        }

        PlayCycleResult result = play_loop();
        if (result == QUIT_GAME)
            break;
    }
}
