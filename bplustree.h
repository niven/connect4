#ifndef __B_PLUS_TREE__
#define __B_PLUS_TREE__

#include "base.h"

#define ORDER 4
#define SPLIT_KEY_INDEX ((ORDER-1)/2)
#define SPLIT_NODE_INDEX (ORDER - ORDER/2)
typedef unsigned long key_t;

#define KEY_SIZE (sizeof(key_t))

static const char data_file[] = "boards.c4_data";

struct bpt_counters {
	uint64_t creates;
	uint64_t frees;
	uint64_t key_inserts;
	uint64_t get_calls;
	uint64_t insert_calls;
	uint64_t parent_inserts;
	uint64_t splits;
	uint64_t key_compares;
	uint64_t leaf_key_compares;
	uint64_t node_key_compares;
	uint64_t any;
};

// nodes have pointers to other nodes, or to table rows
// table rows are just row numbers (and we know the size, thus the offset in the table file)
typedef union pointer {
	struct node* node_ptr;
	size_t table_row_index;
} pointer;

typedef struct record {
	key_t key;
	pointer value;
} record;

typedef struct node {
	
	size_t id;
	
	struct node* parent;

	size_t num_keys; // number of entries

	// these are both 1 bigger than they can max be, but that makes
	// splitting easier (just insert, then split)
	key_t keys[ORDER];
	pointer pointers[ORDER+1]; // points to a value, or to a node

	bool is_leaf;

} node;

typedef node bpt;

// TODO: rename
bpt* new_bptree( size_t node_id );
void free_bptree( bpt* b );
void bpt_dump_cf( void );

typedef struct database {

	char* index_filename;
	FILE* index_file;

	char* table_filename;
	FILE* table_file;

	bpt* index;

	size_t node_count;
	size_t table_row_count;

} database;

database* database_create( const char* filename );
database* database_open( const char* filename );
void database_close( database* db );

void database_put( database* db, board* b );
board* database_get( database* db, key_t key );
size_t database_size( database* db );

node* load_node_from_file( FILE* index_file, size_t node_id );


// public API (always takes a root)
bool bpt_put( bpt** root, record r );
record* bpt_get(  database* db, bpt* root, key_t key );
size_t bpt_size( bpt* node );

void bpt_print( bpt* root, int indent );

// internal stuff (operates on nodes)
// TODO: declare in .c file
void bpt_insert_node( database* db, bpt* node, key_t up_key, bpt* sibling );
void bpt_split( database* db, bpt* node );
bool bpt_insert_or_update( database* db, bpt* node, record r );

#endif
