#include <assert.h>
#include <getopt.h>
#include <string.h>

#include "base.h"

#include "board.h"
#include "bplustree.h"

/* getopt_long stores the option index here. */
global_variable int option_index = 0;

global_variable struct option long_options[] = {
	{"destination",  	required_argument, 0, 'd'}, // last value is an int that is the id for this option. Using a char makes this convenient to overlap with a shortopt
	{"moves",  			optional_argument, 0, 'm'},
	{0,0,0,0} // needs to be terminated with all 0s as per docs
};


int main( int argc, char** argv ) {

	char* moves = NULL;
	char* destination = NULL;

	int c;	

	while( (c = getopt_long (argc, argv, "c:d:", long_options, &option_index) ) != -1 ) {

		switch( c ) {
			case 'm':
				moves = optarg;		
				break;
			case 'd':
				destination = optarg;		
				break;
		}
	}

	assert( destination );
	
	board* current = new_board();
	database* db = database_create( destination );

	if( moves == NULL ) {
		prints("Creating database with an empty board");
		database_store( db, current );
		free_board( current );
		printf("Nodes %u, Rows: %llu, Root Node ID: %u\n", db->header->node_count, db->header->table_row_count, db->header->root_node_id);
		database_close( db );
		exit( EXIT_SUCCESS );
	}
	 
	map_squares_to_winlines(); // could be static but don't like doing it by hand 
	 
	board* next = NULL;
	for( size_t i=0; i<strlen(moves); i++ ) {
		int col_index = moves[i] - '0';
		print("Drop in column %d", col_index);
		next = drop( current, col_index );
		if( next == NULL ) {
			fprintf( stderr, "Illegal drop in column %d\n", col_index );
			abort();
		}
		database_store( db, next );
		render( next, moves, false );

		free_board( current );
		current = next;
	}
	
	render( next, "Final Board", false );
	free_board( next );
	database_close( db );
	
	printf("Done.\n");
	
	return 0;
}

