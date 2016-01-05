/* ctetris.c */
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

#define swap_int(a, b)  { int t = (a); (a) = (b); (b) = t; }

typedef enum PlayCycleResultTag {
    QUIT_GAME,
    END_OF_GAME,
    CONTINUE_PLAY
} PlayCycleResult;

#define NEED_RENDER             0x01
#define NEW_TETRIMINO_SPAWNED   0x02
#define QUIT_REQUESTED          0x04

typedef struct TetriminoTag {
    int defx[4];
    int defy[4];
    int x;      /* x,y - rotation matrix 0,0 */
    int y;
    int box;    /* side of a square box that tetrimino is fits == max(w,h) */
} Tetrimino;

Tetrimino tetriminos[7] = {
    /* I */
    { { 0, 1, 2, 3 },
      { 2, 2, 2, 2 },
        0, 0, 4 },
    /* J */
    { { 0, 0, 1, 2 },
      { 2, 1, 1, 1 },
        0, 0, 3 },
    /* L */
    { { 0, 1, 2, 2 },
      { 1, 1, 1, 2 },
        0, 0, 3 },
    /* O */
    { { 1, 2, 2, 1 },   
      { 2, 2, 1, 1 },
        1, 1, 2 },
    /* S */
    { { 0, 1, 1, 2 },
      { 1, 1, 2, 2 },
        0, 0, 3 },
    /* T (upside-down) */
    { { 0, 1, 1, 2 },
      { 1, 1, 2, 1 },
        0, 0, 3 },
    /* Z */
    { { 0, 1, 1, 2 },
      { 2, 2, 1, 1 },
        0, 0, 3 },
};

#if SHOW_NEXT
int next_tetrimino = -1;
#endif

int gameboard[WIDTH * HEIGHT];

int tetrimino[4*4];
int ttm_x, ttm_y, ttm_box;
int ttm_pos_x, ttm_pos_y;

/* TODO: We can track stack height, which will allow some optimizations */

#define TIMER_TICKS_PER_CYCLE   25

int timer_counter;
int time_is_up;

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

void collapse_rows(int rl, int rh) {
    int i;
    int laddr = rl * WIDTH;
    int haddr = rh * WIDTH;
    int size = WIDTH * HEIGHT - haddr;
    
    ttm_assert(rl < rh);

    for (i = 0; i < size; ++i)
        gameboard[laddr + i] = gameboard[haddr + i];

    /* TODO: If we track stack height, we can optimize here by zeroing
       only part of gameboard that was actually used.
    */
    for (i = (HEIGHT - (rh - rl)) * WIDTH; i < (HEIGHT * WIDTH); ++i)
        gameboard[i] = 0;
}

void check_and_collapse_rows() {
    int r, c;
    int rl = -1, rh = -1;

    /* TODO: If we track stack height, we need only to go as high as the top
        of the stack and no higher.
    */
    for (r = 0; r < HEIGHT; ++r) {
        int full = 1;
        int empty = 0;
        for (c = 0; c < WIDTH; ++c) {
            int cell = gameboard[r * WIDTH + c];
            full &= cell;
            empty |= cell;
        }
        
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

int check_collision(int landing) {
    int r, c;
    
    /*
        Normalize as it is used as an row offset to detect landing collision.
        Whether the piece is landed is checked by offsetting it down one row
        and checking for collisions.
    */
    landing = landing ? 1 : 0;

    for (r = 0; r < 4; ++r) {
        int row = ttm_pos_y - landing + r;
        int row_offset = row * WIDTH;
        
        /* 
            If tetramino is landing on the floor or there is a rotation
            that causes it be appear below the floor and there is no stack,
            then just check is any of the tetramino's bits are set in rows
            below the floor and if so, we hit the floor.
        */
        if (row < 0) {
            for (c = 0; c < 4; ++c) {
                if (tetrimino[r * 4 + c])
                    return 1;
            }
        }
        
        if (row < 0 || row >= HEIGHT)
            continue;
            
        for (c = 0; c < 4; ++c) {
            int col = ttm_pos_x + c;
            int col_offset = row_offset + col;
            
            if (col < 0 || col >= WIDTH) {
                if (tetrimino[r * 4 + c])
                    return 1;
                continue;
            }

            int pfcell = gameboard[col_offset];
            int tmcell = tetrimino[r * 4 + c];
            if (pfcell && tmcell)
                return 1;
        }
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

/*
    Transposes a square matrix.
    m matrix
    bs buffer size (width and height)
    x_off, y_off location of matrix in the buffer
    ms matrix size (width and height)
 */
void transpose(int *m, int bs, int x_off, int y_off, int ms) {
    int x, y;
    
    ttm_assert((x_off + ms) <= bs);
    ttm_assert((y_off + ms) <= bs);

    for (y = 0; y < ms; ++y) {
        for (x = y + 1; x < ms; ++x) {
            int *mrx = &m[(y + y_off) * bs + x + x_off];
            int *mry = &m[(x + x_off) * bs + y + y_off];
            swap_int(*mrx, *mry);
        }
    }
}

void mirror_y(int *m, int bs, int x_off, int y_off, int ms) {
    int x, y;

    for (y = 0; y < ms; ++y) {
        for (x = 0; x < ms / 2; ++x) {
            int *row = &m[(y + y_off) * bs];
            int *mrl = &row[x + x_off];
            int *mrr = &row[ms - 1 - x - x_off];
            swap_int(*mrl, *mrr);
        }
    }
}

void rotate_cw(int *m, int bs, int x_off, int y_off, int ms) {
    transpose(m, bs, x_off, y_off, ms);
    mirror_y(m, bs, x_off, y_off, ms);
}

void rotate_ccw(int *m, int bs, int x_off, int y_off, int ms) {
    mirror_y(m, bs, x_off, y_off, ms);
    transpose(m, bs, x_off, y_off, ms);
}

void rotate_tetrimino(int angle) {
    switch (angle) {
        case 90:
            rotate_cw(tetrimino, 4, ttm_x, ttm_y, ttm_box);
            break;
            
        case -90:
            rotate_ccw(tetrimino, 4, ttm_x, ttm_y, ttm_box);
            break;
    }
}

void move_tetrimino(int offset) {
    if (offset < 0)
        --ttm_pos_x;
    else if (offset > 0)
        ++ttm_pos_x;
}

void spawn_new_tetrimino() {
    int i;
    int ttm_index;
    Tetrimino *tmdef;
#if RANDOM_ROTATE
    int rotation;
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
    
    tmdef = &tetriminos[ttm_index];
    
    for (i = 0; i < 16; ++i)
        tetrimino[i] = 0;
        
    for (i = 0; i < 4; ++i) {
        int r = tmdef->defy[i];
        int c = tmdef->defx[i];
        tetrimino[r * 4 + c] = 1;
        ttm_x = tmdef->x;
        ttm_y = tmdef->y;
        ttm_box = tmdef->box;
    }
    
#if RANDOM_ROTATE
    rotation = ttm_rnd() % 3;
    rotate_tetrimino(rotation * 90);
#endif
    
    ttm_pos_y = HEIGHT - 2;
    ttm_pos_x = (WIDTH - ttm_box) / 2;
}

void place_tetrimino() {
    int r, c;
    for (r = 0; r < 4; ++r) {
        for (c = 0; c < 4; ++c) {
            /* 
                ttm_pos_y may be such that places tetrimino outside bounds
                of the gameboard completely or partially. This is the case
                for spawning, where spawn position is in two rows above the
                visible gameboard. We do not need to allocate these rows
                as they never rendered, thus the check.
            */
            int row = ttm_pos_y + r;
            if (row < HEIGHT) {
                int *pfcell = &gameboard[(ttm_pos_y + r) * WIDTH + ttm_pos_x + c];
                /* 
                    Use XOR instead or OR so we can use it for rendering.
                    Normally there should be no collisions between
                    stack and tetrimino.
                */
                *pfcell ^= tetrimino[r * 4 + c];
            }
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

    for (i = 0; i < (WIDTH * HEIGHT); ++i)
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
