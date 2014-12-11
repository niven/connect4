internal void put_node_in_cache( database* db, node* n );
internal node* retrieve_node( database* db, size_t node_id );
internal node* get_node_from_cache( database* db, size_t node_id );

/*
	Free a node.
	Check if this node is in the cache.

	If it isn't: just free it

	If it is: decrement it's refcount (may end up at 0) and move it to the end

*/
void free_node( database* db, node* n ) {

	assert( n != NULL );
	assert( n->id != 0 );

	for(size_t i=0; i<ARRAY_COUNT(db->node_cache); i++) {
		print("Cache[%lu] = id:%lu rc:%lu %p", i, 
			(db->node_cache[i].node_ptr == NULL ? 0 : db->node_cache[i].node_ptr->id), db->node_cache[i].refcount, db->node_cache[i].node_ptr );
		if( db->node_cache[i].node_ptr == NULL ) {
			break; // no more items after this
		}
		if( db->node_cache[i].node_ptr->id == n->id ) {
			node_cache_item found = db->node_cache[i];
			assert( found.node_ptr == n ); // TODO(improvement): maybe this is the thing to check on, both?
			print("found node %lu in cache, refcount: %lu", n->id, db->node_cache[i].refcount );
			assert( found.refcount != 0 );
			found.refcount--;
			
			// move this item to the end
			size_t num_nodeptrs_to_move = ARRAY_COUNT(db->node_cache) - i;// eg. 5 items, moving item 3 to the end, meaning items 4,5 shift left
			print("Moving %lu node_cache_items", num_nodeptrs_to_move );
			if( num_nodeptrs_to_move > 0 ) { // could be already at the end
				memmove( &db->node_cache[i], &db->node_cache[i+1], sizeof(node_cache_item) * num_nodeptrs_to_move );
			}
			
			db->node_cache[ LAST_INDEX(db->node_cache) ] = found;
			return;
		}
	}


	// it's not in the cache

	counters.frees++;
	print("node %lu (%p) (cr: %llu/ld: %llu/fr: %llu)", n->id, n, counters.creates, counters.loads, counters.frees );

	assert( counters.frees <= counters.loads + counters.creates );

	n->id = 0; // helps finding bugs if someone is still using this
	free( n );

}

/*
	Put a node pointer in the cache if there is space
	If the cache if not full (last item has a NULL nodeptr, meaning it's slot is free) put it in the beginning and move the rest over
	If the cache is full:
		Check if the last item has a refcount of 0, meaning nobody holds that pointer and it can be freed
		Free that item (no point in setting refcount to 0 or nodeptr to NULL since we shift it off anyway)
		Put it in the beginning and move the rest over, or just return if it was full

*/
void put_node_in_cache( database* db, node* n ) {
	
	print("Putting node %lu (%p) in the cache", n->id, n);
	
	// put it at the beginning of the array so it will be found fast
	// if we need it again, move the rest over
	
	// free the last node (if there is one)
	node_cache_item last_node_in_cache = db->node_cache[ LAST_INDEX(db->node_cache) ];
	if( last_node_in_cache.refcount == 0 ) {
		if( last_node_in_cache.node_ptr != NULL ){
			print("Evicting last node in cache: %lu", last_node_in_cache.node_ptr->id );
			database_store_node( db, last_node_in_cache.node_ptr );
			free_node( db, last_node_in_cache.node_ptr ); // TODO(granularity): maybe a separate free_just_node otherwise this will search the cache
		} else {
			prints("WTF: NULL node_ptr for refcount 0 item");
			assert(0);
		}
	} else {
		// no more space in cache
		return;
	}

	// shift the whole thing right by 1 node
	// ie. move all elements - 1 from index 0 to index 1
	size_t num_nodeptrs_to_move = (ARRAY_COUNT(db->node_cache)-1);
	print("Moving %lu node_cache_items", num_nodeptrs_to_move );
	memmove( &db->node_cache[1], &db->node_cache[0], sizeof(node_cache_item) * num_nodeptrs_to_move );
	
	// insert at the beginning
	db->node_cache[0] = (node_cache_item){ .refcount = 1, .node_ptr = n };
	
	for(size_t i=0; i<ARRAY_COUNT(db->node_cache); i++) {
		print("Cache[%lu] = id:%lu rc:%lu %p", i, 
			(db->node_cache[i].node_ptr == NULL ? 0 : db->node_cache[i].node_ptr->id), db->node_cache[i].refcount, db->node_cache[i].node_ptr );
	}
}

node* retrieve_node( database* db, size_t node_id ) {
	
	node* out = get_node_from_cache( db, node_id );
	if( out == NULL ) {
		out = load_node_from_file( db, node_id );
		put_node_in_cache( db, out );
	}
	
	assert( out->id != 0 );
	
	return out;
}

/*
	Get the node from the cache if it's in there.
	If it is, increment the refcount of that item by 1
	and puts it in the front of the cache
*/
node* get_node_from_cache( database* db, size_t node_id ) {

	print("Checking for node %lu in cache", node_id );
	for(size_t i=0; i<ARRAY_COUNT(db->node_cache); i++) {
		print("Cache[%lu] = id:%lu rc:%lu %p", i, 
				db->node_cache[i].node_ptr == NULL ? 0 : db->node_cache[i].node_ptr->id, 
				db->node_cache[i].refcount, 
				db->node_cache[i].node_ptr );
		if( db->node_cache[i].node_ptr == NULL ) {
			prints("No more items in cache");
			counters.cache_misses++;
			return NULL;
		}
		if( db->node_cache[i].node_ptr->id == node_id ) {
			prints("Cache hit!");
			node_cache_item found = db->node_cache[i];
			counters.cache_hits++;
			
			// now move it to the front
			size_t num_nodeptrs_to_move = i; // eg. moving item 3 means we shift items 0,1,2 right by 1
			print("Moving %lu node_cache_items", num_nodeptrs_to_move );
			memmove( &db->node_cache[1], &db->node_cache[0], sizeof(node_cache_item) * num_nodeptrs_to_move );
			
			// increment refcount since something wants to hold on to this
			found.refcount++;
	
			// insert at the beginning
			db->node_cache[0] = found;

			return found.node_ptr;
		}
	}
	counters.cache_misses++;
	return NULL;
}
