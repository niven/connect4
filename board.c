#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "base.h"
#include "utils.h"

#include "board.h"

#include "board63.c"

// states for squares
char states[4] = {' ', 'O', 'X', '@'}; // empty, white, black, unused | 00, 01, 10, 11


winline winlines[NUM_WINLINES] = {

	// 6x4 horizontal = 24
	WINLINE_HOZ(0) 
	WINLINE_HOZ(1) 
	WINLINE_HOZ(2) 
	WINLINE_HOZ(3) 
	WINLINE_HOZ(4) 
	WINLINE_HOZ(5) 
	
	// 7x3 vertical = 21
	WINLINE_VERT(0) 
	WINLINE_VERT(1) 
	WINLINE_VERT(2) 
	WINLINE_VERT(3) 
	WINLINE_VERT(4) 
	WINLINE_VERT(5) 
	WINLINE_VERT(6) 

	// 3x4 diagonal = 12 
	// starting bottom left, sweeping right
	WINLINE_DIAG_UP(0,5)
	// starting bottom left -1y, sweeping right
	WINLINE_DIAG_UP(0,4)
	// starting bottom left -2y, sweeping right
	WINLINE_DIAG_UP(0,3)

	// 3x4 diagonal = 12
	// starting top left, sweeping right
	WINLINE_DIAG_DOWN(0,0)
	// starting top left +1y, sweeping right
	WINLINE_DIAG_DOWN(0,1)
	// starting top left +2y, sweeping right
	WINLINE_DIAG_DOWN(0,2)

	// total = 24+21+12+12=69
};

uint8* s2w[42];

void map_squares_to_winlines() {
	
	uint8 temp[13];
	uint8 wi = 0;
	for( uint8 i=0; i<42; i++ ) {
		uint8 x = i % 7;
		uint8 y = i / 7;
		wi = 0;
		for( uint8 w=0; w<NUM_WINLINES; w++ ) {
			for( uint8 p=0; p<4; p++ ) {
				if( winlines[w].x[p] == x && winlines[w].y[p] == y ) {
					temp[wi] = w;
					wi++;
				}
			}
		}
		s2w[i] = (uint8*) malloc( sizeof(uint8) * (wi+1) ); // first int is count
		if( s2w[i] == NULL ) {
			perror("malloc()");
			abort();
		}
		s2w[i][0] = wi; // first el is count
		memcpy( s2w[i] + 1, temp, sizeof(uint8)*wi ); // copy indices
	}
	
}

void free_s2w() {
	
	for(int i=0; i<42; i++ ) {
		free( s2w[i] );
	}
}


void print_winline( int i ) {
	printf("Winline[%02d] = [%d,%d]-[%d,%d]-[%d,%d]-[%d,%d]\n", i 
		, winlines[i].x[0], winlines[i].y[0] 
		, winlines[i].x[1], winlines[i].y[1] 
		, winlines[i].x[2], winlines[i].y[2] 
		, winlines[i].x[3], winlines[i].y[3]	
	);
	
}

void dump_winlines() {
	
	for(int i=0; i<69; i++ ) {
		print_winline( i );
	}

}


// just print the 69 bits, not the 3 unused top bits of byte 8
void print_winbits( unsigned char player[9] ) {
//	printf("%x-%x-%x-%x-%x-%x-%x-%x-%x\n", player[0],player[1],player[2],player[3],player[4],player[5],player[6],player[7],player[8]);

	char out[ 2 + (8*NUM_WINLINE_BYTES) + NUM_WINLINE_BYTES + 1]; // "0b" + 8 bytes + insert '-' for visual debug + NUL
	out[0] = '0';
	out[1] = 'b';
	int offset = 2;
	for( int i=0; i<8*NUM_WINLINE_BYTES; i++ ) {
		unsigned char target_byte = (unsigned char)i/8;
		int target_bit = i%8;

		if( i%8 == 0 && i>0 ) {
			out[offset+i] = '-';
			offset++;
		}

		out[offset+i] = (player[target_byte] >> (7-target_bit) ) & 1 ? '1' : '0';

	}

	out[82] = '\0';
	printf("%s\n", out);
}

void print_winlines(wins* w) {
	printf("Winlines remaining for White (O):\n");
	print_winbits( w->white );
	for( int i=0; i<NUM_WINLINES; i++ ) { // can be optimized for iterations equal to num bits set
		if( (w->white[i/8] & (1 << (i%8))) > 0 ) {
			print_winline( i );
		}
	}
	printf("Winlines remaining for Black (X):\n");
	print_winbits( w->black );
	for( int i=0; i<NUM_WINLINES; i++ ) { // can be optimized for iterations equal to num bits set
//		printf("Winline %d - Target byte %d = %x\n", i, i%8, target_byte);
		if( (w->black[i/8] & (1 << (i%8))) > 0 ) {
			print_winline( i );
		}
	}
	
}

wins* new_winbits() {
	wins* out = (wins*)malloc( sizeof(wins) );
	if( out == NULL ) {
		perror("malloc()");
		exit( EXIT_FAILURE );
	}
	// set all winlines to available (we only need 69, so don't set the last 3)
	for( int i=0; i<8; i++ ) {
		out->white[i] = 0xff;
		out->black[i] = 0xff;
	}
	out->white[8] = 0x1f; // exclude top 3 bits
	out->black[8] = 0x1f;
	
	return out;
}

board* new_board() {

	board* b = (board*) malloc( sizeof(board) );
	if( b == NULL ) {
		perror("malloc()");
		exit(0);
	}
//	printf("Create board %p\n", b);
	memset( b->squares, 0, ROWS*COLS );
	
	b->winlines = new_winbits();
//	printf("Create winlines: %p\n", b->winlines);
	
	b->state = 0;
	b->state |= WHITE;
	
	return b;
}

void free_board( board* b ) {

	if( b->winlines != NULL ) {
//		printf("Free winlines: %p\n", b->winlines);
		free( b->winlines );	
		b->winlines = NULL;	
	}
//	printf("Free board: %p\n", b);
	free( b );
	
	b = NULL;
}

board* copy_board( board* src ) {
	
	board* dest = (board*) malloc( sizeof(board) );
//	printf("Copy board dest %p, src %p\n", dest, src);
	memcpy( dest->squares, src->squares, ROWS*COLS );
	
	if( src->winlines != NULL ) {
		dest->winlines = (wins*)malloc( sizeof(wins) );
		if( dest->winlines == NULL ) {
			perror("malloc()");
			exit( EXIT_FAILURE );
		}

//		printf("Copy winlines dest %p, src %p\n", dest->winlines, src->winlines);
		memcpy( dest->winlines, src->winlines, sizeof(wins) );
	}
	
	dest->state = src->state;
	
	return dest;
}

void pass_turn( board* b ) {

	if( b->state & WHITE ) {
		b->state &= ~WHITE;
		b->state |= BLACK;
	} else {
		b->state &= ~BLACK;
		b->state |= WHITE;
	}
	
}

unsigned char current_player( board* b ) {
	
	return b->state & WHITE ? WHITE : BLACK;
}

internal void update_winlines( board* b, int move_y, int move_x ) {
	
	player current = current_player( b );
//	print("Updating winlines after move by %s on [%d,%d]", current == WHITE ? "White" : "Black", move_x, move_y);
//	print_winbits( b->winlines->white );
//	print_winbits( b->winlines->black );
	
	int square_index = COLS*move_y + move_x;
//	printf("Square [%d,%d] = %d\n", move_x, move_y, square_index);
//	printf("num winlines: %d\n", s2w[square_index][0]);
	
	unsigned char* winline = current == WHITE ? b->winlines->black : b->winlines->white;
	
	for( unsigned int w=1; w<=s2w[square_index][0]; w++ ) {
		unsigned int winline_index = s2w[square_index][w];

		// clear it for opponent
		int bit_to_turn_off = 1 << (winline_index % 8);
//		printf("Turn off bit %d of byte %d\n", (winline_index % 8), winline_index/8);
		
		// I'm sure we can do this more elegantly, but I've had a few pints and I'm the train and this works.
		unsigned char old_state = winline[winline_index/8];
		int new_stuff = ~bit_to_turn_off;
		winline[winline_index/8] = old_state & new_stuff;

//		print_winbits( winline );
//		printf("\n");

		// now also check the winline of the move to see if there is a win
//		printf("Check win for %s\n", current == WHITE ? "White" : "Black");
//		print_winline( winline_index );
		int in_line = 0;
		for( int i=0; i<4; i++ ) {
			int sx = winlines[winline_index].x[i];
			int sy = winlines[winline_index].y[i];
			if( b->squares[sx][sy] == current ) {
				in_line++;
			}
		}
//		printf("In line in winline %d = %d\n", winline_index, in_line );
		if( in_line == 4 ) {
#ifdef VERBOSE
			char scratch[200];
			sprintf( scratch, "%s wins on line %d", current == WHITE ? "White" : "Black", winline_index );
			render( b, scratch, false );
#endif			
			b->state |= OVER;
		}
	}

	

//	print_winlines( b->winlines );
}





board* drop( board* src, int x ) {
	
	assert( !is_over( src ) );
	
	assert( x >= 0 );
	assert( x < COLS );
	
	// check if any room left
	int y_index = -1;
	for( int y=ROWS-1; y>=0; y-- ) { // seek down along column until we can hit occupied
//		print("checking [%d,%d]", x,y);
		if( src->squares[x][y] == EMPTY ) {
			y_index = y;
		} else {
			break;
		}
	}
	
	if( y_index == -1 ) {
		return NULL;
	}
	
	board* dest = copy_board( src );
	dest->squares[x][y_index] = current_player( src );

	// winlines update
	update_winlines( dest, y_index, x );

	if( !is_win_for( dest, current_player( src ) ) ) {
		pass_turn( dest );
	}

	return dest;
}

internal uint8 check_state_after_move( board* b, uint8 move_y, uint8 move_x ) {
	
	assert( move_y < ROWS );
	assert( move_x < COLS );

	uint8 state = 0;
	
	uint8 square_index = COLS*move_y + move_x;
	player current = current_player( b );
	
	for( uint8 w=1; w<=s2w[square_index][0]; w++ ) {
		uint8 winline_index = s2w[square_index][w];

		// now check the winline of the move to see if there is a win
		// print("Check win for %s", current == WHITE ? "White" : "Black");
		// print_winline( winline_index );
		uint8 in_line = 0;
		for( uint8 i=0; i<4; i++ ) {
			uint8 sx = winlines[winline_index].x[i];
			uint8 sy = winlines[winline_index].y[i];
			if( b->squares[sx][sy] == current ) {
				in_line++;
			}
		}
		// printf("In line in winline %d = %d\n", winline_index, in_line );
		if( in_line == 4 ) {
#ifdef VERBOSE
			char scratch[200];
			sprintf( scratch, "%s wins on line %d", current == WHITE ? "White" : "Black", winline_index );
			render( b, scratch, false );
#endif
			state |= OVER; // game over
			state |= current; // record winner
		}
	}	
	
	return state;
}

int multidrop( board* src, board63* next_boards ) {
	
	assert( !is_over( src ) );

	// check if if we can drop in a column for other columns
	int succesful_drops = 0;
	uint8 old_state;
	for( uint8 x_index=0; x_index<COLS; x_index++ ) {
		uint8 y_index = 0;
		for( uint8 y=0; y < ROWS; y++ ) { // seek down along column until we can hit occupied
			if( src->squares[x_index][y] == EMPTY ) {
				break;
			} else {
				y_index = y;
			}
		}
		if( y_index != ROWS ) {
			// print("Can drop at [%d, %d]", x_index, y_index );
			old_state = src->state;
			// set it
			src->squares[x_index][y_index] = current_player( src );
			uint8 state = check_state_after_move( src, y_index, x_index );
			if( state & OVER ) {
				src->state = state;
			}
			// copy it to dest
			next_boards[succesful_drops] = encode_board( src );

			// reset the move
			src->squares[x_index][y_index] = EMPTY;
			src->state = old_state;
			succesful_drops++;
		}
		
	}

	return succesful_drops;
}


int is_over( board* b ) {
	return is_draw( b ) || is_win_for( b, WHITE ) || is_win_for( b, BLACK );
}

void render( board* b, const char* text, int show_winlines ) {
	
	printf( "Board: %s\n(player turn: %s, State: %d)\n", text, current_player(b) == WHITE ? "White" : "Black", b->state );
	
	for( int y=ROWS-1; y>=0; y-- ) {
		printf("--+---+---+---+---+---+---+---+\n");
		printf("%d ", y);
		for( int x=0; x<COLS; x++) {
			
			unsigned char c = b->squares[x][y];
//			printf("\nstate: %x\n", c);
			// LSB 2 bits are now our value
			//printf("Square value: %x\n", c); 
			printf("| %c ", states[c]);
		}
		printf("|\n");
	}
	printf("--+---+---+---+---+---+---+---+\n");
	printf("  | 0 | 1 | 2 | 3 | 4 | 5 | 6 |\n");
	
	if( show_winlines && !is_over( b ) ) {
		print_winlines( b->winlines );
	}
}

void read_board_record_from_buf( board63 b63, char* buf, off_t pos, board* b ) {
	
	decode_board63( b63, b );

	// read state
	b->state = (uint8)buf[ pos ];	

	b->winlines = new_winbits();

	// TODO(performance): why even have 2 of these? just have 1 array? (memcpy for both at the same time fails)
	memcpy( &b->winlines->white, &buf[ (unsigned long long)pos + SIZE_BOARD_STATE_BYTES ], NUM_WINLINE_BYTES );
	memcpy( &b->winlines->black, &buf[ (unsigned long long)pos + SIZE_BOARD_STATE_BYTES + NUM_WINLINE_BYTES], NUM_WINLINE_BYTES );

}


void write_board_record( board* b, FILE* out ) {
	
	// write gamestate
	fwrite( &b->state, sizeof(b->state), 1, out );

	size_t elements_written = fwrite( b->winlines, NUM_WINLINE_BYTES, 2, out );
	if( elements_written != 2 ) {
		perror("fwrite()");
		exit( EXIT_FAILURE );
	}		
	
}

// TODO(cleanup): This can go
void write_board( char* filename, board* b ) {

	FILE* out;
	FOPEN_CHECK( out, filename, "wb" );

	write_board_record( b, out );
	
	fclose( out );
	
}

player is_win_for( board* b, player p ) {
	
	if( b->state & OVER ) {
		return (b->state & p) ? 1 : 0;
	} 
	
	return 0;
}

unsigned char is_draw( board* b ) {
	return (b->state & DRAW) ? 1 : 0;
}

bool is_end_state( board63 b ) {
	return b & 0x1; // the LSB indicates the game is a draw or win
}

