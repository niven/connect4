#include <stdio.h>
#include <stdlib.h>

#include "base.h"
#include "counter.h"

void print_counter( gen_counter* c ) {

	printf("Generation Counters\n" );
	printf("CPU time used: %f\n", c->cpu_time_used );
	printf("Total boards: %ld\n", c->total_boards );
	printf("Unique boards: %ld\n", c->unique_boards );
	printf("Wins white: %ld\n", c->wins_white );
	printf("Wins black: %ld\n", c->wins_black );
	printf("Draws: %ld\n", c->draws );
}

gen_counter* read_counter( const char* filename ) {
	gen_counter* gc = (gen_counter*)malloc( sizeof(gen_counter) );
	if( gc == NULL ) {
		perror("malloc()");
		exit( EXIT_FAILURE );
	}

	FILE* in;
	FOPEN_CHECK( in, filename, "rb")

	size_t objects_read = fread( gc, sizeof(gen_counter), 1, in );
	if( objects_read != 1 ) {
		perror("fread()");
		exit( EXIT_FAILURE );
	}

	fclose( in );

	return gc;
}

void write_counter( gen_counter* c, const char* filename ) {

	FILE* out;
	FOPEN_CHECK( out, filename, "wb" )

	size_t bytes_written = fwrite( c, 1, sizeof(gen_counter), out );
	if( bytes_written != sizeof(gen_counter) ) {
		perror("fwrite()");
		exit( EXIT_FAILURE );
	}

	fclose( out );
}
