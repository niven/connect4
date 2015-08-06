#include <assert.h>
#include <ctype.h>
#include <getopt.h>
#include <math.h> /* ceil() */
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
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


internal void update_counters( gen_counter* gc, board* b ) {
	
	gc->total_boards++;
	gc->draws += is_draw( b );
	gc->wins_white += is_win_for( b, WHITE );
	gc->wins_black += is_win_for( b, BLACK );
	
	// unique boards?
}

// take the boards of a generation, backtrack to previous gen for the perfect game
#if 0
internal void backtrack_perfect_game( int generation ) {
	
	/*
		if this is the last gen (42):
			Check there are only wins for black or draws

		if this is the penultimate gen (41):
			write a file with all winning boards for white, add a branch id for every board, set #children to 0
	
		otherwise:
			write all good/draw/ongoing boards to a new file, with data: branch id, number of children+1

	*/
	
}
#endif

// cpu timing
// clock_t cpu_time_start = clock();
// gc.cpu_time_used = ((double)( clock() - cpu_time_start ) / CLOCKS_PER_SEC );


internal void next_gen_next_gen( const char* database_from, const char* database_to ) {

	database* from = database_open( database_from );
	database* to = database_create( database_to );
	
	// do all 7 moves in 1 drop, avoiding mallocing a brazillian new boards
	board63 next_gen[7];
	
	// iterate over all boards in the db
	struct database_cursor cursor;
	database_init_cursor( from, &cursor );
	
	// gen next
	size_t num_unique_boards = 0;
	while( cursor.current < cursor.num_records ) {
		print("Retrieving record %lu", cursor.current);
		board63 current_board63 = database_get_record( from, &cursor );
		
		if( is_end_state( current_board63 ) ) {
			continue;
		}
		
		board current_board;
		decode_board63( current_board63, &current_board );
		render( &current_board, "Multidrop for", false);
		int num_succesful_drops = multidrop( &current_board, next_gen );
		print("Got %d drops", num_succesful_drops);
		for( int i=0; i<num_succesful_drops; i++ ) {
			// do stats
			// store
			bool was_insert = database_put( to, next_gen[i] );
			if( was_insert ) {
				num_unique_boards++;
			}
		}

	}
	
	database_dispose_cursor( &cursor );
	
	database_close( from );
	database_close( to );
	
}



int main( int argc, char** argv ) {

	char* create_sequence = NULL;
	char* generate = NULL;
	char* dest_db = NULL;
	int ascii_flag = 0;
	int read_flag = 0;
	int gc_flag = 0;
	char* database_name = NULL;
	char* key_str = NULL;
	int c;	

	map_squares_to_winlines(); // could be static but don't like doing it by hand



	while( (c = getopt (argc, argv, "gc:a:d:rn:x:") ) != -1 ) {

		switch( c ) {
 		  case 'c': // create from drop sequence
 			 create_sequence = optarg;
 		    break;
  		  case 'd':
  			 database_name = optarg;
  		    break;
 		  case 'a': // render board from key
 			 key_str = optarg;
 		    break;
		  case 'g': // show all generation counters
			 gc_flag = 1;
		    break;
  		  case 'n': // next moves for database
   			 generate = optarg;
   		    break;
  		  case 'x': // next moves for database
   			 dest_db = optarg;
   		    break;
  		  case 'r': // read filename
  			 read_flag = 1;
  		    break;
		  case '?':
		    if (optopt == 'c')
		      fprintf (stderr, "Option -%c requires an argument.\n", optopt);
		    else if (isprint (optopt))
		      fprintf (stderr, "Unknown option `-%c'.\n", optopt);
		    else
		      fprintf(stderr,"Unknown option character `\\x%x'.\n", optopt);
		    return 1;
		  default:
		    abort();
		}
	}

	printf ("database = %s, create_sequence = %s, ascii_flag = %d, read_flag = %d, generate to = %s, gc_flag = %d\n", database_name, create_sequence, ascii_flag, read_flag, generate, gc_flag);

	for( int index = optind; index < argc; index++ ) {
		printf("Non-option argument %s\n", argv[index]);
	}

	if( database_name == NULL ) {
		fprintf( stderr, "Required database name missing (-d ...)\n");
	}

	if( create_sequence != NULL ) {

		board* current = new_board();
		database* db = database_create( database_name );

		if( create_sequence[0] == 'e' ) {
			database_store( db, current );
			free_board( current );
			database_close( db );
			exit( EXIT_SUCCESS );
		}
		 
		board* next = NULL;
		for( size_t i=0; i<strlen(create_sequence); i++ ) {
			int col_index = create_sequence[i] - '0';
			printf("Drop in column %d\n", col_index);
			next = drop( current, col_index );
			if( next == NULL ) {
				fprintf( stderr, "Illegal drop in column %d\n", col_index );
				abort();
			}
			database_store( db, next );
			render( next, create_sequence, false );
			//print_winlines( next->winlines );

			free_board( current );
			current = next;
		}
		
		free_board( next );
		database_close( db );
	}
	
	if( dest_db != NULL ) {		
		next_gen_next_gen( database_name, dest_db );
	}

	if( ascii_flag ) {

		database* db = database_open( database_name );
		board63 key = (board63)atoi( key_str );
		board* b = NULL;//database_get( db, key );
		char buf[256];
		sprintf( buf, "%s - %lu", database_name, key );
		render( b, buf, true );

		free_board( b );
		database_close( db );
	}
	
	if( gc_flag ) {

		char genfilename[256];
		char idxfilename[256];
		printf("Node order: %d\tCache size: %lu\n", ORDER, CACHE_SIZE);
		printf("Gen\tTotal\tUnique\twins W\twins B\tCPU time (s)\tCache hit %%\tfilesize (MB)\n");
		for( int g=1; g<=42; g++ ) { // just try all possible and break when done

			sprintf(idxfilename, "%s/num_moves_%d.c4_index", database_name, g);
			struct stat gstat;
			if( stat( idxfilename, &gstat ) == -1 ) {
				break;
			}

			sprintf(genfilename, "%s/gencounter_%d.gc", database_name, g);
			gen_counter* gc = read_counter( genfilename );

			printf( "%d\t%lu\t%lu\t%lu\t%lu\t%f\t%f\t%.3f\n", g, gc->total_boards, gc->unique_boards, gc->wins_white, gc->wins_black, gc->cpu_time_used, 100.0f*gc->cache_hit_ratio, (double)gstat.st_size/(double)megabyte(1) );

		}

	}

	free_s2w();
	
	printf("Done\n");
	
	return 0;
}

