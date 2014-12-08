#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

#include "c4types.h"
#include "board63.h"


// print "0b" + (63 bits as fillcount 3 + ':' + 6 bits values + '\n') + 'endstate marker' + NUL
void print_board63( board63 b ) {

	printf("board63 as long %ld\n", b );

	// as binary string
	printf("0b");
	for(int i=63; i>=0; i--) {
		printf("%c", (b >> i) & 1 ? '1' : '0' );
	}
	printf("\ncnt 0=w 1=b");
	
	char out[7*(3+1+6+1)+1+1+1]; // = 80
	memset( out, 'x', 80);
	int offset = 0; // since we're inserting ':' and '\n'
	for( int i=0; i<63; i++ ) { // 7*(3+6) = 63
		if( i%9==0 ) {
			out[offset+i] = '\n';
			offset++;
		}
		if( (i-3)%9 == 0 ) { // after bits 3, 12, 21
			out[offset+i] = ':';
			offset++;
		}
		out[offset+i] = ((b >> (63-i)) & 1) ? '1' : '0'; // there are 63 bits, but the last one is unused out of 64
	}
	
	out[offset+63+0] = '\n';	
	out[offset+63+1] = '0' + (b & 0x1);
	out[offset+63+2] = '\0';
	printf("%s\n", out);
}

board63 encode_board( board* src ) {
	
	board63 dest = 0;
	
	// encode every column as fillcount + 6 bits
	// MSB(3) = fillcount of col0 etc for easy binary reading of humans
	unsigned char pieces = 0;
	for( int col=0; col<COLS; col++ ) {
//		printf("Encoding col %d: ", col);
		unsigned int row = 0;
		for( row=0; row<ROWS; row++ ) {
			if( src->squares[col][row] == EMPTY ) {
				break; // goto next col
			} else {
				pieces |= ( src->squares[col][row] == WHITE ? 0 : 1 ) << (ROWS-row-1); // first bit is bottom of col, next are appended
			}
		}
		// write encoded fillcount + pieces
		dest <<= 9; // make space first
//		printf("writing a fillcount of %d\n", row );
		unsigned long temp = row;
		temp <<= 6; // create space for 6 bits of pieces
		temp |= pieces; // now 3 bits fillcount followed by up to 6 bits of pieces, lowest y first
		dest |= temp;
		// reset
		pieces = 0;
		
		
	}
	
	dest <<= 1; // last bit is state
	
	if( src->state & OVER ) {
		dest |= 1; // set the gameover bit, indicating no state or winlines follow.
	}
	
	return dest;
}

board* decode_board63( board63 src ) {
	
	board* dest = (board*)malloc( sizeof(board) );
	if( dest == NULL ) {
		perror("malloc()");
		exit( EXIT_FAILURE );
	}
	dest->state = 0;
	dest->winlines = NULL;

//	printf("Decode board %p\n", dest);
	memset( dest->squares, EMPTY, ROWS*COLS );

	if( src & 1 ) {
		dest->state |= OVER;
	}
	src >>= 1; // skip it
	
	// fill it backwards
	int total_pieces = 0;
	for( int datacol=COLS-1; datacol>=0; datacol-- ) {
		int data = (src >> (9*datacol)) & 0x01ff; // Nth col and mask the 9 bits we need
		int fillcount = (data>>6) & 0x7; // LSB(3)
		total_pieces += fillcount;
		int pieces = data & 0x3f; // LSB(6)
//		printf("decoding col %d (data: %d) (fillcount: %d, pieces: %d)\n", 6-datacol, data, fillcount, pieces);
		for( int y=0; y<fillcount; y++ ) {
//			printf("Reading value [%d,%d] = %c\n", 6-datacol, y, (pieces >> (5-y)) ? 'X' : 'O'  );
			dest->squares[6-datacol][y] = (pieces >> (5-y)) & 1 ? BLACK : WHITE;
		}
	}

	return dest;
}
