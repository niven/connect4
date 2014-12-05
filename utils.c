#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils.h"

void create_empty_file( const char* filename ) {
	
	FILE* out = fopen( filename, "w" );
	if( out == NULL ) {
		perror("fopen()");
		exit( EXIT_FAILURE );
	}
	fclose( out );

}

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

/*
char* join( uint64_t* elements, size_t num, const char* separator ) {
	char* buf = malloc( sizeof(char)*256 );
	char str[100];
	
	for( size_t i=0; i<num; i++ ) {
		sprintf( str, "%llu", (uint64_t) elements[i] );
		strcat( buf, str );
		if( i < num-1 ) {
			strcat( buf, separator );
		}
	}
	
	return buf;
}
*/
