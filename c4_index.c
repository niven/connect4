#include <stdio.h>
#include <stdlib.h>

#include "c4_index.h"

int compare_index_entry( const void* a, const void* b ) {


//	printf("Compare %lu <=> %lu\n", (unsigned long)((index_entry*)a)->key, (unsigned long)((index_entry*)b)->key);
	if( ((index_entry*)a)->key == ((index_entry*)b)->key )
		return 0;

	if( ((index_entry*)a)->key < ((index_entry*)b)->key )
		return -1;

	return 1;
	
}


block_header* c4_index_new( const char* filename ) {
	printf("Creating new index %s\n", filename );
	
	block_header* bh = (block_header*)malloc( sizeof(block_header) );
	if( bh == NULL ) {
		perror("malloc()");
		exit( EXIT_FAILURE );
	}
	
	bh->filename = filename;
	
	// create initial entry
	block_header_entry* bhe = (block_header_entry*)malloc( sizeof(block_header_entry) );
	if( bhe == NULL ) {
		perror("malloc()");
		exit( EXIT_FAILURE );
	}
	bhe->next_entry = NULL;
	bhe->min = 0;
	bhe->max = MAX_NUM;
	bhe->index_block_id = 0;
	
	bh->head = bhe;
	
	// create first index_block and write
	index_block* ib = (index_block*)malloc( sizeof(index_block) );
	ib->min = 0;
	ib->max = MAX_NUM;
	ib->used = 0;
	ib->num_unsorted = 0;
	c4_write_index_block( filename, ib, 0 ); // write it at offset 0
	c4_index_block_free( ib );
	
	return bh;
}

void c4_index_free( block_header* bh ) {
	
	block_header_entry* bhe = bh->head;
	while( bhe != NULL ) {
		bh->head = bh->head->next_entry;
		free( bhe );
		bhe = bh->head;
	}
	
	free( bh );
	bh = NULL;
}

void c4_index_block_free( index_block* ib ) {
	
	for( int i=0; i<ib->used; i++ ) {
		//free( ib->keys[i] );
	}
	
	free( ib );
	
}

// big win is caching here, maybe param based #blocks in memory?

index_block* c4_read_index_block( const char* filename, unsigned long index_id ) {
	
	index_block* ib = (index_block*)malloc( sizeof(index_block) );
	printf("Allocated index block %ld bytes\n", sizeof(index_block));
	
	unsigned long offset = index_id * sizeof(index_block);
	FILE* in = fopen( filename, "rb" );
	fseek( in, offset, SEEK_SET );
	fread( ib, sizeof(index_block), 1, in );
	
	fclose( in );
	
	return ib;
}

void c4_write_index_block( const char* filename, index_block* ib, unsigned long index_id ) {
	
	unsigned long offset = index_id * sizeof(index_block);
	
	FILE* out = fopen( filename, "wb" );
	fseek( out, offset, SEEK_SET );
	fwrite( ib, sizeof(index_block), 1, out );
	fclose( out );
}

void print_index_block( index_block* ib ) {
	printf("+--------------------------------+\n");
	printf("Index block used %d, unsorted %d\n", ib->used, ib->num_unsorted );
	for( int i=0; i<ib->used; i++ ) {
		printf("key[%00d] = %lu\n", ib->keys[i].key, ib->keys[i].file_offset );
	}
	printf("+--------------------------------+\n");
}

block_header_entry* c4_get_block_header_entry( block_header* bh, integral_type key ) {
	
	// use the block header to find the index block
	block_header_entry* bhe = NULL;
	for( bhe = bh->head; bhe != NULL; bhe = bhe->next_entry ) {
		printf("Checking entry [%lu-%lu]\n", (unsigned long)bhe->min, (unsigned long)bhe->max );
		if( bhe->min <= key && bhe->max >= key ) {
			printf("Found the block header entry: index block is at offset %d\n", bhe->index_block_id );
			return bhe;
		}
	}
	
	return NULL;
}

void c4_index_store( block_header* bh, integral_type key, unsigned long offset ) {
	printf("store %lu - offset %lu\n", (unsigned long)key, offset);
	
	block_header_entry* bhe = c4_get_block_header_entry( bh, key );
	if( bhe == NULL ) {
		fprintf( stderr, "FAIL: can't find block_header_entry for key %lu\n", (unsigned long)key);
	}
	
	index_block* ib = c4_read_index_block( bh->filename, bhe->index_block_id );

	ib->keys[ib->used].key = key;
	ib->keys[ib->used].file_offset = offset;

	ib->used++;
	ib->num_unsorted++;
	
	// now maybe sort
	if( ib->num_unsorted >= INDEX_BLOCK_SORT_THRESHOLD ) {
		printf("Sorting index block (%d unsorted)\n", ib->num_unsorted );
		// sort everything, the (used-num_unsorted) ones are already sorted
		qsort( ib->keys, ib->used, sizeof(index_entry), compare_index_entry );
		ib->num_unsorted = 0;
	}
	// now maybe split

	
	print_index_block( ib );
	c4_write_index_block( bh->filename, ib, bhe->index_block_id );
	c4_index_block_free( ib );
}

int c4_index_retrieve( block_header* bh, integral_type key, unsigned long* offset ) {
	printf("find %lu\n", (unsigned long)key );
	
	
	block_header_entry* bhe = c4_get_block_header_entry( bh, key );
	if( bhe == NULL ) {
		fprintf( stderr, "FAIL: can't find block_header_entry for key %lu\n", (unsigned long)key);
	}
	
	index_block* ib = c4_read_index_block( bh->filename, bhe->index_block_id );
	
	int found = 0;
	for( int i=0; i<ib->used; i++ ) {
		printf("Checking key %lu == %lu ?\n", (unsigned long)ib->keys[i].key, (unsigned long)key );
		if( ib->keys[i].key == key ) {
			*offset = ib->keys[i].file_offset;
			found = 1;
			break;
		}
	}
	
	c4_index_block_free( ib );
	
	return found;
}
