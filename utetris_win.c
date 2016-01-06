#define LEAN_AND_MEAN
#include <Windows.h>
#include <Rpc.h>

#define ttm_rnd() win_rnd()
#define ttm_sleep_ms(ms) Sleep(ms)

#define ttm_read_command_callback() read_command_callback()
#define ttm_render_callback(gb, w, h) render_callback(gb, w, h)

#define NOCLIB

int win_rnd();
int read_command_callback();
void render_callback(unsigned short *gameboard, int width, int height);

#include "utetris.c"

int win_rnd() {
    UUID uuid;
    UuidCreate(&uuid);
    return uuid.Data3;
}

int read_command_callback() {
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
        
    return NOTHING;
}

HANDLE screen_buffer_handle;
CHAR_INFO screen_buffer[WIDTH * HEIGHT];
COORD screen_buffer_pos;
COORD screen_buffer_size;

void render_callback(unsigned short *gameboard, int width, int height) {
    SMALL_RECT screen_buffer_rect;
    int x, y, i;
    CHAR_INFO preview_sb[4 * 4];
    COORD preview_sb_pos;
    COORD preview_sb_size;
    SMALL_RECT preview_sb_rect;
    
    /* Suppress pedantic warning */
    width = width;
    height = height;
    
    for (y = 0; y < HEIGHT; ++y) {
        for (x = 0; x < WIDTH; ++x) {
            CHAR_INFO *sptr = &screen_buffer[(HEIGHT - 1 - y) * WIDTH + x];
            sptr->Char.AsciiChar = (gameboard[y] >> (3 + x) & 1) ? '\xDB' : '\xfa';
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
            Tetrimino *ttm = &tetriminos[next_tetrimino];
            CHAR_INFO *sptr = &preview_sb[i];
            sptr->Char.AsciiChar = (ttm->def << i & 0x8000) ? '\xDB' : ' ';
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

int __stdcall WinMainCRTStartup() {
    CONSOLE_SCREEN_BUFFER_INFO console_info;
    
    AllocConsole();
    
    screen_buffer_handle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (screen_buffer_handle == INVALID_HANDLE_VALUE) {
        return 1;
    }
    
    GetConsoleScreenBufferInfo(screen_buffer_handle, &console_info);

    screen_buffer_pos.X = 0;
    screen_buffer_pos.Y = 0;
    
    screen_buffer_size.X = WIDTH;
    screen_buffer_size.Y = HEIGHT;
    // SetConsoleScreenBufferSize(screen_buffer_handle, screen_buffer_size);

    game_loop();

    screen_buffer_size.X = console_info.dwSize.X;
    screen_buffer_size.Y = console_info.dwSize.Y;
    SetConsoleScreenBufferSize(screen_buffer_handle, screen_buffer_size);

    return 0;
}
