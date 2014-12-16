internal void put_node_in_cache( database* db, node* n );
internal node* retrieve_node( database* db, size_t node_id );
internal node* get_node_from_cache( database* db, size_t node_id );
internal void free_node( node* n );
internal void dump_cache( database* db );
internal void clear_cache( database* db );

internal void push_free_node( node_cache_free_item** head, node_cache_item item );
internal node_cache_item pop_free_node( node_cache_free_item** head );

// TODO(performance): Searching a node in the cache is slow (maybe a hash table to lookup?)
// and moving a node_cache_item to the end is not useful if it's refcount is not 0 yet
// maybe a linked list would be better, the biggest bottleneck now seems to be memmove()
// Something that might be bad: if we have 1 slot left and use linear probing open addressing
// to find slots in the table we have a painful long search for those last few items
// OTOH, the cache should quickly be full so we just take 1 from the free_list
// OTGH, in that scenario we actually construct an array with 1 empty slot
// Maybe some more thought needed.

void push_free_node( node_cache_free_item** head, node_cache_item item ) {

	assert( item.refcount == 0 );
	assert( item.node_ptr != NULL );
	
	node_cache_free_item* first = (node_cache_free_item*) malloc( sizeof(node_cache_free_item) );
	first->item = item;
	first->next = *head;
	*head = first;

}

node_cache_item pop_free_node( node_cache_free_item** head ) {
	
	assert( *head != NULL );
	
	node_cache_item first = (*head)->item;

	node_cache_free_item* new_head = (*head)->next;
	free( *head );
	*head = new_head;

	return first;
}


// TODO(rename): maybe flush_cache or something
void clear_cache( database* db ) {
	size_t num_items_in_cache = ARRAY_COUNT(db->node_cache) - db->free_slots_in_node_cache;
	print("items: %lu", num_items_in_cache);
	for(size_t i=0; i < num_items_in_cache; i++) {
		print("reclaiming slot %lu", i);
		database_store_node( db, db->node_cache[i].node_ptr );
		free_node( db->node_cache[i].node_ptr );
		db->node_cache[i].node_ptr = NULL;
		db->node_cache[i].refcount = 0;
		db->free_slots_in_node_cache++;
	}

	assert( ARRAY_COUNT(db->node_cache) == db->free_slots_in_node_cache );
}

void dump_cache( database* db ) {
	
	for(size_t i=0; i< ARRAY_COUNT(db->node_cache) - db->free_slots_in_node_cache; i++) {
		print("Cache[%lu] = id:%lu rc:%lu %p", i, db->node_cache[i].node_ptr->id, db->node_cache[i].refcount, db->node_cache[i].node_ptr );
	}
	
}

void free_node( node* n ) {

	counters.frees++;
	print("node %lu (%p) (cr: %llu/ld: %llu/fr: %llu)", n->id, n, counters.creates, counters.loads, counters.frees );

	assert( counters.frees <= counters.loads + counters.creates );

	n->id = 0; // helps finding bugs if someone is still using this
	free( n );
	
}

/*

	Check if this node is in the cache.

	If it isn't: just free it

	If it is: decrement it's refcount (may end up at 0) and move it to the end

*/
void release_node( database* db, node* n ) {

	assert( n != NULL );
	assert( n->id != 0 );

	
	// - find the node in the cache
	// - decrement the refcount 
	// - move it to the end
	size_t num_items_in_cache = ARRAY_COUNT(db->node_cache) - db->free_slots_in_node_cache;
	print("looking for node %lu, items in cache: %lu", n->id, num_items_in_cache);
	for(size_t i=0; i<num_items_in_cache; i++) {
		print("Cache[%lu] = id:%lu rc:%lu %p", i, db->node_cache[i].node_ptr->id, db->node_cache[i].refcount, db->node_cache[i].node_ptr );

		if( db->node_cache[i].node_ptr == n ) {
			node_cache_item found = (node_cache_item){ .refcount = db->node_cache[i].refcount, .node_ptr = db->node_cache[i].node_ptr };
			print("Found - Cache[%lu] = id:%lu rc:%lu %p", i, found.node_ptr->id, found.refcount, found.node_ptr );
			assert( found.refcount != 0 );
			assert( found.node_ptr->id == n->id ); // TODO(improvement): maybe this is the thing to check on, both?
			print("found node %lu in cache, refcount: %lu (cache location %lu)", n->id, found.refcount, i );
			found.refcount--;
			
			// move this item to the end of the
			// eg. cache size 6 items, free slots 2: moving item 3 (index 2) to the end, meaning items 4 shifts left
			size_t index_to_move_from = i + 1; // ie. if we move item 3 (index 2) to the end, item 4 (index 3) needs to move 
			size_t index_to_move_to = i; // this is the one we're taking out
			// move all items from 1 after the one we take out, to how many are left over
			// which reduces to (all used slots) - (element we take out)
			size_t num_nodeptrs_to_move = num_items_in_cache - (i + 1); 
			print("Moving %lu node_cache_items from cache[%lu] to cache[%lu]", num_nodeptrs_to_move, index_to_move_from, index_to_move_to );
			if( num_nodeptrs_to_move > 0 ) { // could be already at the end
				memmove( &db->node_cache[index_to_move_to], &db->node_cache[index_to_move_from], sizeof(node_cache_item) * num_nodeptrs_to_move );
			}
			
			db->node_cache[ num_items_in_cache - 1 ] = found;
			print("new location of node %lu in cache: %lu", found.node_ptr->id, num_items_in_cache-1 );
			// dump_cache( db );
			return;
		}
	}

	// it's not in the cache
	print("node %lu was not in the cache", n->id);
	free_node( n );
	// dump_cache( db );
	
}

/*
	Put a node pointer in the cache if there is space
	If the cache if not full put it in the beginning and move the rest over
	If the cache is full:
		Check if the last item has a refcount of 0, meaning nobody holds that pointer and it can be freed
		Free that item (no point in setting refcount to 0 or nodeptr to NULL since we shift it off anyway)
		Put it in the beginning and move the rest over, or just return if it was full

*/
void put_node_in_cache( database* db, node* n ) {
	
	print("Putting node %lu (%p) in the cache (%lu slots free)", n->id, n, db->free_slots_in_node_cache);
	assert( db->free_slots_in_node_cache <= ARRAY_COUNT( db->node_cache ) );
	
	// if there is room left, just put it there
	if( db->free_slots_in_node_cache ) {
		print("space for %lu items left in cache", db->free_slots_in_node_cache );

		// put it at the beginning of the array so it will be found fast if we need it again
		// and shift the rest right

		// shift all occupied slots right by 1
		size_t num_nodeptrs_to_move = ARRAY_COUNT(db->node_cache) - db->free_slots_in_node_cache;
		if( num_nodeptrs_to_move > 0 ){
			print("Moving %lu node_cache_items", num_nodeptrs_to_move );
			memmove( &db->node_cache[1], &db->node_cache[0], sizeof(node_cache_item) * num_nodeptrs_to_move );
		}

		db->node_cache[0] = (node_cache_item){ .refcount = 1, .node_ptr = n };
		db->free_slots_in_node_cache--;
		return;
	}

	assert( db->free_slots_in_node_cache == 0 );
	
	// maybe we can free the last item in the cache
	node_cache_item last_node_in_cache = db->node_cache[ LAST_INDEX(db->node_cache) ];
	if( last_node_in_cache.refcount == 0 ) {
		assert( last_node_in_cache.node_ptr != NULL ); // there must be an item here
		print("last node in cache: %lu rc:%lu %p", last_node_in_cache.node_ptr->id, last_node_in_cache.refcount, last_node_in_cache.node_ptr );
		print("Evicting last node in cache: %lu", last_node_in_cache.node_ptr->id );
		counters.cache_evicts++;
		database_store_node( db, last_node_in_cache.node_ptr );
		free_node( last_node_in_cache.node_ptr ); 
		db->free_slots_in_node_cache++;
	} else {
		// no more space in cache
		prints("no more space in cache, couldn't free anything either");
		return;
	}
	
	assert( db->free_slots_in_node_cache == 1 ); // should just have one spot free now

	// shift the whole thing right by 1 node
	// ie. move all elements - 1 from index 0 to index 1
	size_t num_nodeptrs_to_move = ARRAY_COUNT(db->node_cache) - 1;
	print("Shifting %lu node_cache_items right", num_nodeptrs_to_move );
	memmove( &db->node_cache[1], &db->node_cache[0], sizeof(node_cache_item) * num_nodeptrs_to_move );
	
	// insert at the beginning
	db->node_cache[0] = (node_cache_item){ .refcount = 1, .node_ptr = n };
	db->free_slots_in_node_cache--;
	
}

node* retrieve_node( database* db, size_t node_id ) {
	
	node* out = get_node_from_cache( db, node_id );
	if( out == NULL ) {
		out = load_node_from_file( db, node_id );
		put_node_in_cache( db, out );
	}

	// dump_cache( db );
	
	assert( out->id != 0 );
	
	return out;
}

/*
	Get the node from the cache if it's in there.
	If it is, increment the refcount of that item by 1
	and puts it in the front of the cache
	TODO(performance): this is the slowest now
*/
node* get_node_from_cache( database* db, size_t node_id ) {

	print("Checking for node %lu in cache (%lu free slots)", node_id, db->free_slots_in_node_cache );
	size_t num_items_in_cache = ARRAY_COUNT( db->node_cache ) - db->free_slots_in_node_cache;
	for(size_t i=0; i<num_items_in_cache; i++) {
		// print("Cache[%lu] = id:%lu rc:%lu %p", i,
		// 		db->node_cache[i].node_ptr->id,
		// 		db->node_cache[i].refcount,
		// 		db->node_cache[i].node_ptr );

		if( db->node_cache[i].node_ptr->id == node_id ) {
			prints("Cache hit!");

			// increment refcount since something wants to hold on to this
			node_cache_item found = (struct node_cache_item){ .refcount = db->node_cache[i].refcount+1, .node_ptr = db->node_cache[i].node_ptr };
			counters.cache_hits++;
			
			// now move it to the front
			size_t num_nodeptrs_to_move = i; // eg. moving item 3 means we shift items 0,1,2 right by 1
			print("Moving %lu node_cache_items", num_nodeptrs_to_move );
			memmove( &db->node_cache[1], &db->node_cache[0], sizeof(node_cache_item) * num_nodeptrs_to_move );
			
			// insert at the beginning
			db->node_cache[0] = found;

			return found.node_ptr;
		}
	}
	print("node %lu was not in the cache", node_id);
	counters.cache_misses++;
	return NULL;
}
