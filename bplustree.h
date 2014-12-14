#ifndef __B_PLUS_TREE__
#define __B_PLUS_TREE__

#include "base.h"
#include "c4types.h"

// TODO(research): Find out if it is possible to have ORDER=2 behave like a bintree
// TODO(research): find some optimal ORDER (pref a power of 2, and within a pagesize or something)
#define ORDER 240
#define SPLIT_KEY_INDEX ((ORDER-1)/2)
#define SPLIT_NODE_INDEX (ORDER - ORDER/2)


struct bpt_counters {
	uint64_t cache_hits;
	uint64_t cache_misses;
	uint64_t cache_evicts;
	uint64_t creates;
	uint64_t loads;
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
// both are the same, but this makes the code much more clear
typedef union pointer {
	size_t child_node_id;
	size_t table_row_index;
} pointer;

typedef struct record {
	key_t key;
	pointer value;
} record;

// node_id 0 is reserved to indicate NULL/empty/no parent
typedef struct node {
	
	size_t id;
	
	size_t parent_node_id;

	size_t num_keys; // number of entries

	// these are both 1 bigger than they can max be, but that makes
	// splitting easier (just insert, then split)
	key_t keys[ORDER];
	pointer pointers[ORDER+1]; // points to a value, or to a node

	bool is_leaf;

} node;

typedef struct node_cache_item {
	size_t refcount;
	node* node_ptr;
} node_cache_item;

// TODO(convenience): maybe just put these directly in struct database
typedef struct database_header {
	size_t node_count;
	size_t table_row_count;
	size_t root_node_id;
} database_header;

#define DATABASE_FILENAME_SIZE 256
typedef struct database {

	const char* name;

	char index_filename[DATABASE_FILENAME_SIZE];
	FILE* index_file;

	char table_filename[DATABASE_FILENAME_SIZE];
	FILE* table_file;

	database_header* header;

	// TODO(performance): find the optimal size for the cache
	node_cache_item node_cache[1024];
	size_t free_slots_in_node_cache;

} database;

// public API (always takes a root)
database* database_create( const char* name );
database* database_open( const char* name );
void database_close( database* db );

bool database_put( database* db, board* b );
board* database_get( database* db, key_t key );
size_t database_size( database* db );

// internal stuff (operates on nodes)
node* new_node( size_t node_id );
void bpt_dump_cf( void );

node* load_node_from_file( database* db, size_t node_id );
board* load_row_from_file( FILE* in, off_t offset );

bool bpt_put( node** root, record r );
record* bpt_get( database* db, node* n, key_t key );
size_t bpt_size( database* db, node* n );

void print_index( database* db );
void print_index_from( database* db, size_t start_node_id );

void release_node( database* db, node* n );

#endif
