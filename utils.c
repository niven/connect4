#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils.h"

void print_bits(unsigned char c) {
	char out[11];
	out[0] = '0';
	out[1] = 'b';
	for(int i=7; i>=0; i-- ) {
		out[2+i] = (c >> (7-i)) & 1 ? '1' : '0';
	}
	out[10] = '\0';
	printf("%s\n", out);
}


char* join( int* elements, size_t num, const char* separator ) {
	char* buf = malloc( sizeof(char)*256 );
	char str[100];
	
	for( int i=0; i<num; i++ ) {
		sprintf( str, "%d", elements[i] );
		strcat( buf, str );
		if( i < num-1 ) {
			strcat( buf, separator );
		}
	}
	
	return buf;
}
