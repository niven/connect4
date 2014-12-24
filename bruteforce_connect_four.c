#include <assert.h>
#include <ctype.h>
#include <math.h> /* ceil() */
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
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

internal void next_gen( const char* database_from, const char* database_to ) {
	
	gen_counter gc = { .total_boards = 0 }; // will init the rest to default, which is 0
	
	clock_t cpu_time_start = clock();
	// open database_from
	// read 1 board
	// drop everywhere we can
	// write to database_to
	
	database* from = database_open( database_from );
	database* to = database_create( database_to );
	
	printf("Boards in database: %lu\n", from->header->table_row_count );
	
	// Reading a board: go to the first node in the index, skip until you find a leaf node
	// then iterate over the keys, looking up the offset for the board state+winlines
	// construct a board and do the thing.
	// whenever a node is done, go to the next one (and maybe skip until yout hit a leaf)
	
	struct stat index_file_stat;
	struct stat table_file_stat;

	fstat( fileno(from->table_file), &table_file_stat );
	fstat( fileno(from->index_file), &index_file_stat );

	// TODO(research): find out if we can just effecitively mmap any file size
	assert( index_file_stat.st_size < (off_t)gigabyte(1) );
	assert( table_file_stat.st_size < (off_t)gigabyte(1) );

	// Check filesize against what the db thinks
	assert( sizeof(*(from->header)) + (sizeof(node) * from->header->node_count) == (size_t)index_file_stat.st_size );
	assert( (SIZE_BOARD_STATE_BYTES + 2*NUM_WINLINE_BYTES) * from->header->table_row_count == (size_t)table_file_stat.st_size);
	

	// mmap the index and the board

	char* node_data = mmap( NULL, (size_t)index_file_stat.st_size, PROT_READ, MAP_PRIVATE, fileno(from->index_file), 0 );
	assert( node_data != MAP_FAILED );
	char* board_data = mmap( NULL, (size_t)table_file_stat.st_size, PROT_READ, MAP_PRIVATE, fileno(from->table_file), 0 );
	assert( board_data != MAP_FAILED );

	// start at the first node
	size_t node_counter = 0;
	assert( from->header->node_count >= 1 );
	node* current_node = (node*)malloc( sizeof(node) );

	// just a timer to give regular output
	time_t start_time = time( NULL ), next_time;
	size_t boards_in_interval = 0;

	// read every node
	size_t board_counter = 0;
	board* start_board = NULL;

	while( node_counter < from->header->node_count ) {
		
		node_counter++; // ensure we start at 1, since node 0 doesn't exist
		off_t node_offset = file_offset_from_node( node_counter );
		memcpy( current_node, &node_data[node_offset], sizeof(node) );
		// printf("Current node %lu, leaf: %s\n", current_node->id, current_node->is_leaf ? "true" : "false");
		
		if( !current_node->is_leaf ) { // skip over nonleaf nodes
			continue;
		}
		
		// printf("Reading %lu boards from node %lu\n", current_node->num_keys, current_node->id );
	
		// read every board
		for( size_t key_index = 0; key_index < current_node->num_keys; key_index++ ) {
			
			board_counter++;
			
			// show some progress output
			if( time( &next_time ) - start_time > 2 ) {
				double percentage_done = 100.0f * ((double)board_counter / (double)from->header->table_row_count);
				printf("Reading board %lu/%lu - %.2f%% (generating %.2f K boards/sec)\n", board_counter, from->header->table_row_count, percentage_done, (double)(boards_in_interval/1000)/(double)(next_time-start_time));
				start_time = next_time;
				boards_in_interval = 0;
			} 		

			// read this specific board
			board63 encoded_board = current_node->keys[key_index];
			off_t board_data_offset = file_offset_from_row_index( current_node->pointers[key_index].board_data_index );
			// printf("Read form offset %llu\n", board_data_offset);
			start_board = read_board_record_from_buf( encoded_board, board_data, board_data_offset );
			// render( start_board, "Start", false );
			// no need to go on after the game is over
			if( start_board->state & OVER ) {
				continue;
			}
			
			// try and make a move in every column
			for(int col=0; col<COLS; col++) {
				
				board* move_made = drop( start_board, col );

				if( move_made == NULL ) {
					print("Can't drop in column %d (column full)", col);
				} else {
					boards_in_interval++;
					assert( encode_board( move_made) != 0 ); // the empty board encodes to 0, so no other board should
					// there are multiple ways to win that lead to the same board, but we are counting ways to win, not unique wins.
					update_counters( &gc, move_made );

					// if( is_over(move_made) ) {
					// 	printf("over: %lx\n", encode_board( move_made) );
					// 	render( move_made, "GAMEOVER", false );
					// 	render( start_board, "FROM", false );
					//
					// }
					bool was_insert = database_put( to, move_made );
					gc.unique_boards += was_insert;

					free_board( move_made );
				}
			}

			// TODO(bug?): Not sure why this could be NULL
			if( start_board != NULL ) {
				free_board( start_board );
			}
			
		}

		// printf("Finished with node %lu\n", current_node->id);
		
	}
	assert( node_counter == from->header->node_count );
	assert( board_counter == from->header->table_row_count );
	
	printf("Stats for destination database:\n");
	print_database_stats( to );

	database_close( from );
	database_close( to );

	printf("Generated %lu boards\n", gc.unique_boards);
	

	gc.cpu_time_used = ((double)( clock() - cpu_time_start ) / CLOCKS_PER_SEC );
	cache_stats stats = get_database_cache_stats( to );
	gc.cache_hit_ratio = stats.hit_ratio;
	
	munmap( node_data, (size_t) index_file_stat.st_size );
	munmap( board_data, (size_t) table_file_stat.st_size );
	
	write_counter( &gc, "gencounter.gc" );

}


int main( int argc, char** argv ) {

	char* create_sequence = NULL;
	char* generate = NULL;
	int ascii_flag = 0;
	int read_flag = 0;
	int gc_flag = 0;
	char* database_name = NULL;
	char* key_str = NULL;
	int c;	

	map_squares_to_winlines(); // could be static but don't like doing it by hand



	while( (c = getopt (argc, argv, "gc:a:d:rn:") ) != -1 ) {

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
			database_put( db, current );
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
			database_put( db, next );
			render( next, create_sequence, false );
			//print_winlines( next->winlines );

			free_board( current );
			current = next;
		}
		
		free_board( next );
		database_close( db );
	}
	
	if( generate != NULL ) {
		
		next_gen( database_name, generate );
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
		printf("Node order: %d\tCache buckets: %lu\tCache max: %lu\n", ORDER, CACHE_BUCKETS, CACHE_MAX);
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

