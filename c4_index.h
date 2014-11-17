#ifndef __C4_INDEX_H__
#define __C4_INDEX_H__

#include <limits.h>

#define MAX_NUM UCHAR_MAX

typedef unsigned char integral_type;

#define INDEX_BLOCK_SIZE 8
#define INDEX_BLOCK_SORT_THRESHOLD 2

// these are our entries
typedef struct index_entry {
	integral_type key;
	unsigned long file_offset;
} index_entry;

int compare_index_entry( const void* a, const void* b );


typedef struct index_block {

	index_entry keys[INDEX_BLOCK_SIZE];
	integral_type min;
	integral_type max;
	unsigned int used;
	unsigned int num_unsorted;
} index_block;


// These tell us how many index_blocks we have and where they are
typedef struct block_header_entry {
	
	int index_block_id;
	integral_type min;
	integral_type max;
	
	struct block_header_entry* next_entry;
} block_header_entry;

typedef struct block_header {
	int num_entries;
	const char* filename;
	block_header_entry* head;
} block_header;


// TODO: also _open()
block_header* c4_index_new( const char* filename );
void c4_index_free( block_header* bh );

void c4_index_block_free( index_block* ib );
index_block* c4_read_index_block( const char* filename, unsigned long index_id );
void c4_write_index_block( const char* filename, index_block* ib, unsigned long index_id );

void c4_index_store( block_header* bh, integral_type key, unsigned long offset );

int c4_index_retrieve( block_header* bh, integral_type key, unsigned long* offset );

block_header_entry* c4_get_block_header_entry( block_header* bh, integral_type key );
#endif
