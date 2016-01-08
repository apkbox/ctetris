#ifndef UTETRIS_H_HEADER_GUARD___
#define UTETRIS_H_HEADER_GUARD___

/*
    Two basic types used in the implementation.
    utt_bit  any boolean type.
    utt_int8 any signed one byte type.
    utt_byte any type at least 8 bit long.
    utt_word any type at least WIDTH + 2 bits long.
*/
typedef int utt_int8;
typedef int utt_bit;
typedef unsigned int utt_byte;
typedef unsigned int utt_word;

/*
    Width of the gameboard.
    The width must be no more than number_of_bits(utt_word)-2.
 */
#define WIDTH 10

/*
    utt_word size in bits.
*/
#define UTT_WORD_WIDTH      16

/* Height of the gameboard. */
#define HEIGHT 20

/*
    Specifies whether the tetrimino spawned with
    random rotation.
*/
#define RANDOM_ROTATE   0

/*
    Whether the next tetrimino preview is available.
*/
#define SHOW_NEXT       1

/*
    Number of ticks per cycle. One tick is 20ms.
*/
#define TIMER_TICKS_PER_CYCLE   25

/*
    User input commands.
*/
typedef enum UserCommandTag {
    NOTHING,        /* Unsupported user input or no user input. */
    ROTATE_CW,
    ROTATE_CCW,
    MOVE_LEFT,
    MOVE_RIGHT,
    SPEEDUP,
    DROP,
    QUIT
} UserCommand;

typedef struct TetriminoTag {
    utt_word def;
    utt_byte box;    /* bounding box size that fits tetrimino */
} Tetrimino;

extern Tetrimino tetriminos[];
extern utt_int8 next_tetrimino;

/*
    Externally provided implementation function
    that reads user input and returns command.
    This function must not be blocking and return NOTHING
    when there is no command.
*/
UserCommand ut_read_command_callback();
void ut_render_callback(utt_word *gameboard,
                        utt_int8 width,
                        utt_int8 height,
                        utt_int8 left_padding);

#include "utetfunc.h"

#endif /* UTETRIS_H_HEADER_GUARD___ */
