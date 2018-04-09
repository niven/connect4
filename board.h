#ifndef BFCF_BOARD_H
#define BFCF_BOARD_H

#include <stdio.h>
#include <sys/types.h>

// TODO(clarity): explain what this is and what it is for
#define NUM_BOARD_BYTES 11

#define NUM_WINLINE_BYTES 9
#define NUM_WINLINES 69

// Size of the state of the board (draw, win, whose turn it is, ...)
#define SIZE_BOARD_STATE_BYTES (sizeof(char))

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
// the game is over (WIN/DRAW)
// we use this for storing on disk to save space, but to operate it's nicer to have a normal board
// so we convert at read/write time

typedef unsigned long board63;

// TODO(confusion): two names is maybe confusing
#define ENCODED_BOARD_SIZE (sizeof(board63))

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
	uint8 state; /* WHITE, BLACK, OVER, DRAW */
	wins* winlines;
} board;

// Size of a serialized board: gamestate (1 char) + winlines WHITE + winlines BLACK
#define BOARD_SERIALIZATION_NUM_BYTES (SIZE_BOARD_STATE_BYTES + 2*NUM_WINLINE_BYTES)

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



extern winline winlines[NUM_WINLINES];
extern char states[4];

// 42 lists of ints, every one a list of [num_winlines, winline_0, winline_1, ...]
// To explain what this actually *is*: This maps every square on the board to all winlines it can be part of.
extern uint8 s2w[ ROWS * COLS ][14];

void map_squares_to_winlines( void );
void free_s2w( void );

board* new_board( void );
void free_board( board* b );
board* copy_board( board* src );


void print_winline( int i );
void dump_winlines( void );
void print_winbits( unsigned char player[NUM_WINLINE_BYTES] );
void print_winlines(wins* w);
wins* new_winbits( void );

board* drop( board* src, int x );
uint8 multidrop( board* b, board63* next_boards );
void pass_turn( board* b );


void render( board* b, const char* text, int show_winlines );

unsigned char current_player( board* b );
unsigned char is_win_for( board* b, unsigned char player );
unsigned char is_draw( board* b );
int is_over( board* b );

void write_board( char* filename, board* b );

void read_board_record_from_buf( board63 b63, char* buf, off_t pos, board* dest );
void write_board_record( board* b, FILE* out );

void print_board63( board63 b );
board63 encode_board( board* src );
void decode_board63( board63 src, board* dest );
bool is_end_state( board63 b );


#endif
