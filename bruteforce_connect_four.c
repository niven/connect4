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

#define BLOCK_SIZE (1 * 1 * 1000)

internal void display_progress( size_t current, size_t total ) {

	if( total < 100 ) {
		return;
	}
	size_t one_percent = total / 100;
	if( current % one_percent == 0 ) {
		printf("\r%.2f%%", 100. * (double)current / (double)total);
	}
}

internal int compare_boards(const void* a, const void* b) {
    board63 arg1 = *(const board63*)a;
    board63 arg2 = *(const board63*)b;
 
    if (arg1 < arg2) return -1;
    if (arg1 > arg2) return 1;
    return 0;
}
 

internal void write_block( const char* destination_directory, uint16 index, uint64 count, board63 boards[] ) {

	// sort so merging is easy and can eliminate duplicates
	qsort(boards, count, sizeof(board63), compare_boards);

	// write
	char block_file[255];
	sprintf( block_file, "%s/%016hu.block", destination_directory, index );
	FILE* out = fopen( block_file, "w" );
	fwrite( boards, sizeof(board63), count, out );
	fclose( out );
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
		boards.current++;
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
			output_boards[created++] = next_gen[i]; // Could probably copy all in 1 go
		}

		// Store and ensure we never exceed the block size
		if( created + 7 > BLOCK_SIZE ) {
			write_block( destination_directory, block, created, output_boards);
		}
	}

	// write the final block
	if( created > 0 ) {
		write_block( destination_directory, block, created, output_boards);
	}


	counters.cpu_time_used = ((double)( clock() - cpu_time_start ) / CLOCKS_PER_SEC );

	// Note: mmap maps in multiples of page size, but why does unmap need it?
	if( munmap( (void*)boards.head, (size_t) sysconf(_SC_PAGESIZE) ) == -1 ){
		perror("munmap");
	}


	char stats_file[255];
	sprintf( stats_file, "%s/stats.txt", destination_directory );
	FILE* stats;
	FOPEN_CHECK( stats, stats_file, "w" )
	fprintf(stats, "CPU time: %f\n", counters.cpu_time_used );
	fprintf(stats, "Total boards: %ld\n", counters.total_boards );
	fprintf(stats, "Unique boards: %ld\n", counters.unique_boards );
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

	exit( EXIT_SUCCESS );
}

