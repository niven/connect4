#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "base.h"
#include "bplustree.h"

internal void test_header( const char* title ) {
	
	char row[] = "#####################################################";
	row[ strlen(title)+4 ] = '\0';
	printf("\n%s\n# %s #\n%s\n", row, title, row);
	
}

internal void test_store_10() {
	
	test_header( "Store 10 records and count" );
	size_t max = 10;
	bpt* store = new_bptree();
	printf("Test store: %d\n", 10);
	for( size_t i=0; i<max; i++ ) {
		key_t k = (key_t)i;
		store = bpt_insert_or_update( store, (struct record){ .key = k, { .value_int = (int)i } } );
		printf("### Result after storing %d records\n", i+1);
		print_bpt( store, 0 );
		record* r = bpt_find( store, (key_t)i );
		assert( r != NULL );
		assert( r->key == k );
		assert( r->value.value_int == (int)i );
		assert( bpt_count_records( store ) == (i+1) );
		printf("### Test OK - store %d records\n", i+1);
	}

	// repeat the finding to ensure we didn't remove or lose stuff
	for( size_t i=0; i<max; i++ ) {
		key_t k = (key_t)i;
		record* r = bpt_find( store, k );
		assert( r != NULL );
		assert( r->key == k );
		assert( r->value.value_int == (int)i );
	}
	free_bptree( store );
	
}

internal void test_overwrite_dupes() {
	bpt* store = new_bptree();
	key_t dupes[6] = { 1, 2, 3, 4, 2, 2 };
	for( int i=0; i<6; i++ ) {		
		store = bpt_insert_or_update( store, (struct record){ .key = dupes[i], { .value_int = i} } );
//		print_bpt( store, 0 );
		record* r = bpt_find( store, dupes[i] );
		assert( r != NULL );
		assert( r->key == dupes[i] );
		assert( r->value.value_int == i );
	}
	free_bptree( store );
	
}

internal void test_store_random() {
	bpt* store = new_bptree();
	int num_rands = 20;
	for( int i=0; i<num_rands; i++ ) {
		key_t num = rand()%1024;
		store = bpt_insert_or_update( store, (struct record){ .key = num, { .value_int = i} } );
		record* r = bpt_find( store, num );
		assert( r != NULL );
		assert( r->key == num );
	}
	printf("\nRand filled tree:\n");
	print_bpt( store, 0 );
	free_bptree( store );
	
}

internal void test_store_cmdline_seq( char* seq ) {
	
	bpt* root = new_bptree();
	
	printf("Sequence: %s\n", seq);
	char* element = strtok( seq, "," );
	int i = 0;
	while( element != NULL ) {
		printf("\n>>>>> Insert %s\n", element );
		root = bpt_insert_or_update( root, (struct record){ .key = (key_t)atoi(element), { .value_int = i++} } );
		printf("<<<<< After insert %s %p\n", element, root );
		print_bpt( root, 0 );
		element = strtok( NULL, "," );
	}
	
	free_bptree( root );
	
}

int main(int argc, char** argv) {

	printf("ORDER %d, SPLIT_KEY_INDEX %d\n", ORDER, SPLIT_KEY_INDEX );

	// run cmd line stuff OR all tests
	if( argc == 2 ){
		test_store_cmdline_seq( argv[1] );
		exit( EXIT_SUCCESS );
	}

	// test storing 10 ints and finding them
	test_store_10();

	// insert duplicates (should overwrite
	test_overwrite_dupes();
	
	// insert a bunch of random numbers and find them
	test_store_random();

	printf("Done\n");
}
