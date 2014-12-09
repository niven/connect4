#ifndef __C4TYPES_H__
#define __C4TYPES_H__

#include "base.h"

#define NUM_BOARD_BYTES 11

#define NUM_WINLINE_BYTES 9
#define NUM_WINLINES 69


#define ROWS 6
#define COLS 7

// used for both state (who's turn it is and squares)
#define EMPTY 0
#define WHITE 1
#define BLACK 2
#define OVER 4
#define DRAW 8

typedef unsigned char player;

// variable length encoding for a board (but using 64 bits always)
// every column has 3 bits (0-7) indicating number of squares occupied starting from the bottom
// then the next n bits indicate white (1), black (0)
// full board will mean every column fillcount is 6 (3 bits) + 6 bits = 9 bits for a total of 7x9=63 bits
// the remaining bit (LSB, so we can read col0 fillcount, values, col1 fillcount values, ...) indicates
// having winlines follow.
// we use this for storing on disk to save space, but to operate it's nicer to have a normal board
// so we convert at read/write time

typedef unsigned long board63;
typedef board63 key_t;

#define KEY_SIZE (sizeof(key_t))
#define ENCODED_BOARD_SIZE (sizeof(board63))

// TODO(correctness): find a way to compile time check ENCODED_BOARD_SIZE == KEY_SIZE just in case
	
// bit fields for winlineIDs for White/Black
// winlineID 0 is byte 0, LSB 0
// winlineID 69 is byte 8 (69/8=8) LSB 5 (69%8=5)
// this means bits 8,7,6 of byte 8 are unused 
// byte 0          1
// 7,6,5,4,3,2,1,0 14,13,12,11,10,9,8
typedef struct wins {
	unsigned char white[NUM_WINLINE_BYTES]; // just use 69 bits
	unsigned char black[NUM_WINLINE_BYTES];
} wins;


// winline in square coords
typedef struct winline {
	unsigned char x[4];
	unsigned char y[4];
} winline;


/*
	board used in code
	squares as square[x][y]
	bottom left = [0,0]
	top right = [6,5]
	values: 0 empty, 1 white, 2 black (see #defines)
*/
typedef struct board {
	unsigned char squares[COLS][ROWS];
	char state; /* WHITE, BLACK, OVER, DRAW */
	wins* winlines;
} board;

// Size of a serialized board: the key/board + gamestate (1 char) + winlines WHITE + winlines BLACK
#define BOARD_SERIALIZATION_NUM_BYTES (sizeof(board63) + sizeof(char) + 2*NUM_WINLINE_BYTES)

#define WINLINE_HOZ(y) 	{ {0,1,2,3}, {y,y,y,y} }, \
								{ {1,2,3,4}, {y,y,y,y} }, \
								{ {2,3,4,5}, {y,y,y,y} }, \
								{ {3,4,5,6}, {y,y,y,y} },

#define WINLINE_VERT(x)	{ {x,x,x,x}, {0,1,2,3} }, \
								{ {x,x,x,x}, {1,2,3,4} }, \
								{ {x,x,x,x}, {2,3,4,5} },

#define WINLINE_DIAG_UP(x,y) 	{ {x+0,x+1,x+2,x+3}, {y-0,y-1,y-2,y-3} }, \
										{ {x+1,x+2,x+3,x+4}, {y-0,y-1,y-2,y-3} }, \
										{ {x+2,x+3,x+4,x+5}, {y-0,y-1,y-2,y-3} }, \
										{ {x+3,x+4,x+5,x+6}, {y-0,y-1,y-2,y-3} },

#define WINLINE_DIAG_DOWN(x,y) 	{ {x+0,x+1,x+2,x+3}, {y+0,y+1,y+2,y+3} }, \
											{ {x+1,x+2,x+3,x+4}, {y+0,y+1,y+2,y+3} }, \
											{ {x+2,x+3,x+4,x+5}, {y+0,y+1,y+2,y+3} }, \
											{ {x+3,x+4,x+5,x+6}, {y+0,y+1,y+2,y+3} },

											


#endif
