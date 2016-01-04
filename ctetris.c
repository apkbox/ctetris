#define USE_STDLIB
#ifdef USE_STDLIB
#include <assert.h>
#include <stdio.h>
#include <conio.h>
#include <stdlib.h>
#include <time.h>
#endif

#include <windows.h>

#ifndef RANDOM_ROTATE
#define RANDOM_ROTATE   0
#endif

#ifndef SHOW_NEXT
#define SHOW_NEXT       1
#endif

#ifndef WIDTH
#define WIDTH 10
#endif

#ifndef HEIGHT
#define HEIGHT 20
#endif

#define swap_int(a, b)  { int t = (a); (a) = (b); (b) = t; }

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

#ifndef NOMAIN
UserCommand read_command_callback();
void render_callback(int *gameboard, int width, int height);

#define READ_COMMAND_CALLBACK() read_command_callback()
#define RENDER_CALLBACK(gameboard, width, height) render_callback(gameboard, width, height)
#else
#define READ_COMMAND_CALLBACK() (DROP)
#define RENDER_CALLBACK(gameboard, width, height) {}
#endif

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

int time_is_up;

int max_int(int a, int b) {
    return a >= b ? a : b;
}

void collapse_rows(int rl, int rh) {
    int i;
    int laddr = rl * WIDTH;
    int haddr = rh * WIDTH;
    int size = WIDTH * HEIGHT - haddr;
    
#ifdef USE_STDLIB
    assert(rl < rh);
#endif

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
        }
        else if (rl != -1) {
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

void advance_tetrimino() {
    --ttm_pos_y;
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
    
#ifdef USE_STDLIB
    assert((x_off + ms) <= bs);
    assert((y_off + ms) <= bs);
#endif

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
    if (offset < 0) {
        --ttm_pos_x;
    }
    else if (offset > 0) {
        ++ttm_pos_x;
    }
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
        ttm_index = rand() % 7;
        next_tetrimino = rand() % 7;
    }
    else {
        ttm_index = next_tetrimino;
        next_tetrimino = rand() % 7;
    }
#else
    ttm_index = rand() % 7;
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
    rotation = rand() % 3;
    rotate_tetrimino(rotation * 90);
#endif
    
    ttm_pos_y = HEIGHT - 2;
    ttm_pos_x = (WIDTH - ttm_box) / 2;
}

void place_tetrimino() {
    int r, c;
    for (r = 0; r < 4; ++r) {
        for (c = 0; c < 4; ++c) {
            /* ttm_pos_y may be such that places tetrimino outside bounds
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
    Returns 1 if user input resulted in need to render the gameboard.
    Returns -1 if QUIT command received.
*/
int process_user_input() {
    int render = 0;
    UserCommand cmd = READ_COMMAND_CALLBACK();
    
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
                render = 1;
            break;

        case ROTATE_CCW:
            rotate_tetrimino(-90);
            if (check_collision(0))
                rotate_tetrimino(90);
            else
                render = 1;
            break;
            
        case MOVE_LEFT:
            move_tetrimino(-1);
            if (check_collision(0))
                move_tetrimino(1);
            else
                render = 1;
            break;
            
        case MOVE_RIGHT:
            move_tetrimino(1);
            if (check_collision(0))
                move_tetrimino(-1);
            else
                render = 1;
            break;
            
        case SPEEDUP:
            /* TODO */
            advance_tetrimino();
            if (check_landing()) {
                place_tetrimino();
                check_and_collapse_rows();
                spawn_new_tetrimino();
            }
            render = 1;
            break;
            
        case DROP:
            /* TODO:
                1. Find the first non-empty row.
                2. Move down checking for landing iteratively.
            */
            while (1) {
                advance_tetrimino();
                if (check_landing()) {
                    place_tetrimino();
                    check_and_collapse_rows();
                    spawn_new_tetrimino();
                    break;
                }
            }
            render = 1;
            break;
            
        case QUIT:
            return -1;
            
        case NOTHING:
        default:
            break;
    }
    
    return render;
}

void render_gameboard() {
    /* TODO:
        1. Place the tetramino on the gameboard
        2. Call render callback with gameboard address, width and height.
        3. Remove the teramino from the gameboard by XORing the tetramino
           matrix with the gameboard.
    */
    place_tetrimino();
    RENDER_CALLBACK(gameboard, WIDTH, HEIGHT);
    place_tetrimino();
}

int run_cycle() {
    int need_render = 0;

    need_render = process_user_input();

    if (time_is_up) {
        /* 
            If user moved the piece into position that ended up being
            a landing position, then advance_tetrimino still attempt
            advance it without checking.
            So, we check if piece is landed due to user input, then
            we advance, if it is not, and then check again.
        */
        if (!check_landing()) {
            advance_tetrimino();
        }
        else {
            place_tetrimino();
            check_and_collapse_rows();
            spawn_new_tetrimino();
            if (check_collision(0)) {
                /* TODO: END OF GAME */; 
            }
        }
        
        need_render = 1;
        time_is_up = 0;
    }
    
    if (need_render)
        render_gameboard();
        
    if (need_render < 0)
        return 0;
        
    return 1;
}

#ifndef NOMAIN

UserCommand read_command_callback() {
#ifdef USE_STDLIB
    if (!_kbhit())
        return NOTHING;

    int c = getch();
    if (c == 224) {
        c = getch();
        switch(c) {
            case 75:
                return MOVE_LEFT;
            case 77:
                return MOVE_RIGHT;
            case 72: /* UP */
                return ROTATE_CCW;
            case 80: /* DOWN */
                return ROTATE_CW;
        }
    } else if (c == 'r' || c == 'R') {
        return SPEEDUP;
    } else if (c == ' ') {
        return DROP;
    } else if (c == 'q' || c == 'Q') {
        return QUIT;
    }
    
#else
    DWORD wait_result;
    HANDLE std_input_handle;
    
    std_input_handle = GetStdHandle(STD_INPUT_HANDLE);
    
    wait_result = WaitForSingleObject(std_input_handle, 0);
    if (wait_result == WAIT_OBJECT_0)  {
        INPUT_RECORD input_record;
        DWORD records_read;
        BOOL result = ReadConsoleInput(std_input_handle, 
                &input_record,
                1,
                &records_read);
        if (result &&
                input_record.EventType == KEY_EVENT &&
                input_record.Event.KeyEvent.bKeyDown) {
            WORD key = input_record.Event.KeyEvent.wVirtualKeyCode;
            switch (key) {
                case VK_LEFT:
                    return MOVE_LEFT;
                case VK_RIGHT:
                    return MOVE_RIGHT;
                case VK_UP: /* UP */
                    return ROTATE_CCW;
                case VK_DOWN: /* DOWN */
                    return ROTATE_CW;
                case 0x52:
                    return SPEEDUP;
                case VK_SPACE:
                    return DROP;
                case 0x51:
                    return QUIT;
            }
        }
    }
#endif
        
    return NOTHING;
}

HANDLE screen_buffer_handle;
CHAR_INFO screen_buffer[WIDTH * HEIGHT];
COORD screen_buffer_pos;
COORD screen_buffer_size;

void render_callback(int *gameboard, int width, int height) {
    SMALL_RECT screen_buffer_rect;
    int x, y, i;
    CHAR_INFO preview_sb[4 * 4];
    COORD preview_sb_pos;
    COORD preview_sb_size;
    SMALL_RECT preview_sb_rect;
    
    for (y = 0; y < HEIGHT; ++y) {
        for (x = 0; x < WIDTH; ++x) {
            CHAR_INFO *sptr = &screen_buffer[(HEIGHT - 1 - y) * WIDTH + x];
            sptr->Char.AsciiChar = gameboard[y * WIDTH + x] ? '\xDB' : '\xfa';
            sptr->Attributes = FOREGROUND_GREEN;
        }
    }

    screen_buffer_rect.Left = 0;
    screen_buffer_rect.Top = 0;
    screen_buffer_rect.Right = WIDTH - 1;
    screen_buffer_rect.Bottom = HEIGHT - 1;
    
    WriteConsoleOutput( 
        screen_buffer_handle,   /* screen buffer to write to            */
        screen_buffer,          /* buffer to copy from                  */
        screen_buffer_size,     /* col-row size of screen_buffer        */
        screen_buffer_pos,      /* top left src cell in screen_buffer   */
        &screen_buffer_rect);   /* dest. screen buffer rectangle        */

#if SHOW_NEXT
    if (next_tetrimino >= 0) {
        for (i = 0; i < 4 * 4; ++i) {
            CHAR_INFO *sptr = &preview_sb[i];
            sptr->Char.AsciiChar = ' ';
            sptr->Attributes = FOREGROUND_GREEN;
        }
        
        for (i = 0; i < 4; ++i) {
            Tetrimino *ttm = &tetriminos[next_tetrimino];
            CHAR_INFO *sptr = &preview_sb[(4 - ttm->defy[i]) * 4 + ttm->defx[i]];
            sptr->Char.AsciiChar = '\xDB';
            sptr->Attributes = FOREGROUND_GREEN;
        }
    }
    
    preview_sb_size.X = 4;
    preview_sb_size.Y = 4;
    preview_sb_pos.X = 0;
    preview_sb_pos.Y = 0;
    preview_sb_rect.Left = 23;
    preview_sb_rect.Top = 4;
    preview_sb_rect.Right = 23 + 4;
    preview_sb_rect.Bottom = 4 + 4;

    WriteConsoleOutput( 
        screen_buffer_handle,   /* screen buffer to write to            */
        preview_sb,             /* buffer to copy from                  */
        preview_sb_size,        /* col-row size of screen_buffer        */
        preview_sb_pos,         /* top left src cell in screen_buffer   */
        &preview_sb_rect);      /* dest. screen buffer rectangle        */
#endif
}

void game_loop() {
    int i = 0;
    
    spawn_new_tetrimino();

    while(1) {
        if (++i >= 25) {
            time_is_up = 1;
            i = 0;
        }

        if (!run_cycle())
            break;
            
        Sleep(20);  // 20ms
    }
}

int main() {
    CONSOLE_SCREEN_BUFFER_INFO console_info;
    
#ifdef USE_STDLIB
    srand(time(NULL));
#else
    /* TODO: Non-stdlib based random generator */
#endif

    screen_buffer_handle = GetStdHandle(STD_OUTPUT_HANDLE);
    // screen_buffer_handle = CreateConsoleScreenBuffer(
            // GENERIC_READ | GENERIC_WRITE,
            // FILE_SHARE_READ | FILE_SHARE_WRITE,
            // NULL, 
            // CONSOLE_TEXTMODE_BUFFER,
            // NULL);
    if (screen_buffer_handle == INVALID_HANDLE_VALUE) {
#ifdef USE_STDLIB
        printf("error: cannot create console screen buffer\n");
#endif
        return 1;
    }
    
    // SetConsoleActiveScreenBuffer(screen_buffer_handle);

    GetConsoleScreenBufferInfo(screen_buffer_handle, &console_info);

    screen_buffer_pos.X = 0;
    screen_buffer_pos.Y = 0;
    
    screen_buffer_size.X = WIDTH;
    screen_buffer_size.Y = HEIGHT;
    SetConsoleScreenBufferSize(screen_buffer_handle, screen_buffer_size);

    game_loop();

    screen_buffer_size.X = console_info.dwSize.X;
    screen_buffer_size.Y = console_info.dwSize.Y;
    SetConsoleScreenBufferSize(screen_buffer_handle, screen_buffer_size);

    return 0;
}

#endif

