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

#include "c4types.h"
#include "board.h"
#include "board63.h"
#include "counter.h"



void update_counters( gen_counter* gc, board* b ) {
	
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
	
Gen	Boards
0	1
1	7
2	49
3	343
4	2,401
5	16,807
6	117,649
7	823,536
8	5,673,234
9	39,394,572

Where the generation_9.c4 file is 995 MB
*/
void generate_option_1( const char* filename ) {
	
	gen_counter gc = { .total_boards = 0 }; // will init the rest to default, which is 0
	
	// open file with N boards (including winlines), open out file
	// read 1 board
	// drop everywhere we can
	// write to out file
	
	FILE* in = fopen( filename, "rb" );
	if( in == NULL ) {
		perror("fopen()");
		exit( EXIT_FAILURE );
	}
	FILE* out = fopen( "next_generation.c4", "wb" );
	if( out == NULL ) {
		perror("fopen()");
		exit( EXIT_FAILURE );
	}
	
	board* start_board = NULL;
	size_t boards_read = 0;
	while( (start_board = read_board_record( in )) != NULL ) {
		boards_read++;
//		printf("\nRead board %ld\n", boards_read);
//		render( start_board, "board_record used as src", false );
//		printf("Player to make moves: %s\n", current_player( start_board ) == WHITE ? "White" : "Black");

		// no need to go on after the game is over
		if( start_board->state & OVER ) {
			continue;
		}

		// try and make a move in every column
		for(int col=0; col<COLS; col++) {
			board* move_made = drop( start_board, col );
			if( move_made == NULL ) {
//				printf("Can't drop in column %d\n", col);
			} else {
				update_counters( &gc, move_made );

				write_board_record( move_made, out );
				//render( move_made, "GEN MOVE", false );
				free_board( move_made );
			}
		}

		free_board( start_board );

	}
	
	fclose( in );
	fclose( out );
	
	
	write_counter( &gc, "gencounter.gc" );

}


int main( int argc, char** argv ) {

	char* create_sequence = NULL;
	char* generate_option = NULL;
	int ascii_flag = 0;
	int read_flag = 0;
	int gc_flag = 0;
	char* filename = NULL;
	int c;	

	map_squares_to_winlines(); // could be static but don't like doing it by hand

	while( (c = getopt (argc, argv, "gc:af:rn:") ) != -1 ) {

		switch( c ) {
 		  case 'c': // create from drop sequence
 			 create_sequence = optarg;
 		    break;
  		  case 'f': // create from drop sequence
  			 filename = optarg;
  		    break;
 		  case 'a': // render filename
 			 ascii_flag = 1;
 		    break;
 		  case 'g': // show generation counter
 			 gc_flag = 1;
 		    break;
		  case 'n': // next moves for filename
 			 generate_option = optarg;
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

	printf ("filename = %s, create_sequence = %s, ascii_flag = %d, read_flag = %d, next_moves = %s, gc_flag = %d\n", filename, create_sequence, ascii_flag, read_flag, generate_option, gc_flag);

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

	if( filename == NULL ) {
		fprintf( stderr, "Required filename missing (-f ...)\n");
	}

	if( create_sequence != NULL ) {

		board* current = new_board();
		
		if( create_sequence[0] == 'e' ) {
			write_board( filename, current );
			free_board( current );
			exit( EXIT_SUCCESS );
		}
		 
		board* next = NULL;
		for( int i=0; i<strlen(create_sequence); i++ ) {
			int col_index = create_sequence[i] - '0';
			printf("Drop in column %d\n", col_index);
			next = drop( current, col_index );
			if( next == NULL ) {
				fprintf( stderr, "Illegal drop in column %d\n", col_index );
				abort();
			}
			render( next, create_sequence, false );
			//print_winlines( next->winlines );

			free_board( current );
			current = next;
		}
		
		write_board( filename, current );
		free_board( next );

	}
	
	if( generate_option != NULL ) {
		
		switch( generate_option[0] ) {
			case '1':
				generate_option_1( filename );
				break;
			default:
				fprintf( stderr, "Unknown generation option: %c\n", generate_option[0] );
				exit( EXIT_FAILURE );
		}

	}

	if( ascii_flag ) {

		board* b = read_board( filename );
		render( b, filename, true );

		free_board( b );
	}

	free_s2w();
	
	if( gc_flag ) {
		gen_counter* gc = read_counter( filename );
		print_counter( gc );
		free( gc );
	}
	
	printf("Done\n");
	
	return 0;
}

