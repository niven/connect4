#ifndef __BOARD_H__
#define __BOARD_H__	


// 42 lists of ints, every one a list of [num_winlines, winline_0, winline_1, ...]
int* s2w[42];
void map_squares_to_winlines();
void free_s2w();

board* new_board();
void free_board( board* b );
board* copy_board( board* src );


void print_winline( int i );
void dump_winlines();
void print_winbits( unsigned char player[NUM_WINLINE_BYTES] );
void print_winlines(wins* w);
wins* new_winbits();

board* drop( board* src, int x );
void pass_turn( board* b );


void render( board* b, const char* text, int show_winlines );

unsigned char current_player( board* b );	
unsigned char is_win_for( board* b, unsigned char player );
unsigned char is_draw( board* b );

board* read_board( const char* filename );
void write_board( char* filename, board* b );

board* read_board_record( FILE* in );
void write_board_record( board* b, FILE* out );


#endif
