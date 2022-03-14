#include <assert.h>
#include <ctype.h>
#include <getopt.h>
#include <math.h> /* ceil() */
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>


#include "base.h"

#include "board.h"
#include "counter.h"
#include "utils.h"

#define BLOCK_SIZE (1 * 1 * 1000)

internal void print_stats( const char* directory ) {

	char genfilename[256];
	printf("Gen\tTotal\tUnique\twins W\twins B\tCPU time (s)\n");
	for( int g=1; g<=42; g++ ) { // just try all possible and break when done


		sprintf(genfilename, "%s/gencounter_%d.gc", directory, g);
		struct stat gstat;
		if( stat( genfilename, &gstat ) == -1 ) {
			break;
		}
		gen_counter* gc = read_counter( genfilename );

		printf( "%d\t%lu\t%lu\t%lu\t%lu\t%f\n", g, gc->total_boards, gc->unique_boards, gc->wins_white, gc->wins_black, gc->cpu_time_used );

	}

}

internal void display_progress( size_t current, size_t total ) {

	if( total < 100 ) {
		return;
	}
	size_t one_percent = total / 100;
	if( current % one_percent == 0 ) {
		printf("\r%.2f%%", 100. * (double)current / (double)total);
	}
}

internal void next_generation( const char* source_file, const char* destination_directory ) {

	board63 output_boards[ BLOCK_SIZE ];
	uint64 created = 0;

	// to number the blocks we output
	uint16 block = 0;

	// do all 7 moves in 1 drop, avoiding mallocing a brazillian new boards
	board63 next_gen[7];

	// iterate over all boards
	entry boards = map( source_file );
	uint64 total_boards = boards.remaining;

	// gen next
	gen_counter counters;
	memset( &counters, 0, sizeof(gen_counter) );

	// cpu timing
	clock_t cpu_time_start = clock();

	while( boards.remaining > 0 ) {
		display_progress( total_boards - boards.remaining, total_boards );

		board63 current_board63 = *(boards.current);
		boards.remaining--;

		if( is_end_state( current_board63 ) ) {
			continue;
		}

		board current_board;
		decode_board63( current_board63, &current_board );
		// render( &current_board, "Multidrop", false);
		unsigned char player = current_player( &current_board );

		uint8 num_succesful_drops = multidrop( &current_board, next_gen );
		print("Got %d drops", num_succesful_drops);
		counters.total_boards += num_succesful_drops;
		for( int i=0; i<num_succesful_drops; i++ ) {
			// do stats
			// TODO(performance): the counting can go when calculating all generations as you never need them
			if( is_end_state( next_gen[i] ) ) {
				if( player == WHITE ) {
					counters.wins_white++;
				} else {
					counters.wins_black++;
				}
			}
			// store
			output_boards[created++] = next_gen[i]; // Could probably copy all in 1 go
		}

		if( created + 7 > BLOCK_SIZE ) {
			// sort
			// write
			char block_file[255];
			sprintf( block_file, "%s/%016hu.block", destination_directory, block );
			block++;
			FILE* out = fopen( block_file, "w" );
            fwrite( output_boards, sizeof(board63), created, out );
		    fclose( out );
			created = 0;
		}
	}

	counters.cpu_time_used = ((double)( clock() - cpu_time_start ) / CLOCKS_PER_SEC );

	// Note: mmap maps in multiples of page size, but why does unmap need it?
	if( munmap( (void*)boards.head, (size_t) sysconf(_SC_PAGESIZE) ) == -1 ){
		perror("munmap");
	}


	write_counter( &counters, "gencounter.gc" );

}

/* getopt_long stores the option index here. */
global_variable int option_index = 0;

global_variable struct option long_options[] = {
	{"command",  		required_argument, 0, 'c'},
	{"source",  		required_argument, 0, 's'},
	{"destination",		optional_argument, 0, 'd'},
	{0, 0, 0, 0}
};


int main( int argc, char** argv ) {

	char* command = NULL;
	char* source = NULL;
	char* destination = NULL;

	int c;

	while( (c = getopt_long (argc, argv, "c:s:d:", long_options, &option_index) ) != -1 ) {

		switch( c ) {
		  case 's':
			 source = optarg;
		    break;
 		  case 'd':
 			 destination = optarg;
 		    break;
		  case 'c':
			 command = optarg;
		    break;
		  default:
		    abort();
		}
	}

	assert( command );
	assert( source );

	if( source == NULL ) {
		fprintf( stderr, "Source file missing [--source name]\n");
	}

	if( strcmp("stats", command) == 0 ) {
		print_stats( source );
	} else if( strcmp("nextgen", command) == 0 ) {
		next_generation( source, destination );
	} else {
		printf("Unknown command: %s\n", command);
	}

	return EXIT_SUCCESS;
}

