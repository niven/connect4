#ifndef __BOARD_H__
#define __BOARD_H__	

extern winline winlines[NUM_WINLINES];
extern char states[4];

// 42 lists of ints, every one a list of [num_winlines, winline_0, winline_1, ...]
// To explain what this actually *is*: This maps every square on the board to all winlines it can be part of.
extern unsigned int* s2w[ ROWS * COLS ];

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
void pass_turn( board* b );


void render( board* b, const char* text, int show_winlines );

unsigned char current_player( board* b );	
unsigned char is_win_for( board* b, unsigned char player );
unsigned char is_draw( board* b );
int is_over( board* b );

void write_board( char* filename, board* b );

board* read_board_record( FILE* in );
void write_board_record( board* b, FILE* out );


#endif
