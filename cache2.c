/*
POC thing for a cache with the following properties
- cache is explicitly memory limited
- key/values are id/blobs
- retrieval is O(1) (blah blah amortized hash buckets separate chaining)
- items loaded/stored on disk
- evicting items based on 2 level hierarchy (data and nodes) as well as clean/dirty
- evicting items O(1) (feel this is mostly true now? hard to say)
- does not use malloc/free (for performance reasons)
*/
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> /* memset */

#define u32 uint32_t
#define u8 uint8_t
#define bool u8
#define true 1
#define false 0

#define CACHE_MEM_LIMIT_BYTES (128 * 1)

#define internal static
#define global_variable static
#define local_persist static

// count things for stats
typedef struct counter {
	u32 mallocs;
	u32 frees;
	u32 stores;
	u32 loads;
	u32 evicts[4];
} counter;

global_variable counter c;

// nodes that contain data (leaf nodes) or "pointers" to more nodes (index nodes)
// they can be clean/dirty depending if they were changed.
// if dirty, they would need to be written to disk before freed.
#define NODE_FLAG_DIRTY ( 1 << 0 )
#define NODE_FLAG_INDEX ( 1 << 1 )
#define NODE_FLAG_USED  ( 1 << 2 )

typedef struct node {
	u32 id;
	u32 data[8];
	u8 flags;
} node;
#define CACHE_SIZE ( (size_t) CACHE_MEM_LIMIT_BYTES / sizeof(node) )

#define NUM_BUCKETS 2

#define NC_LIST_DATA_CLEAN 0
#define NC_LIST_DATA_DIRTY 1
#define NC_LIST_INDEX_CLEAN 2
#define NC_LIST_INDEX_DIRTY 3

// the cache contains nodes, using fixed amount of memory
// the idea is to do stuff with nodes, and keep as many of them as possible in memory since disk access is expensive
// we can't keep all of them in memory so we set an explicit limit
// when we need to evict nodes, we evict in order: clean data, clean index, dirty data, dirty index
typedef struct entry {
	node* node;
	struct entry* next;
	struct entry* prev;
	u8 refcount;
} entry;
typedef struct node_cache {
	char* mem_pool;

	// store the nodes in buckets determined by just (id % NUM_BUCKETS)
	entry* bucket[NUM_BUCKETS];

	// we don't want to malloc/free these either. These correspond to the node in the pool
	// so the 0th node is entries[0] etc etc
	entry bucket_entries[CACHE_SIZE];

	// we don't want to malloc/free these either. These correspond to the node in the pool
	// so the 0th node is entries[0] etc etc
	entry entries[CACHE_SIZE];

	u32 load; // number of nodes in the cache

	// we keep lists of each type of node as well as clean/dirty
	entry* list[4]; // data/clean, data/dirty, index/clean, index/dirty

} node_cache;

void node_cache_init( node_cache* nc );
void node_cache_free( node_cache* nc );
void node_cache_debug( node_cache* nc );

// use new_node()/release_node() or get_node()/release_node()
node* node_cache_new_node( node_cache* nc, u32 id );
node* node_cache_get_node( node_cache* nc, u32 id );
void node_cache_release_node( node_cache* nc, node* n, bool has_changes );

void node_set_value( u32 index, u32 data );

void node_store_on_disk( node* n );
void node_load_from_disk( node* n, u32 id );

/* ################ Internal utility functions ############### */

internal void node_debug( node* n ) {
	printf("\tNode ID:%03d F:%d %p\n", n->id, n->flags, (void*)n );
}
internal void entry_debug( entry* e ) {
	printf("\tentry %p (next[%02d]:%p prev[%02d]:%p node[%02d]:%p rc:%d)\n", (void*)e, e->next->node->id, (void*)e->next, e->prev->node->id, (void*)e->prev, e->node->id, (void*)e->node, e->refcount);
}

internal u32 index_from_node_pointer( node_cache* nc, node* n ) {
	u32 result = 0;
	assert( ((char*)n) >= nc->mem_pool );
	result = ( (char*)n - nc->mem_pool ) / sizeof(node);
	return result;
}

internal node* node_pointer_from_index( node_cache* nc, u32 index ) {
	node* result = NULL;
	assert( index < CACHE_SIZE );
	result = (node*) ( nc->mem_pool +  index * sizeof(node) );
	return result;
}

/* ################ Double linked list functions ############# */

entry* dl_list_insert( entry* head, entry* e );
entry* dl_list_remove( entry* head, entry* e );

internal void dl_list_debug( entry* head ) {

	printf("Debug list %p\n", (void*) head );

	if( head != NULL ) {
		entry* start = head;
		entry* current = head;
		do {
			entry_debug( current );
			node_debug( current->node );
			current = current->next;
		} while( current != start );

	}
}

entry* dl_list_insert( entry* head, entry* e ) {

	printf("inserting e:%p n[%02d]:\n", (void*)e, e->node->id);
	dl_list_debug( head );

	if( head == NULL ) {
		e->next = e->prev = e;
	} else {
		e->next = head;
		e->prev = head->prev;
		e->next->prev = e;
		e->prev->next = e;
	}

	return e; // new head
}

entry* dl_list_remove( entry* head, entry* e ) {
	// 3 cases
	// A: e is the only element
	// B: e is the head element
	// C: e is some other element

	if( head == e ) {
		if( e->next == e ) { // single element list (no need to check e->prev == e)
			head = NULL; // new head is NULL, nothing to do
		} else {
			head = e->next; // just pick the next one, I don't think it matters
		}
	}

	// take it out of the list. This doesn't do anything in case e is the only element
	e->prev->next = e->next;
	e->next->prev = e->prev;

	// clear pointers to find bugs earlier
	e->next = NULL;
	e->prev = NULL;

	return head;
}


/* ################ Create a cache ##########################  */

void node_cache_init( node_cache* nc ) {

	nc->mem_pool = malloc( CACHE_SIZE * sizeof(node) );
	printf("alloc %lu bytes at %p\n",CACHE_SIZE * sizeof(node), (void*) nc->mem_pool );
	c.mallocs++;

	nc->load = 0; // number of nodes in the cache

	for( u32 i=0; i<CACHE_SIZE; i++ ) {
		node* n = node_pointer_from_index( nc, i );
		n->flags = 0; // clear all flags (we need the USED flag to be cleared)
	}

	memset( nc->bucket_entries, 0, sizeof(nc->bucket_entries) );
	memset( nc->entries, 0, sizeof(nc->entries) );

	for( u32 i=0; i<NUM_BUCKETS; i++ ) {
		nc->bucket[i] = NULL;
	}

	nc->list[NC_LIST_DATA_CLEAN] = nc->list[NC_LIST_INDEX_CLEAN]= nc->list[NC_LIST_INDEX_DIRTY] = nc->list[NC_LIST_DATA_DIRTY] = NULL;
}

void node_cache_free( node_cache* nc ) {

	free( nc->mem_pool );
	nc->mem_pool = NULL;
	c.frees++;
}

void node_cache_debug( node_cache* nc ) {

	printf("Load %d\n", nc->load);
	for( u32 i=0; i<NUM_BUCKETS; i++ ) {
		dl_list_debug( nc->bucket[i] );
	}
}

/* ################### Get/New/Release nodes ##################### */

node* node_cache_evict_node( node_cache* nc );

internal u32 bucket_index_from_node_id( u32 id ) {
	u32 result = 0;
	result = id % NUM_BUCKETS;
	return result;
}

internal u32 bucket_index_from_node( node* n ) {
	u32 result = 0;
	result = bucket_index_from_node_id( n->id );
	return result;
}


// Remove a node from the cache in order: clean data, clean index, dirty data, dirty index
// Make sure we don't evict ones with refcount > 0 as they are in use
node* node_cache_evict_node( node_cache* nc ) {

	node* result = NULL;

	u32 evict_order[4] = { NC_LIST_DATA_CLEAN, NC_LIST_INDEX_CLEAN, NC_LIST_DATA_DIRTY, NC_LIST_INDEX_DIRTY };

	for( u32 i=0; i<4; i++ ) {
		entry* list = nc->list[ evict_order[i] ];
		printf("Trying to evict from list %d\n", i);
		if( list != NULL ) {
			dl_list_debug( list );
			entry* current = list;
			do {
				if( current->refcount == 0 ) {
					node* to_be_evicted = current->node;
					printf("\tEviction candidate found: \n");
					entry_debug( current );
					node_debug( to_be_evicted );

					node_store_on_disk( to_be_evicted );
					to_be_evicted->flags = 0; // reset all, though only would need ~NODE_FLAG_USED
					result = to_be_evicted; // we can immediately reuse this one if needed

					u32 bucket_entry_index = index_from_node_pointer( nc, to_be_evicted );
					entry* bucket_entry = &nc->bucket_entries[bucket_entry_index];
					u32 bucket_index = bucket_index_from_node( to_be_evicted );
					nc->bucket[bucket_index] = dl_list_remove( nc->bucket[bucket_index], bucket_entry );
					nc->list[ evict_order[i] ] = dl_list_remove( list, current );
					dl_list_debug( nc->list[ evict_order[i] ] );
					c.evicts[ evict_order[i] ]++;
					goto DONE;
				}
				current = current->next;
			} while( current != list );
		}
	}

	assert( result != NULL );
	if( result == NULL ) {
		printf("[FAIL]: Could not evict any nodes. Maybe all are in use? (refcount > 0)\n");
	}

DONE:
	return result;
}

internal node* node_cache_allocate_node( node_cache* nc ) {

	node* result = NULL;

	// check for space, common path is that it should be full
	if( nc->load == CACHE_SIZE ) {
		printf("**** Cache full\n");
		result = node_cache_evict_node( nc );
		printf("Evicted node %p\n", (void*) result);
	} else {
		// get mem from the pool, start at the beginning and check the flags on the nodes
		// Can we do better? possibly using a "not in use list" of freed nodes.
		// in practice it probably won't matter as the cache should always be full
		// if that is the case, a counter would help us to know that.
		for( u32 i=0; i<CACHE_SIZE; i++ ) {
			node* n = node_pointer_from_index( nc, i );
			printf("New node, checking for space at %p\n", (void*) n);
			if( (n->flags & NODE_FLAG_USED) == 0) {
				result = n;
				nc->load++;
				break;
			}
		}
	}

	printf("Allocated node %p\n", (void*)result );
	result->flags = NODE_FLAG_USED;

	return result;
}

internal void 	node_cache_add_to_bucket( node_cache* nc, node* n ) {

	u32 node_index = index_from_node_pointer( nc, n );
	entry* bucket_entry = &nc->bucket_entries[node_index];

	printf("Bucket entry %p\n", (void*)bucket_entry);

	bucket_entry->node = n;
	u32 index = bucket_index_from_node( n );
	printf("Adding node %d to bucket %d\n", n->id, index);
	nc->bucket[index] = dl_list_insert( nc->bucket[index], bucket_entry );

}

internal entry* node_cache_add_to_list( node_cache* nc, node* n, u8 list_index ) {

	u32 node_index = index_from_node_pointer( nc, n );
	entry* list_entry = &nc->entries[node_index];

	// wrap it in an entry in the appropriate list (index/key, clean/dirty)
	printf("List entry %p\n", (void*)list_entry);
	list_entry->node = n;
	nc->list[list_index] = dl_list_insert( nc->list[list_index], list_entry );
	dl_list_debug( nc->list[list_index] );

	return list_entry;
}

// create a new node and store it in the cache.
// If the cache is full, evict an item
node* node_cache_new_node( node_cache* nc, u32 id ) {

	node* result = NULL;

	result = node_cache_allocate_node( nc );

	result->id = id;
	result->flags |= NODE_FLAG_DIRTY;

	entry* list_entry = node_cache_add_to_list( nc, result,NC_LIST_DATA_DIRTY );
	list_entry->refcount = 1;

	node_cache_add_to_bucket( nc, result );

	return result;
}

// Done with this node. Decrements its refcount, maybe move it to another list
void node_cache_release_node( node_cache* nc, node* n, bool has_changes ) {

	// get the entry for this node
	u32 node_index = index_from_node_pointer( nc, n );
	printf("Node index %d\n", node_index);
	entry* entry = &nc->entries[node_index];

	printf("Releasing node[%03d]\n", n->id);
	printf("Associated entry: %d RC:%d\n", node_index, entry->refcount);

	if( has_changes ) {
		if( n->flags & NODE_FLAG_DIRTY ) {
			// dirty node with more dirt
		} else {
			// move it to the dirty list, remove it from the clean list
			if( n->flags & NODE_FLAG_INDEX ) {
				nc->list[NC_LIST_INDEX_CLEAN] = dl_list_remove( nc->list[NC_LIST_INDEX_CLEAN], entry );
			} else {
				nc->list[NC_LIST_DATA_CLEAN] = dl_list_remove( nc->list[NC_LIST_DATA_CLEAN], entry );
			}
		}
	}

	entry->refcount--;
	assert( entry->refcount != UINT8_MAX ); // underflow

}

// get a node from disk or cache. if from disk, store in the cache
node* node_cache_get_node( node_cache* nc, u32 id ) {

	u32 bucket_index = bucket_index_from_node_id( id );
	entry* current = nc->bucket[bucket_index];
	if( current != NULL ) {
		entry* head = nc->bucket[bucket_index];
		do {
			if( current->node->id == id ) {
				current->refcount++;
				return current->node;
			}
		} while( current != head );
	}

	// not found in cache: get a new one and read it's data from disk

	node* to_be_loaded = node_cache_allocate_node( nc );

	node_load_from_disk( to_be_loaded, id ); // write data to node, set the id

	entry* list_entry = node_cache_add_to_list( nc, to_be_loaded, NC_LIST_DATA_CLEAN );
	list_entry->refcount = 1;

	node_cache_add_to_bucket( nc, to_be_loaded );

	return to_be_loaded;
}

void node_store_on_disk( node* n ) {

	printf("Storing node %d on disk\n", n->id);
	node_debug( n );
	c.stores++;

}

void node_load_from_disk( node* n, u32 id ) {

	printf("Loading node %d from disk\n", id);
	node_debug( n );
	c.loads++;

}

int main() {

	printf("Node size: %lu, Cache size: %lu, Cache mem limit bytes: %d\n", sizeof(node), CACHE_SIZE, CACHE_MEM_LIMIT_BYTES);

	node_cache cache;
	for( int i=0; i<4; i++ ) {
		node_cache_init( &cache );
		node_cache_free( &cache );
	}
	printf("Mallocs: %d, Frees: %d\n", c.mallocs, c.frees );
	assert( c.mallocs = c.frees );

	node_cache_init( &cache );
	u32 node_counter = 0;
	for(unsigned long i=0; i<CACHE_SIZE+3; i++) {
		node* n = node_cache_new_node( &cache, node_counter++ );
		// assert( n != NULL );
		node_cache_release_node( &cache, n, false );
	}
	node_cache_debug( &cache );
	node_cache_free( &cache );

	printf("Mallocs: %d, Frees: %d, Stores: %d\n", c.mallocs, c.frees, c.stores );
	for(u8 i=0; i<4; i++) {
		printf("Evicts from list %d = %u\n", i, c.evicts[i] );
	}
	assert( c.mallocs = c.frees );

	printf("Done.\n");
}
