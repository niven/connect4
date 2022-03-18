#include <assert.h>
#include <ctype.h>
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
#include "utils.h"

#include "board.h"

global_variable const uint64 BLOCK_SIZE = UINT64_C(32 * 1000 * 1000);
global_variable gen_counter counters;


internal int compare_boards(const void* a, const void* b) {
    board63 arg1 = *(const board63*)a;
    board63 arg2 = *(const board63*)b;
 
    if (arg1 < arg2) return -1;
    if (arg1 > arg2) return 1;
    return 0;
}
 

internal void write_block( const char* destination_directory, uint16 index, uint64 count, board63 boards[] ) {

	print("Writing block %hu with %lu boards", index, count);

	// sort so merging is easy and can eliminate duplicates
	clock_t cpu_time_start = clock();
	qsort(boards, count, sizeof(board63), compare_boards);
	counters.cpu_time_sort = ((double)( clock() - cpu_time_start ) / CLOCKS_PER_SEC );

	// write
	char block_file[255];
	sprintf( block_file, "%s/%08hu.block", destination_directory, index );
	FILE* out = fopen( block_file, "w" );
	uint64 previous = 0;
	for( uint64 i=0; i<count; i++ ) {
		print("Writing %016lx", boards[i]);
		varint_write( boards[i] - previous, out );
		previous = boards[i];
	}
	fclose( out );
}

internal void next_generation( const char* source_file, const char* destination_directory ) {


	board63* output_boards = malloc( sizeof(board63) * BLOCK_SIZE );
	if( output_boards == NULL ) {
		perror("Could not allocate memory for output boards. Maybe BLOCK_SIZE is too large");
		exit( EXIT_FAILURE );
	}

	uint64 created = 0;

	// to number the blocks we output
	uint16 block = 0;

	// do all 7 moves in 1 drop, avoiding mallocing a brazillian new boards
	board63 next_gen[7];

	// iterate over all boards
	entry_v boards = map( source_file );
	uint64 boards_total_bytes = boards.remaining_bytes;

	// gen next
	memset( &counters, 0, sizeof(gen_counter) );

	// cpu timing
	clock_t cpu_time_start = clock();

	// to display progress percentage
	uint16 progress_counter = 0;

	board63 current_board63;
	board current_board;
	uint8 player;
	uint8 num_succesful_drops;

	while( boards.remaining_bytes > 0 || boards.consumed < boards.read ) {
		print("Remaining bytes: %lu read: %lu consumed: %lu", boards.remaining_bytes, boards.read, boards.consumed );

		current_board63 = boards.value;
		entry_next( &boards ); // fetch the next one here, so we always have one before the end_state check
		boards.consumed++; // always consume unlike merge

		if( is_end_state( current_board63 ) ) {
			continue;
		}

		decode_board63( current_board63, &current_board );
		// char title[255];
		// sprintf(title, "Multidrop board %016lx", current_board63 );
		// render( &current_board, title, false);

		num_succesful_drops = multidrop( &current_board, next_gen );
		print("Got %d drops", num_succesful_drops);
		counters.total_boards += num_succesful_drops;
		for( int i=0; i<num_succesful_drops; i++ ) {
			// do stats
			// TODO(performance): the counting can go when calculating all generations as you never need them
			if( is_end_state( next_gen[i] ) ) {
				player = current_player( &current_board );
				if( player == WHITE ) {
					counters.wins_white++;
				} else {
					counters.wins_black++;
				}
			}
			output_boards[created++] = next_gen[i]; // Could probably copy all in 1 go
		}

		// Store and ensure we never exceed the block size
		if( created > BLOCK_SIZE ) {
			display_progress( boards_total_bytes - boards.remaining_bytes, boards_total_bytes );
			write_block( destination_directory, block++, created, output_boards);
			created = 0;
		}

	}

	// write the final block
	if( created > 0 ) {
		write_block( destination_directory, block, created, output_boards);
	}
	// write a 100% completeness
	display_progress( boards_total_bytes, boards_total_bytes );


	counters.cpu_time_generate = ((double)( clock() - cpu_time_start ) / CLOCKS_PER_SEC );

	free( output_boards );

	// Note: mmap maps in multiples of page size, but why does unmap need it?
	if( munmap( (void*)boards.head, (size_t) sysconf(_SC_PAGESIZE) ) == -1 ){
		perror("munmap");
	}


	char stats_file[255];
	sprintf( stats_file, "%s/stats.txt", destination_directory );
	FILE* stats;
	FOPEN_CHECK( stats, stats_file, "w" )
	fprintf(stats, "CPU time generate: %f\n", counters.cpu_time_generate );
	fprintf(stats, "CPU time sorting:  %f\n", counters.cpu_time_sort );
	fprintf(stats, "Total boards: %ld\n", counters.total_boards );
	fprintf(stats, "Wins white: %ld\n", counters.wins_white );
	fprintf(stats, "Wins black: %ld\n", counters.wins_black );
	fprintf(stats, "Draws: %ld\n", counters.draws );
	fclose( stats );

}

int main( int argc, char** argv ) {
	char* source_file = NULL;
	char* destination_directory = NULL;

	if( argc != 3 ) {
		printf("Usage: bfcf source_file destination_directory\n");
		exit( EXIT_SUCCESS );
	}

	source_file = argv[1];
	destination_directory = argv[2];

	// TODO: check file and dir

	next_generation( source_file, destination_directory );

	printf("\n");

	exit( EXIT_SUCCESS );
}

