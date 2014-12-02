#ifndef __B_PLUS_TREE__
#define __B_PLUS_TREE__

#include "base.h"

#define ORDER 16
#define SPLIT_KEY_INDEX ((ORDER-1)/2)
#define SPLIT_NODE_INDEX (ORDER - ORDER/2)
typedef unsigned long key_t;

#define KEY_SIZE (sizeof(key_t))

struct bpt_counters {
	uint64_t creates;
	uint64_t frees;
	uint64_t key_inserts;
	uint64_t insert_calls;
	uint64_t parent_inserts;
	uint64_t splits;
	uint64_t key_compares;
};


typedef union pointer {
	struct node* node_ptr;
	int value_int;
} pointer;

typedef struct record {
	key_t key;
	pointer value;
} record;

typedef struct node {
	
	struct node* parent;

	size_t num_keys; // number of entries

	// these are both 1 bigger than they can max be, but that makes
	// splitting easier (just insert, then split)
	key_t keys[ORDER];
	pointer pointers[ORDER+1]; // points to a value, or to a node

	bool is_leaf;

} node;

typedef node bpt;

bpt* new_bptree( void );
void free_bptree( bpt* b );
void bpt_dump_cf( void );

// public API (always takes a root)
void bpt_put( bpt** root, record r );
record* bpt_get( bpt* root, key_t key );
unsigned long bpt_size( bpt* node );

void bpt_print( bpt* root, int indent );

// internal stuff (operates on nodes)
void bpt_insert_node( bpt* node, key_t up_key, bpt* sibling );
void bpt_split( bpt* node );
void bpt_insert_or_update( bpt* node, record r );

#endif
