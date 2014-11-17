#ifndef __B_PLUS_TREE__
#define __B_PLUS_TREE__

#define true 1
#define false 0

#define ORDER 4
#define SPLIT_KEY_INDEX ((ORDER-1)/2)
#define RIGHT_SIZE (ORDER - ORDER/2)
#define LEFT_SIZE (ORDER/2)

typedef union pointer {
	struct node* node_ptr;
	int value_int;
} pointer;

typedef struct record {
	int key;
	pointer value;
} record;

typedef struct node {
	
	struct node* parent;
	
	int num_keys; // number of entries
	
	int keys[ORDER];
	pointer pointers[ORDER]; // points to a value, or to a node

	char is_leaf;

} node;

typedef node bpt;

bpt* new_bptree();
void free_bptree( bpt* b );

void bpt_insert_or_update( bpt** root, record r );
record* bpt_find( bpt* root, int key );

void print_bpt( bpt* root, int indent );

unsigned long bpt_count_records( bpt* root );

#endif
