internal void put_node_in_cache( database* db, node* n );
internal node* retrieve_node( database* db, size_t node_id );
internal node* get_node_from_cache( database* db, size_t node_id );
internal void free_node( node* n );
internal void dump_cache( cache* db );
internal void clear_cache( cache* db );

// TODO(rename): maybe flush_cache or something
void clear_cache( cache* c ) {
	
	print("Clearing the cache (%lu entries)", c->num_stored);
	// free all items in the buckets
	for( size_t b=0; b<CACHE_BUCKETS; b++ ) {
		entry* current = c->buckets[b];
		while( current != NULL ) {
			// only free actual foos
			if( current->refcount != 0 ) {
				print("\tnode %lu", current->ptr.to_node->id );
				free_node( current->ptr.to_node );
			}
			// free the entry
			print("\tentry, node %lu", current->node_id );
			entry* next = current->next;
			free( current );
			counters.cache_entry_frees++;
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
		print("\tfree_entry, node %lu", current->node_id );
		free_node( current->evictable_node );
		free_entry* next = current->next;
		free( current );
		counters.cache_free_entry_frees++;
		current = next;
	}
	
	c->num_stored = 0;
	c->free_list = NULL;
}


void dump_cache( cache* c ) {
	
	print("Cache (%lu items):", c->num_stored);
	for(size_t i=0; i<CACHE_BUCKETS; i++ ) {
		entry* current = c->buckets[i];
		print("bucket %lu %p", i, current);
		while( current != NULL ) {
			print("\tentry key=%lu (node %lu) refcount: %lu", current->node_id, current->ptr.to_node->id, current->refcount );
			current = current->next;
		}
	}

	prints("Free list:");
	free_entry* current = c->free_list;
	if( current != NULL ){
		do {
			print("\tfree entry: key=%lu (node %lu) [next=%lu, prev=%lu]",
			current->node_id, current->evictable_node->id, current->next->node_id, current->prev->node_id );
			current = current->next;	
		} while( current != c->free_list );
	}
	
}

void free_node( node* n ) {

	counters.node_frees++;
	print("node %lu (%p) (cr: %llu/ld: %llu/fr: %llu)", n->id, n, counters.node_creates, counters.node_loads, counters.node_frees );

	assert( counters.node_frees <= counters.node_loads + counters.node_creates );

	n->id = 0; // helps finding bugs if someone is still using this
	free( n );
	
}


void release_node( database* db, node* n ) {

	assert( n != NULL );
	assert( n->id != 0 );
	
	cache* c = db->node_cache;
	size_t node_id = n->id;

	size_t b = hash( node_id ) % CACHE_BUCKETS;
	for( entry* i = c->buckets[b]; i != NULL; i = i->next ) {
		printf("release node %lu, looking in bucket %lu\n", node_id, b );
		if( i->node_id == node_id ) {
			assert( i->refcount > 0 );
			// TODO(safety): maybe also assert the node* is correct?
			i->refcount--;
			// add it to the free list if refcount hits 0
			if( i->refcount == 0 ) {
				free_entry* new_head = (free_entry*) malloc( sizeof(free_entry) );
				counters.cache_free_entry_allocs++;
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
			return;
		}
	}
	
	// was not in the cache, just free it
	print("Node %lu was not in the cache, doing a normal free()\n", node_id);
	free_node( n );
}


void put_node_in_cache( database* db, node* n ) {

	assert( n != NULL );

	cache* c = db->node_cache;
	print("Putting node %lu (%p) in the cache (load %lu/%lu)", n->id, n, c->num_stored, CACHE_MAX);

	if( c->num_stored == CACHE_MAX ) {
		prints("Cache full");
		// check the free list
		if( c->free_list != NULL ) {
			// take the first thing in the free list
			free_entry* fe = c->free_list;
			// remove it from the free list
			if( c->free_list->next == c->free_list ) { // just single item
				c->free_list = NULL;
			} else {
				// [prev]<->[fe]<->[next]
				//           ^
				//           c->free_list
				fe->prev->next = fe->next;
				fe->next->prev = fe->prev;
				c->free_list = fe->next;
			}
			// now our free list is ok again
			print("Can evict node %lu from free list (it's in bucket %lu)\n", fe->node_id, hash(fe->node_id) % CACHE_BUCKETS );
			entry* current;
			size_t b = hash(fe->node_id) % CACHE_BUCKETS;
			for(current = c->buckets[b]; current->next->node_id != fe->node_id; current = current->next ) {
				print("Checking node %lu (next node %lu)\n", current->node_id, current->next->node_id );
				if( current->node_id == fe->node_id ) {
					break;
				}
			}
			printf("Found parent of the free_entry: node %lu (next node %lu)\n", current->node_id, current->next->node_id );
			// free that one
			entry* to_be_freed = current->next;
			current->next = to_be_freed->next;
			
			// free the free_entry, the foo it points to and the entry in the bucket
			free_node( fe->evictable_node );
			free( fe );
			counters.cache_free_entry_frees++;
			free( to_be_freed );
			counters.cache_entry_frees++;
			
			counters.cache_evicts++;
			c->num_stored--; // we'll increment downbelow again
			// fall out and carry on with inserting 
		} else {
			prints("Nothing in the free list.");
			return;
		}

	}
	size_t b = hash( n->id ) % CACHE_BUCKETS;
	entry* bucket = c->buckets[b];
	
	entry* i = (entry*) malloc( sizeof(entry) );
	counters.cache_entry_allocs++;
	i->ptr.to_node = n;
	i->refcount = 1;
	i->node_id = n->id;
	i->next = NULL;
	
	// put it at the front of the chain
	if( bucket != NULL ) {
		i->next = bucket;
	}
	c->buckets[b] = i;

	c->num_stored++;
}

node* retrieve_node( database* db, size_t node_id ) {
	
	node* out = get_node_from_cache( db, node_id );
	if( out == NULL ) {
		counters.cache_misses++;
		out = load_node_from_file( db, node_id );
		put_node_in_cache( db, out );
	} else {
		counters.cache_hits++;
	}

	// dump_cache( db );
	
	assert( out->id != 0 );
	
	return out;
}

node* get_node_from_cache( database* db, size_t node_id ) {
	
	cache* c = db->node_cache;
	
	size_t b = hash( node_id ) % CACHE_BUCKETS;
	for( entry* i = c->buckets[b]; i != NULL; i = i->next ) {
		print("Get item %lu check bucket[%lu] = %lu\n", node_id, b, i->node_id);
		if( i->node_id == node_id ) {
			
			// either a foo, or a pointer to a free_entry
			if( i->refcount == 0 ) {
				// it's one on the free list, means we need to remove it from there
				free_entry* discard = i->ptr.to_free_entry;
				assert( discard != NULL );
				i->ptr.to_node = discard->evictable_node; // put it back in the regular entry

				// remove it from the free list
				discard->prev->next = discard->next;
				discard->next->prev = discard->prev;
				
				// if we happen to free the initial entry in the free list, set a new head
				if( c->free_list == discard ) {
					c->free_list = discard->next;
				}
				free( discard );
				counters.cache_free_entry_frees++;
				discard = NULL;
			}
			// regular item, or free_entry in between was discarded
			i->refcount++;
			return i->ptr.to_node;
		}
	}
	
	print("node %lu was not in the cache", node_id);
	return NULL;
	
}


