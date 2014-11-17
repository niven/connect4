#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bplustree.h"

void test_store_10() {
	
	int max = 10;
	bpt* store = new_bptree();
	printf("Test store: %d\n", 10);
	for( int i=0; i<max; i++ ) {
		bpt_insert_or_update( &store, (struct record){ .key = i, { .value_int = i} } );
		record* r = bpt_find( store, i );
		assert( r != NULL );
		assert( r->key == i );
		assert( r->value.value_int == i );
		assert( bpt_count_records( store ) == (i+1) );
	}

	// repeat the finding to ensure we didn't remove or lose stuff
	for( int i=0; i<max; i++ ) {
		record* r = bpt_find( store, i );
		assert( r != NULL );
		assert( r->key == i );
		assert( r->value.value_int == i );
	}
	free_bptree( store );
	
}

void test_overwrite_dupes() {
	bpt* store = new_bptree();
	int dupes[6] = { 1, 2, 3, 4, 2, 2 };
	for( int i=0; i<6; i++ ) {		
		bpt_insert_or_update( &store, (struct record){ .key = dupes[i], { .value_int = i} } );
//		print_bpt( store, 0 );
		record* r = bpt_find( store, dupes[i] );
		assert( r != NULL );
		assert( r->key == dupes[i] );
		assert( r->value.value_int == i );
	}
	free_bptree( store );
	
}

void test_store_random() {
	bpt* store = new_bptree();
	int num_rands = 20;
	for( int i=0; i<num_rands; i++ ) {
		int num = rand()%1024;
		bpt_insert_or_update( &store, (struct record){ .key = num, { .value_int = i} } );
		record* r = bpt_find( store, num );
		assert( r != NULL );
		assert( r->key == num );
	}
	printf("\nRand filled tree:\n");
	print_bpt( store, 0 );
	free_bptree( store );
	
}

void test_store_cmdline_seq( char* seq ) {
	
	bpt* root = new_bptree();
	
	printf("Sequence: %s - SPLIT_KEY_INDEX %d\n", seq, SPLIT_KEY_INDEX);
	for( int i=0; i<strlen(seq); i++ ) {
		printf("\n##### insert %c ####\n", seq[i]);
		bpt_insert_or_update( &root, (struct record){ .key = seq[i] - '0', { .value_int = i} } );
		print_bpt( root, 0 );
	}
	
	free_bptree( root );
	
}
int main(int argc, char** argv) {

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
