internal void put_node_in_cache( database* db, node* n );
internal node* retrieve_node( database* db, uint32 node_id );
internal node* get_node_from_cache( database* db, uint32 node_id );
internal void free_node( database* db, node* n );
internal void clear_cache( database* db, cache* c );

// TODO(rename): maybe flush_cache or something
void clear_cache( database* db, cache* c ) {

	print("Clearing the cache (%lu entries)", c->num_stored);
	// free all items in the buckets
	for( uint32 b=0; b<CACHE_SIZE; b++ ) {
		entry* current = c->buckets[b];
		while( current != NULL ) {
			// only free actual foos
			if( current->refcount != 0 ) {
				print("   node %u", current->ptr.to_node->id );
				free_node( db, current->ptr.to_node );
			}
			// free the entry
			print("   entry, node %u", current->node_id );
			entry* next = current->next;
			free( current );
			db->cstats.entry_frees++;
			current = next;
		}
		c->buckets[b] = NULL;
	}

	// now all foos in entries are freed, as well as all entries
	// free the free_entry and foos they contain
	free_entry* current = c->free_list;
	// break the chain (sounds more exciting than it is)
	if( current != NULL ) {
		current->prev->next = NULL;
	}
	while( current != NULL ) {
		print("   free_entry, node %u", current->node_id );
		free_node( db, current->evictable_node );
		free_entry* next = current->next;
		free( current );
		db->cstats.free_entry_frees++;
		current = next;
	}

	c->num_stored = 0;
	c->free_list = NULL;
}


#ifdef VERBOSE
void dump_cache( cache* c );
void dump_cache( cache* c ) {

	print("Cache (%lu items):", c->num_stored);
	for(size_t i=0; i<CACHE_SIZE; i++ ) {
		entry* current = c->buckets[i];
		print("bucket %lu %p", i, current);
		while( current != NULL ) {
			print("   entry key=%u (node %u) refcount: %lu", current->node_id, current->refcount == 0 ? current->ptr.to_free_entry->node_id : current->ptr.to_node->id, current->refcount );
			current = current->next;
		}
	}

	print("Free list:%s", c->free_list == NULL ? " (empty)" : "" );
	free_entry* current = c->free_list;
	if( current != NULL ){
		do {
			print("   free entry: node %u [next=%u, prev=%u]", current->evictable_node->id, current->next->node_id, current->prev->node_id );
			current = current->next;
		} while( current != c->free_list );
	}
}
#else
#define dump_cache( c ) // nothing
#endif


void free_node( database* db, node* n ) {

	counters.node_frees++;
	print("node %u (%p) (dirty: %s) (cr: %llu/ld: %llu/fr: %llu)", n->id, n, n->is_dirty ? "true" : "false", counters.node_creates, counters.node_loads, counters.node_frees );

	assert( counters.node_frees <= counters.node_loads + counters.node_creates );

	database_store_node( db, n );

	n->id = 0; // helps finding bugs if someone is still using this
	free( n );

}


void release_node( database* db, node* n ) {

	assert( n != NULL );
	assert( n->id != 0 );

	cache* c = db->node_cache;
	uint32 node_id = n->id;
	print("node %u", node_id );

	size_t b = hash( node_id ) % CACHE_SIZE;
	for( entry* i = c->buckets[b]; i != NULL; i = i->next ) {
		print("checking bucket[%lu] = %u", b, i->node_id );
		if( i->node_id == node_id ) {
			assert( i->refcount > 0 );
			// TODO(safety): maybe also assert the node* is correct?
			i->refcount--;
			// add it to the free list if refcount hits 0
			if( i->refcount == 0 ) {
				prints("refcount 0, creating a free_entry");
				free_entry* new_head = (free_entry*) malloc( sizeof(free_entry) );
				db->cstats.free_entry_allocs++;
				new_head->evictable_node = i->ptr.to_node; // keep the actual thing we store
				i->ptr.to_free_entry = new_head; // replace it with ref to the free_entry
				new_head->node_id = node_id;
				if( c->free_list == NULL ) {
					prints("empty free_list, setting first item");
					new_head->next = new_head;
					new_head->prev = new_head;
				} else {
					new_head->next = c->free_list;
					new_head->prev = c->free_list->prev;
					c->free_list->prev = new_head;
					new_head->prev->next = new_head;
				}

				c->free_list = new_head;
			}
			// prints("cache after release");
			// dump_cache( c );
			return;
		}
	}

	// was not in the cache, just free it
	print("Node %u was not in the cache, doing a normal free()", node_id);
	free_node( db, n );
}

internal void evict_item( database* db, cache* c, free_entry** free_list ) {

	// TODO(performance): this is a very (if not the the most common) path when the cache is full, maybe something could be optimized

	// take the first thing in the free list
	free_entry* fe = *free_list;
	// remove it from the free list
	if( (*free_list)->next == *free_list ) { // just single item
		*free_list = NULL;
	} else {
		// [prev]<->[fe]<->[next]
		//           ^
		//           c->free_list
		fe->prev->next = fe->next;
		fe->next->prev = fe->prev;
		*free_list = fe->next;
	}
	// now our free list is ok again
	size_t b = hash( fe->node_id ) % CACHE_SIZE;
	print("Can evict node %u from free list (it's in bucket %lu)", fe->node_id, b );

	// find and remove the entry from the bucket
	entry* entry_to_free = NULL;
	if( c->buckets[b]->node_id == fe->node_id ) { // it's the head item
		prints("Removing the entry from the bucket (it was the head)");
		entry_to_free = c->buckets[b];
		c->buckets[b] = c->buckets[b]->next; // just move to the next one
	} else {
		entry* current;
		for(current = c->buckets[b]; current->next->node_id != fe->node_id; current = current->next ) {
			print("Checking node %u (next %u)", current->node_id, current->next->node_id );
			if( current->node_id == fe->node_id ) {
				break;
			}
			assert( current->next != NULL ); // it has to be in this list
		}
		print("Found the bucket entry: node %u (next node %u)", current->node_id, current->next->node_id );
		entry_to_free = current->next;
		current->next = current->next->next; // skip over it
	}
	assert( entry_to_free != NULL );

	// free the free_entry, the foo it points to and the entry in the bucket
	if( fe->evictable_node->is_dirty ) {
		db->cstats.dirty_evicts++;
		assert( db->cstats.dirty_node_count > 0 );
		db->cstats.dirty_node_count--;
	} else {
		db->cstats.clean_evicts++;
		assert( db->cstats.clean_node_count > 0 );
		db->cstats.clean_node_count--;
	}
	free_node( db, fe->evictable_node );
	free( fe );
	db->cstats.free_entry_frees++;

	free( entry_to_free );
	db->cstats.entry_frees++;

	c->num_stored--;
}

void put_node_in_cache( database* db, node* n ) {

	assert( n != NULL );

	cache* c = db->node_cache;
	print("Putting node %u in the cache (load %lu/%lu)", n->id, c->num_stored, CACHE_SIZE);

	if( c->num_stored == CACHE_SIZE ) {
		prints("Cache full");
		// check the free list
		if( c->free_list != NULL ) {
			evict_item( db, c, &c->free_list );
		} else {
			prints("Nothing in the free list.");
		}

	}
	size_t b = hash( n->id ) % CACHE_SIZE;
	entry* bucket = c->buckets[b];

	entry* i = (entry*) malloc( sizeof(entry) );
	db->cstats.entry_allocs++;
	i->ptr.to_node = n;
	i->refcount = 1;
	i->node_id = n->id;
	i->next = NULL;

	if( n->is_dirty ) {
		db->cstats.dirty_node_count++;
	} else {
		db->cstats.clean_node_count++;
	}

	// put it at the front of the chain
	// TODO(research): is that the righ place? What does this mean for perf?
	if( bucket != NULL ) {
		i->next = bucket;
	}
	c->buckets[b] = i;

	c->num_stored++;
}

node* retrieve_node( database* db, uint32 node_id ) {

	node* out = get_node_from_cache( db, node_id );
	if( out == NULL ) {
		db->cstats.misses++;
		out = load_node_from_file( db, node_id );
		put_node_in_cache( db, out );
	} else {
		db->cstats.hits++;
	}

	// dump_cache( db->node_cache );
	// TODO(debug): this assert failed ONCE after I changed all the node IDs to uint32
	print("Cache contents: %lu dirty / %lu clean (size %lu)", db->cstats.dirty_node_count, db->cstats.clean_node_count, CACHE_SIZE );
	assert( db->cstats.dirty_node_count + db->cstats.clean_node_count <= CACHE_SIZE );

	assert( out->id != 0 );

	return out;
}

node* get_node_from_cache( database* db, uint32 node_id ) {

	cache* c = db->node_cache;
	print("node id %u", node_id );
	size_t b = hash( node_id ) % CACHE_SIZE;
	for( entry* i = c->buckets[b]; i != NULL; i = i->next ) {
		print("checking bucket[%lu] = node %u", b, i->node_id);
		if( i->node_id == node_id ) {

			// either a foo, or a pointer to a free_entry
			if( i->refcount == 0 ) {
				print("Node %u is on the free list: unfreeing", i->node_id );
				// it's one on the free list, means we need to remove it from there
				free_entry* discard = i->ptr.to_free_entry;
				assert( discard != NULL );
				i->ptr.to_node = discard->evictable_node; // put it back in the regular entry

				// remove it from the free list
				discard->prev->next = discard->next;
				discard->next->prev = discard->prev;

				// if we happen to free the initial entry in the free list, set a new head
				// unless this was the last item, then set the list to NULL
				if( c->free_list == discard ) {
					c->free_list = discard->next == discard ? NULL : discard->next;
				}
				free( discard );
				db->cstats.free_entry_frees++;

				discard = NULL;
			}
			// regular item, or free_entry in between was discarded
			i->refcount++;
			return i->ptr.to_node;
		}
	}

	print("node %u was not in the cache", node_id);
	return NULL;

}


