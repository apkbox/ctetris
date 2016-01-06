/* utetris.c */
#ifndef WIDTH
#define WIDTH 10
#endif

#ifndef HEIGHT
#define HEIGHT 20
#endif

#ifndef RANDOM_ROTATE
#define RANDOM_ROTATE   0
#endif

#ifndef SHOW_NEXT
#define SHOW_NEXT       1
#endif

typedef char ttm_byte;
typedef unsigned short ttm_word;

typedef enum UserCommandTag {
    NOTHING,
    ROTATE_CW,
    ROTATE_CCW,
    MOVE_LEFT,
    MOVE_RIGHT,
    SPEEDUP,
    DROP,
    QUIT
} UserCommand;

#ifndef ttm_assert
#define ttm_assert(e)
#endif

#ifndef ttm_rnd_init
#define ttm_rnd_init()
#endif

#ifndef ttm_rnd
#define ttm_rnd()   (1)
#endif

#ifndef ttm_sleep_ms
#define ttm_sleep_ms(ms)
#endif

#ifndef ttm_render_callback
#define ttm_render_callback(gameboard, width, height)
#define NO_RENDER
#endif

UserCommand ttm_read_command_callback();

#define swap_byte(a, b)  { ttm_byte t = (a); (a) = (b); (b) = t; }
#define swap_word(a, b)  { ttm_word t = (a); (a) = (b); (b) = t; }

typedef enum PlayCycleResultTag {
    QUIT_GAME,
    END_OF_GAME,
    CONTINUE_PLAY
} PlayCycleResult;

#define NEED_RENDER             0x01
#define NEW_TETRIMINO_SPAWNED   0x02
#define QUIT_REQUESTED          0x04

typedef struct TetriminoTag {
    ttm_word def;
    /* TODO: Optimize here as we need only to cover range (2..4) */
    ttm_byte box:4;    /* side of a square box that tetrimino is fits == max(w,h) */
} Tetrimino;

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
ttm_byte next_tetrimino = -1;
#endif

/* Each word represents a single row with three bit padding on either side */
ttm_word gameboard[HEIGHT];

#define ROW_MASK    0x1FF8

/* A single tetrimino with bottom left being bit zero. */
ttm_word tetrimino[4];
Tetrimino *ttm_def;
ttm_byte ttm_pos_x, ttm_pos_y;

/* TODO: We can track stack height, which will allow some optimizations */

#define TIMER_TICKS_PER_CYCLE   25

ttm_byte timer_counter;
ttm_byte time_is_up;

void reset_game_timer() {
    timer_counter = 0;
    time_is_up = 0;
}

void run_game_timer() {
    if (++timer_counter >= 25) {
        time_is_up = 1;
        timer_counter = 0;
    }
}

int max_int(int a, int b) {
    return a >= b ? a : b;
}

#ifdef NOCLIB
void *memset(void *ptr, int v, size_t s) {
    char *p = (char *)ptr;
    while (p < (((char *)ptr) + s))
        *p++ = v;
    return ptr;
}
#endif

void collapse_rows(ttm_byte rl, ttm_byte rh) {
    int i;
    int size = HEIGHT - rh;
    
    ttm_assert(rl < rh);

    for (i = 0; i < size; ++i)
        gameboard[rl + i] = gameboard[rh + i];

    /* TODO: If we track stack height, we can optimize here by zeroing
       only part of gameboard that was actually used.
    */
    for (i = (HEIGHT - (rh - rl)); i < (HEIGHT); ++i)
        gameboard[i] = 0;
}

void check_and_collapse_rows() {
    int r, c;
    int rl = -1, rh = -1;

    /* TODO: If we track stack height, we need only to go as high as the top
        of the stack and no higher.
    */
    for (r = 0; r < HEIGHT; ++r) {
        int full = gameboard[r] == ROW_MASK;
        int empty = gameboard[r] == 0;
        
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

int check_collision(ttm_byte landing) {
    int r;
    
    /*
        Normalize as it is used as an row offset to detect landing collision.
        Whether the piece is landed is checked by offsetting it down one row
        and checking for collisions.
    */
    landing = landing ? 1 : 0;

    for (r = 0; r < 4; ++r) {
        int row = ttm_pos_y - landing + r;
        
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

void transpose4x4(ttm_word *m, ttm_byte offset) {
    ttm_word m0 = m[0] >> offset;
    ttm_word m1 = m[1] >> offset;
    ttm_word m2 = m[2] >> offset;
    ttm_word m3 = m[3] >> offset;

    m[0] = ((m0 & 0x1) | (m1 << 1 & 0x2) | (m2 << 2 & 0x4) | (m3 << 3 & 0x8)) << offset;
    m[1] = ((m0 >> 1 & 0x1) | (m1 & 0x2) | (m2 << 1 & 0x4) | (m3 << 2 & 0x8)) << offset;
    m[2] = ((m0 >> 2 & 0x1) | (m1 >> 1 & 0x2) | (m2 & 0x4) | (m3 << 1 & 0x8)) << offset;
    m[3] = ((m0 >> 3 & 0x1) | (m1 >> 2 & 0x2) | (m2 >> 1 & 0x4) | (m3 & 0x8)) << offset;
}

void transpose3x3(ttm_word *m, ttm_byte offset) {
    ttm_word m0 = m[0] >> offset;
    ttm_word m1 = m[1] >> offset;
    ttm_word m2 = m[2] >> offset;

    m[0] = ((m0 & 0x1) | (m1 << 1 & 0x2) | (m2 << 2 & 0x4)) << offset;
    m[1] = ((m0 >> 1 & 0x1) | (m1 & 0x2) | (m2 << 1 & 0x4)) << offset;
    m[2] = ((m0 >> 2 & 0x1) | (m1 >> 1 & 0x2) | (m2 & 0x4)) << offset;
}

/*
    Transposes a square matrix.
 */
void transpose(ttm_word *m, ttm_byte offset, ttm_byte matrix_size) {
    switch (matrix_size) {
        case 4:
            transpose4x4(m, offset);
            break;
            
        case 3:
            transpose3x3(m, offset);
            break;
    }
}

void mirror_y4x4(ttm_word *m, ttm_byte offset) {
    ttm_word m0 = m[0] >> offset;
    ttm_word m1 = m[1] >> offset;
    ttm_word m2 = m[2] >> offset;
    ttm_word m3 = m[3] >> offset;

    m[0] = ((m0 << 3 & 0x8) | (m0 << 1 & 0x4) | (m0 >> 1 & 0x2) | (m0 >> 3 & 0x1)) << offset;
    m[1] = ((m1 << 3 & 0x8) | (m1 << 1 & 0x4) | (m1 >> 1 & 0x2) | (m1 >> 3 & 0x1)) << offset;
    m[2] = ((m2 << 3 & 0x8) | (m2 << 1 & 0x4) | (m2 >> 1 & 0x2) | (m2 >> 3 & 0x1)) << offset;
    m[3] = ((m3 << 3 & 0x8) | (m3 << 1 & 0x4) | (m3 >> 1 & 0x2) | (m3 >> 3 & 0x1)) << offset;
}

void mirror_y3x3(ttm_word *m, ttm_byte offset) {
    ttm_word m0 = m[0] >> offset;
    ttm_word m1 = m[1] >> offset;
    ttm_word m2 = m[2] >> offset;
    ttm_word m3 = m[3] >> offset;

    m[0] = ((m0 << 2 & 0x4) | (m0 & 0x2) | (m0 >> 2 & 0x1)) << offset;
    m[1] = ((m1 << 2 & 0x4) | (m1 & 0x2) | (m1 >> 2 & 0x1)) << offset;
    m[2] = ((m2 << 2 & 0x4) | (m2 & 0x2) | (m2 >> 2 & 0x1)) << offset;
}

void mirror_y(ttm_word *m, ttm_byte offset, ttm_byte matrix_size) {
    switch (matrix_size) {
        case 4:
            mirror_y4x4(m, offset);
            break;
            
        case 3:
            mirror_y3x3(m, offset);
            break;
    }
}

void rotate_cw(ttm_word *m, ttm_byte offset, ttm_byte matrix_size) {
    transpose(m, offset, matrix_size);
    mirror_y(m, offset, matrix_size);
}

void rotate_ccw(ttm_word *m, ttm_byte offset, ttm_byte matrix_size) {
    mirror_y(m, offset, matrix_size);
    transpose(m, offset, matrix_size);
}

void rotate_tetrimino(ttm_byte angle) {
    switch (angle) {
        case 90:
            rotate_cw(tetrimino, ttm_pos_x, ttm_def->box);
            break;
            
        case -90:
            rotate_ccw(tetrimino, ttm_pos_x, ttm_def->box);
            break;
    }
}

void move_tetrimino(ttm_byte offset) {
    int i;

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
    int i;
    ttm_byte ttm_index;
#if RANDOM_ROTATE
    ttm_byte rotation;
#endif
    
#if SHOW_NEXT
    if (next_tetrimino < 0) {
        ttm_index = ttm_rnd() % 7;
        next_tetrimino = ttm_rnd() % 7;
    } else {
        ttm_index = next_tetrimino;
        next_tetrimino = ttm_rnd() % 7;
    }
#else
    ttm_index = ttm_rnd() % 7;
#endif
    
    ttm_def = &tetriminos[ttm_index];
    
    for (i = 0; i < 4; ++i)
        tetrimino[i] = 0;
        
    ttm_pos_y = HEIGHT - 2;
    ttm_pos_x = (WIDTH - ttm_def->box) / 2 + 3;   /* +3 for 3 bit padding */

    for (i = 0; i < 4; ++i)
        tetrimino[i] = ((((ttm_word)ttm_def->def) >> (i * 4)) & 0x000F) << ttm_pos_x;

#if RANDOM_ROTATE
    rotation = ttm_rnd() % 3;
    rotate_tetrimino(rotation * 90);
#endif
}

void place_tetrimino() {
    int r, c;
    for (r = 0; r < 4; ++r) {
        /* 
            ttm_pos_y may be such that places tetrimino outside bounds
            of the gameboard completely or partially. This is the case
            for spawning, where spawn position is in two rows above the
            visible gameboard. We do not need to allocate these rows
            as they never rendered, thus the check.
        */
        int row = ttm_pos_y + r;
        if (row < HEIGHT) {
            ttm_word *rowptr = &gameboard[(ttm_pos_y + r)];
            /* 
                Use XOR instead or OR so we can use it for rendering.
                Normally there should be no collisions between
                stack and tetrimino.
            */
            *rowptr ^= tetrimino[r];
        }
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
    int flags = 0;
    UserCommand cmd = ttm_read_command_callback();
    
    /* TODO: 
        For now just skip the whole wall/floor kick thing as it is
        non-trivial.
        And yet also see https://tetris.wiki/SRS 
        (How Guidline SDS Really Works).
    */
    switch (cmd) {
        case ROTATE_CW:
            rotate_tetrimino(90);
            if (check_collision(0))
                rotate_tetrimino(-90);
            else
                flags |= NEED_RENDER;
            break;

        case ROTATE_CCW:
            rotate_tetrimino(-90);
            if (check_collision(0))
                rotate_tetrimino(90);
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
    ttm_render_callback(gameboard, WIDTH, HEIGHT);
    place_tetrimino();
#endif    
}

PlayCycleResult run_cycle() {
    PlayCycleResult result = CONTINUE_PLAY;
    int flags;

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
    int i;

    for (i = 0; i < HEIGHT; ++i)
        gameboard[i] = 0;

    spawn_new_tetrimino();
    
    ttm_assert(!check_collision(0));

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
            
        ttm_sleep_ms(20);  // 20ms (50Hz)
    }
    
    return result;
}

void game_loop() {
    ttm_rnd_init();

    while (1) {
        while (1) {
            UserCommand cmd = ttm_read_command_callback();
            if (cmd == QUIT)
                return;
            else if (cmd != NOTHING)
                break;

            ttm_sleep_ms(300);
        }
    
        PlayCycleResult result = play_loop();
        if (result == QUIT_GAME)
            break;
    }
}
