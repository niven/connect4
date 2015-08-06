#ifndef __B_PLUS_TREE__
#define __B_PLUS_TREE__

#include "base.h"
#include "board.h"

#include <stdint.h>
#include <sys/types.h>

// TODO(research): Find out if it is possible to have ORDER=2 behave like a bintree
// TODO(research): find some optimal ORDER (pref a power of 2, and within a pagesize or something)
// sysconf(_SC_PAGE_SIZE)
#define ORDER 4
#define SPLIT_KEY_INDEX ((ORDER-1)/2)
#define SPLIT_NODE_INDEX (ORDER - ORDER/2)

struct bpt_counters {
	uint64_t node_creates;
	uint64_t node_loads;
	uint64_t node_frees;
	uint64_t node_writes;
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

typedef struct cache_stats {
	double hit_ratio;
	uint64_t hits;
	uint64_t misses;
	uint64_t clean_evicts;
	uint64_t dirty_evicts;
	uint64_t entry_allocs;
	uint64_t entry_frees;
	uint64_t free_entry_allocs;
	uint64_t free_entry_frees;
	size_t dirty_node_count;
	size_t clean_node_count;
} cache_stats;

// nodes have pointers to other nodes, or to table rows
// table rows are just row numbers (and we know the size, thus the offset in the table file)
// both are the same, but this makes the code much more clear
// TODO(memory): I think these could be uint32 if we are not going to store any board data in another table
// TODO: rename to value or something
typedef union pointer {
	uint32 child_node_id;
	uint32 board_data;
} pointer;

typedef struct record {
	board63 key;
	pointer value;
} record;

// node_id 0 is reserved to indicate NULL/empty/no parent
// TODO: maybe we can put the is_leaf first so we can check for leaf nodes easily in the file (well, wecould use an offset., but still)
typedef struct node {
	
	uint32 id;
	
	uint32 parent_node_id;

	uint32 num_keys; // number of entries

	// these are both 1 bigger than they can max be, but that makes
	// splitting easier (just insert, then split)
	board63 keys[ORDER];
	pointer pointers[ORDER+1]; // points to a value, or to a node

	// TODO(storage): these can be combined
	bool is_leaf;
	bool is_dirty; // modified since being read from disk
} node;

/*********************** Node cache types ************************/

/*
Node cache.

Having a cache makes sure we hit the disk as little as possible.

Constraints:
- The items in the cache must be refcounted (since an item could be retrieved more than once, and is used)
- Nothing with a refcount > 0 can ever be freed
- Keep as many nodes as possible in the cache (ie, keep it full at all times)
- retrieve must be O(1)
- release must be O(1)

Overview:
In order to have O(1) access the nodes are stored in a hash. Every bucket uses separate chaining.
Adding an item to the cache can happen when it is not full yet or when there is at least 1 "free entry".
A free entry means a node that has refcount == 0. The retrieve works by removing the free entry from the cache
and then inserting normally. This is in fact the normal situation: the cache is completely full but has some
free entries that can be recycled.

Finding a free entry must then also be O(1), which is why there is a separate free list (just a doubly linked list of free_entry's).

The cache entry holds a pointer to either a node or a free_entry.

Operations:
(1) Insert, space in the cache: 
	- hash the key
	- create an entry
	- set the node ptr of the entry
(2) Insert, no space in the cache, free entry present
	- take the first free entry
	- hash the key
	- find the entry in the bucket
	- remove the entry from the bucket
	- free the entry
	- free the node pointed to by the free entry
	- free the free_entry
	- decrement num_stored
	- go to (1)
(3) Insert, no space in cache, no free entry:
	- don't insert
(4) Release, item not in the cache
	- free the item
(5) Release, item in the cache with refcount > 1
	- hash the key
	- find the entry in the bucket
	- decrement its refcount
(6) Release, item in the cache with refcount == 1
	- hash the key etc.
	- set its refcount to 0
	- create a new free_entry
	- set the free_entry to point to the node the entry points to
	- set the entry ptr to point to the free_entry
	- insert the free_enty in the free_list
(7) Retrieve, item in the cache, refcount > 0
	- increment its refcount
(8) Retrieve, item in the cache, refcount == 0
	- find the entry
	- find the free_entry it points to
	- set the entry to point to the node pointed to by the free_entry
	- set its refcount to 1
	- free the free_entry
(9) Retrieve, item not in the cache
	- load it from disk
	- Insert

Committing nodes to disk:
When a node is created or retrieved from disk we put it in the cache, when it has to be evicted we write it to disk.
Whenever the cache is full and nothing can be evicted, on release it has to written to disk regardless.

TODO(performance): keep the free items as dirty/clean and recycle the clean ones to avoid writes, and probably also in Most Frequently Used order maybe,
							or maybe even use the is_dirty flag as a counter (mabe things that are very dirty are hot? Dunno)
TODO(profiling): distribution of hits/misses/dirty per node

*/

// buckets in the hash that stores the entries and max number of entries in the cache
#define CACHE_SIZE ((size_t) 8 )

// doubly linked list of refcount==0 entries in cache
typedef struct free_entry {
	node* evictable_node;
	uint32 node_id;
	struct free_entry* next;
	struct free_entry* prev;
} free_entry;

// bucket entries
typedef struct entry {
	union {
		node* to_node;
		free_entry* to_free_entry;
	} ptr;
	
	struct entry* next;
	// TODO(space): maybe node id could be 32 bits? refcount certainly is and that would save 8 bytes
	// maybe even some kind of size_t node_id:54, size_t refcount:10 kind of thing
	uint32 node_id;
	size_t refcount;
} entry;

// Cache with buckets, number stored and free list (entries with refcount 0)
typedef struct cache {
	entry* buckets[CACHE_SIZE];
	size_t num_stored;
	free_entry* free_list;
} cache;

/*************************** Database ********************************/

// TODO(convenience): maybe just put these directly in struct database
typedef struct database_header {
	uint32 root_node_id;

	uint32 node_count;
	uint64 table_row_count;
} database_header;

#define DATABASE_FILENAME_SIZE 256
typedef struct database {

	const char* name;

	char index_filename[DATABASE_FILENAME_SIZE];
	FILE* index_file;
#if 0
	char table_filename[DATABASE_FILENAME_SIZE];
	FILE* table_file;
#endif
	database_header* header;

	// TODO(performance): find the optimal size for the cache
	cache* node_cache;
	cache_stats cstats;
} database;

typedef struct database_cursor {
	
	size_t current;
	size_t num_records;
	
	// internal
	uint64* data; // mmapped
	node* current_node; // offset into data
	uint32 current_node_id;
	uint32 current_in_node;
	uint32 node_count;
	off_t index_file_size; // how much data
	database* db; // which db
} database_cursor;

// public API (always takes a root)
database* database_create( const char* name );
database* database_open( const char* name );
void database_close( database* db );

bool database_store( database* db, board* b );
bool database_put( database* db, board63 key );
bool database_get( database* db, board63 key );
size_t database_size( database* db );

void print_database_stats( database* db );
cache_stats get_database_cache_stats( database* db );

void database_init_cursor( database* db, database_cursor* cursor );
void database_dispose_cursor( database_cursor* cursor );

board63 database_get_record( database* db, database_cursor* cursor );

// internal stuff (operates on nodes)
node* new_node( database* db );

node* load_node_from_file( database* db, uint32 node_id );
bool load_row_from_file( board63 b63, FILE* in, off_t offset, board* b );

bool bpt_put( node** root, record r );
record* bpt_get( database* db, node* n, board63 key );
size_t bpt_size( database* db, node* n );

void print_index( database* db );
void print_index_from( database* db, uint32 start_node_id );

void release_node( database* db, node* n );

off_t file_offset_from_node( size_t id );
off_t file_offset_from_row_index( size_t row_index );

#endif
