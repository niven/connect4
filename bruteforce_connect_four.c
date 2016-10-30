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
#include "bplustree.h"


internal void print_stats( const char* directory ) {
	
	char genfilename[256];
	printf("Node order: %d\tCache size: %lu\n", ORDER, CACHE_SIZE);
	printf("Gen\tTotal\tUnique\twins W\twins B\tCPU time (s)\tCache hit %%\tfilesize (MB)\n");
	for( int g=1; g<=42; g++ ) { // just try all possible and break when done


		sprintf(genfilename, "%s/gencounter_%d.gc", directory, g);
		struct stat gstat;
		if( stat( genfilename, &gstat ) == -1 ) {
			break;
		}
		gen_counter* gc = read_counter( genfilename );

		printf( "%d\t%lu\t%lu\t%lu\t%lu\t%f\t%f\t%.3f\n", g, gc->total_boards, gc->unique_boards, gc->wins_white, gc->wins_black, gc->cpu_time_used, 100.0f*gc->cache_hit_ratio, (double)gc->database_size/(double)megabyte(1) );

	}
	
}

internal void display_progress( size_t current, size_t total ) {

	if( total < 100 ) {
		return;
	}
	size_t one_percent = total / 100;
	if( current % one_percent == 0 ) {
		printf("%.2f%%\n", 100. * (double)current / (double)total);
	}
}

internal void next_generation( const char* database_from, const char* database_to ) {

	database* from = database_open( database_from );
	database* to = database_create( database_to );
	
	// do all 7 moves in 1 drop, avoiding mallocing a brazillian new boards
	board63 next_gen[7];
	
	// iterate over all boards in the db
	struct database_cursor cursor;
	database_init_cursor( from, &cursor );
	
	// gen next
	gen_counter counters;
	memset( &counters, 0, sizeof(gen_counter) );	

	// cpu timing
	clock_t cpu_time_start = clock();
	
	while( cursor.current < cursor.num_records ) {
		print("Retrieving record %lu", cursor.current);
		display_progress( cursor.current, cursor.num_records );
		
		board63 current_board63 = database_get_record( from, &cursor );
		
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
			bool was_insert = database_put( to, next_gen[i] );
			if( was_insert ) {
				counters.unique_boards++;
			}
		}

	}

	counters.cpu_time_used = ((double)( clock() - cpu_time_start ) / CLOCKS_PER_SEC );
	cache_stats cstats = get_database_cache_stats( to );
	counters.cache_hit_ratio = cstats.hit_ratio;
	
	database_dispose_cursor( &cursor );
	
	database_close( from );
	database_close( to );

	struct stat gstat;
	char index_filename[256];
	sprintf(index_filename, "%s.c4_index", database_to);
	
	int stat_result = stat( index_filename, &gstat);
	assert( stat_result != -1 );
	counters.database_size = (unsigned long)gstat.st_size;
	
	write_counter( &counters, "gencounter.gc" );
	
}

/* getopt_long stores the option index here. */
global_variable int option_index = 0;

global_variable struct option long_options[] = {
	{"command",  		required_argument, 0, 'c'},
	{"source",  		required_argument, 0, 's'},
	{"destination",	optional_argument, 0, 'd'},
	{0, 0, 0, 0}
};


int main( int argc, char** argv ) {

	char* command = NULL;
	char* source = NULL;
	char* destination = NULL;

	int c;	

	while( (c = getopt_long (argc, argv, "ts:d:r:", long_options, &option_index) ) != -1 ) {

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
		fprintf( stderr, "Required database name missing [--source name]\n");
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

