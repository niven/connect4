#include <ctype.h>
#include <math.h> /* ceil() */
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "base.h"

#include "c4types.h"
#include "board.h"
#include "board63.h"
#include "counter.h"
#include "bplustree.h"

// TODO(fail): define from something in board.h or something
#define ROW_SIZE_BYTES 27


internal void update_counters( gen_counter* gc, board* b ) {
	
	gc->total_boards++;
	gc->draws += is_draw( b );
	gc->wins_white += is_win_for( b, WHITE );
	gc->wins_black += is_win_for( b, BLACK );
	
	// unique boards?
}


/*
Generate all possible boards reading boards from file1, generating all possibles into file2
This includes duplicates etc

So this produces around this:
	


Where the generation_9.c4 file is 995 MB
*/

internal board* read_board(FILE* in, size_t board_index ) {
	
	long filepos = (long)board_index * ROW_SIZE_BYTES;
	if( fseek( in, filepos, SEEK_SET ) ) {
		perror("fseek()");
		printf("Most likely no such board index: %lu\n", board_index );
		return NULL;
	}
	
	
	return read_board_record( in );
	
}

internal void next_gen( const char* database_from, const char* database_to ) {
	
	gen_counter gc = { .total_boards = 0 }; // will init the rest to default, which is 0
	
	// open database_from
	// read 1 board
	// drop everywhere we can
	// write to database_to
	
	database* from = database_open( database_from );
	database* to = database_create( database_to );
	
	printf("Boards in database: %lu\n", from->header->table_row_count );
	
	board* start_board = NULL;

	for( size_t i=0; i<from->header->table_row_count; i++ ) {
		printf("Reading board %lu\n", i);
		start_board = read_board( from->table_file, i );

		// no need to go on after the game is over
		if( start_board->state & OVER ) {
			continue;
		}

		// try and make a move in every column
		for(int col=0; col<COLS; col++) {
			board* move_made = drop( start_board, col );
			if( move_made == NULL ) {
				printf("Can't drop in column %d\n", col);
			} else {
				update_counters( &gc, move_made );

				database_put( to, move_made );
				render( move_made, "GEN MOVE", false );
				free_board( move_made );
			}
		}

		if( start_board != NULL ) {
			free_board( start_board );
		}

	}

	database_close( from );
	database_close( to );
	
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
	
	/*
	Anyway, on to autogen
	I think board->enumerate possible moves->N boards in a stream
	then for every new board repeat.
	this does mean of course storing each board twice :(
	
	OTOH, as we know that white can always win, we can discard any move by white that ends in either black winning or draws.
	So if we go depth first we can prune rather large parts of the tree
	
	(and since no state we can create a nice REST API to always win ;)
	
	We can also prune every move that leads to opponent-win
	
	Maybe just create a hierarhy of directories, which have names encoding the board states.
	
	or files could encode the branches and have contents
	
	For debugging and firguring out what is going on: encode board state, read encoded board and render in ascii
	
	What is better is keeping every board as 88 bits/11 bytes and then storing them sorted on disk in 1 big
	file (aka a "database"). Since many different move sequences can lead to identical board states we save a lot of stuff.
	
	Whenever we hit a blackwin or draw we need to backpropagate to find and eliminate all white moves that lead to this point.
	this means we need the move sequence as well. Maybe? lemme think.
	
	*/

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
		key_t key = (key_t)atoi( key_str );
		board* b = NULL;//database_get( db, key );
		char buf[256];
		sprintf( buf, "%s - %lu", database_name, key );
		render( b, buf, true );

		free_board( b );
		database_close( db );
	}

	free_s2w();
	
	printf("Done\n");
	
	return 0;
}

