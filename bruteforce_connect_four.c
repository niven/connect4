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

internal board* read_board_from_mmap(char* data, size_t board_index ) {
	
	unsigned long long board_pos = (unsigned long long)board_index * BOARD_SERIALIZATION_NUM_BYTES;

	return read_board_record_from_buf( data, board_pos );	
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
	
	// TODO: also map the board table
	
	struct stat index_file_stat;
	fstat( fileno(from->index_file), &index_file_stat );
	size_t nodes_in_file = (sizeof( from->header) - (size_t)index_file_stat.st_size) / sizeof(node);
	printf("File size: %lld bytes, %lu bytes/node -> %zu nodes\n", index_file_stat.st_size, sizeof(node), nodes_in_file );

	// TODO(research): find out if we can just effecitively mmap any file size
	assert( nodes_in_file.st_size < 1 * 1024 * 1024 * 1024 );
	char* node_data = mmap( NULL, (size_t)index_file_stat.st_size, PROT_READ, MAP_PRIVATE, fileno(from->index_file), 0 );
	assert( node_data != MAP_FAILED );

	size_t node_offset = sizeof( from->header ); // this is where the first node starts
	node* current_node = (node*) node_data[node_offset];
	while( !current_node->is_leaf ) {
		node_offset += sizeof(node);
		current_node = (node*) node_data[node_offset];
	}
	printf("Reading keys from node %lu\n", current_node->id );
	board* start_board = NULL;
	// char scratch[256];
	time_t start_time = time( NULL );
	time_t next_time;
	for( size_t i=0; i<from->header->table_row_count; i++ ) {
		
		if( time( &next_time ) - start_time > 2 ) {
			double percentage_done = 100.0f * ((double)i / (double)from->header->table_row_count);
			printf("Reading board %lu/%lu - %.2f%% (%.0f boards/sec)\n", i, from->header->table_row_count, percentage_done, (double)i/(double)(next_time-start_time));
			start_time = next_time;
		} 		

		// TODO: get the next board63 from the node (a leaf one, otherwise just goto next node)
		// then lookup the state+winlines from the table
		start_board = read_board_from_mmap( board_data, i );
		
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
				assert( encode_board( move_made) != 0 ); // the empty board encodes to 0, so no other board should
				update_counters( &gc, move_made );
				
				bool was_insert = database_put( to, move_made );

				if( was_insert ) {
					gc.unique_boards++;
					// sprintf( scratch, "Unique board - %lu - %d", i, col );
					// render( move_made, scratch, false );
				} else {
					// sprintf( scratch, "Dupe board - %lu - %d", i, col );
					// render( move_made, scratch, false );
				}

				free_board( move_made );
			}
		}

		if( start_board != NULL ) {
			free_board( start_board );
		}

	}

	database_close( from );
	database_close( to );
	
	gc.cpu_time_used = ((double)( clock() - cpu_time_start ) / CLOCKS_PER_SEC );
	
	munmap( node_data, (size_t) node_file_stat.st_size );
	
	write_counter( &gc, "gencounter.gc" );

   bpt_dump_cf();
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
 		  case 'g': // show generation counter
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

		gen_counter* g = read_counter( database_name );
		print_counter( g );

	}

	free_s2w();
	
	printf("Done\n");
	
	return 0;
}

