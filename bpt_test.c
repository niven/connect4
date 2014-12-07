#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h> 


// TODO(clarity): fix this mess of includes
#include "base.h"
#include "c4types.h"

#include "board.h"
#include "board63.h"

#include "bplustree.h"

// GCD for only positive ints and not caring about m==n==0 returning 0
internal size_t gcd(size_t m, size_t n) {
    if(m == 0 && n == 0)
        return 0;

    size_t r;
    while(n) {
        r = m % n;
        m = n;
        n = r;
    }
    return m;
}

internal void test_header( const char* title ) {
	
	char row[] = "#####################################################";
	row[ strlen(title)+4 ] = '\0';
	printf("\n%s\n# %s #\n%s\n", row, title, row);
	
}

// TODO(testing): something with these tests
/*
internal void test_store_randomly() {

	test_header( "Store 0..100 in random order");
	
	srand( (unsigned int)time(NULL) );
	const size_t max = 1 * 1000 * 1000;
	// find a relative prime
	size_t relprime = (size_t)rand() % max;
	while( gcd(max, relprime) != 1 ) {
		relprime = (size_t)rand() % max;
	}
	printf("Using relative prime %lu %d\n", relprime, rand());
	size_t current = relprime;
	bpt* store = new_bptree();
	size_t count = 0;
	do {
		if( (count % (max/100)) == 0 ) {
			printf("inserted %lu%%\n", (count*100)/max);
		}
		bpt_put( &store, (struct record){ .key = current, { .table_row_index = count } });
		record* r = bpt_get( store, current );
		assert( r != NULL );
		assert( r->key == current );
		assert( r->value.table_row_index == count );
		// not doing a size() test here since it's very slow
//		assert( bpt_size( store ) == (count+1) );
		count++;
		current = (current + relprime) % max;
	} while( current != relprime );

#ifdef VERBOSE
		printf("Randomly insert result:\n");
		bpt_print( store, 0 );
#endif
	
	free_bptree( store );
}

internal void test_store_10() {
	
	test_header( "Store 10 records and count" );
	size_t max = 10;
	bpt* store = new_bptree();
	printf("Test store: %d\n", 10);
	for( size_t i=0; i<max; i++ ) {
		key_t k = (key_t)i;
		bpt_put( &store, (struct record){ .key = k, { .table_row_index = i } } );
		printf("### Result after storing %lu records\n", i+1);
		bpt_print( store, 0 );
		record* r = bpt_get( store, (key_t)i );
		assert( r != NULL );
		assert( r->key == k );
		assert( r->value.table_row_index == i );
		assert( bpt_size( store ) == (i+1) );
		printf("### Test OK - store %lu records\n", i+1);
	}
	printf("Repeat find\n");
	// repeat the finding to ensure we didn't remove or lose stuff
	for( size_t i=0; i<max; i++ ) {
		key_t k = (key_t)i;
		record* r = bpt_get( store, k );
		assert( r != NULL );
		assert( r->key == k );
		assert( r->value.table_row_index == i );
	}
	printf("Repeat find\n");
	free_bptree( store );
	printf("Repeat find\n");
	
}

internal void test_search_nonexisting_items() {
	bpt* store = new_bptree();

	key_t keys[6] = { 1, 3, 5, 7, 9, 11 };
	for( size_t i=0; i<6; i++ ) {		
		bpt_put( &store, (struct record){ .key = keys[i], { .table_row_index = i} } );
	}

	key_t missing_keys[6] = { 0, 2, 4, 6, 8, 10 };
	for( int i=0; i<6; i++ ) {		
		record* r = bpt_get( store, missing_keys[i] );
		assert( r == NULL );
	}

	free_bptree( store );
	
}

internal void test_store_random() {
	bpt* store = new_bptree();
	size_t num_rands = 20;
	for( size_t i=0; i<num_rands; i++ ) {
		key_t num = rand()%1024;
		bpt_put( &store, (struct record){ .key = num, { .table_row_index = i} } );
		record* r = bpt_get( store, num );
		assert( r != NULL );
		assert( r->key == num );
	}
	printf("\nRand filled tree:\n");
	bpt_print( store, 0 );
	free_bptree( store );
	
}
*/

internal void test_dupes() {

	database* db = database_create( "dupes" );

	board* current = new_board();

	int drops[] = {3, 4, 1, 2, 5, 1, 4, 6, 3, 4};
	bool was_insert;
	for(int i=0; i<10; i++) {
		board* next = drop( current, drops[i] );
		render( next, "Board", false );
		was_insert = database_put( db, next );
		assert( was_insert );
		free_board( next );

		next = drop( current, drops[i] );
		render( next, "Dupe board", false );
		was_insert = database_put( db, next );
		assert( !was_insert );
		
		free_board( current );
		current = next;
		
		bpt_print( db->index, 0 );
	}
	
	free_board( current );
	
	database_close( db );
	
}


internal void test_store_cmdline_seq( char* seq ) {
	
	database* db = database_create( "test" );
	
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

		database_put( db, next );

		free_board( current );
		current = next;

		printf("<<<<< After insert\n" );
		bpt_print( db->index, 0 );
		
		element = strtok( NULL, "," );
	}
	
	free_board( current );
	
	database_close( db );
}

int main(int argc, char** argv) {

	// TODO(stupid): if you don't call this you get segfaults. maybe board.o can call this itself somehow?
	map_squares_to_winlines();

	printf("ORDER %d, SPLIT_KEY_INDEX %d\n", ORDER, SPLIT_KEY_INDEX );
	
	// run cmd line stuff OR all tests
	if( argc == 2 ){
		test_store_cmdline_seq( argv[1] );
		bpt_dump_cf();
		exit( EXIT_SUCCESS );
	}
	
	test_dupes();
	
	/*
	// test storing 10 ints and finding them
	test_store_10();

	// test we don't find items that aren't there
	test_search_nonexisting_items();

	// insert a bunch of random numbers and find them
	test_store_random();

	// insert 0..100 in random order
	test_store_randomly();
*/
	bpt_dump_cf();

	printf("Done\n");
}
