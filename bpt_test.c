#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


#include "base.h"

#include "utils.h"
#include "board.h"

#include "bplustree.h"


internal void test_header( const char* title ) {

	char row[] = "#####################################################";
	row[ strlen(title)+4 ] = '\0';
	printf("\n%s\n# %s #\n%s\n", row, title, row);

}

internal void test_dupes() {

	test_header( "Intentionally storing dupes (should be ignored)" );

	database_create( "dupes" );
	database* db = database_open( "dupes", DATABASE_READ | DATABASE_WRITE );

	board* current = new_board();

	size_t COUNT = 10;
	int drops[10] = {3, 4, 1, 2, 5, 1,  4, 6, 3,  4};
	bool was_insert;
	for(size_t i=0; i<COUNT; i++) {
		board* next = drop( current, drops[i] );
		render( next, "Board", false );
		was_insert = database_store( db, next );
		assert( was_insert );
		free_board( next );

		next = drop( current, drops[i] );
		printf(">>>>>> Checking of dupe board is not inserted\n");
		render( next, "Dupe board", false );
		was_insert = database_store( db, next );
		assert( !was_insert );

		free_board( current );
		current = next;

		print_index( db );
	}

	node* root_node = load_node_from_file( db, db->header->root_node_id );
	assert( bpt_size( db, root_node ) == COUNT );
	release_node( db, root_node );

	assert( db->header->table_row_count == COUNT );

	free_board( current );

	print_database_stats( db );

	database_close( db );

}


internal void test_store_cmdline_seq( char* seq ) {

	database_create( "test_store_cmdline_seq" );
	database* db = database_open( "test_store_cmdline_seq", DATABASE_READ | DATABASE_WRITE );

	printf("Sequence: %s\n", seq);
	char* element = strtok( seq, "," );

	board* current = new_board();

	while( element != NULL ) {

		int col_index = atoi(element);
		printf("\n>>>>> Insert Board with move in col %d\n", col_index );

		board* next = drop( current, col_index );

		if( next == NULL ) {
			fprintf( stderr, "Illegal drop in column %d\n", col_index );
			exit( EXIT_FAILURE );
		}
		render( next, element, false );

		database_store( db, next );
		board63 key = encode_board( next );
		printf(">>>>>>>> inserted key: %lx\n", key);
		board retrieved;
		bool exists = database_get( db, key );
		assert( exists );
		decode_board63( key, &retrieved );
		render( &retrieved, "result from db_get", false);
		assert( encode_board( &retrieved ) == key );


		free_board( current );
		current = next;

		printf("<<<<< After insert\n" );
		print_index( db );


		element = strtok( NULL, "," );
	}

	free_board( current );

	print_database_stats( db );

	database_close( db );
}

int main(int argc, char** argv) {

	// TODO(stupid): if you don't call this you get segfaults. maybe board.o can call this itself somehow?
	map_squares_to_winlines();

	printf("ORDER %d, SPLIT_KEY_INDEX %d\n", ORDER, SPLIT_KEY_INDEX );

	// run cmd line stuff OR all tests
	if( argc == 2 ){
		test_store_cmdline_seq( argv[1] );
		exit( EXIT_SUCCESS );
	}

	test_dupes();

	printf("Done\n");
}
