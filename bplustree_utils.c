
internal void bpt_print_leaf( node* n, int indent ) {
	
	char ind[100] = "                               END";
	ind[indent*2] = '\0';
	
	printf("%sL(%lu)-[ ", ind, n->id);
	for( size_t i=0; i<n->num_keys; i++ ) {
		printf("0x%lx ", n->keys[i] );
	}
	printf("] - (%p) (#keys: %lu, parent id %lu)\n", n, n->num_keys, n->parent_node_id);

}


internal void bpt_print( database* db, node* start, int indent ) {

	char ind[100] = "                               END";
	ind[indent*2] = '\0';
	
	if( start->is_leaf ) {
		bpt_print_leaf( start, indent );
		return;
	}

	printf("%sN(%lu) (%p) keys: %zu (parent id %lu)\n", ind, start->id, start, start->num_keys, start->parent_node_id);
		
	// print every key/node
	node* n;
	for( size_t i=0; i<start->num_keys; i++ ) {
		n = retrieve_node( db, start->pointers[i].child_node_id  );
		assert( n != NULL );

		if( n->is_leaf ) {
			bpt_print_leaf( n, indent+1 );
		} else {
			bpt_print( db, n, indent+1 );			
		}
		printf("%sK[%lu]-[ 0x%lx ]\n", ind, i, start->keys[i] );
		release_node( db, n );
		
	}
	// print the last node
	n = retrieve_node( db, start->pointers[start->num_keys].child_node_id  ); 
	assert( n != NULL );
	bpt_print( db, n, indent + 1 );
	release_node( db, n );

}

internal key_t max_key( database* db, node* n ) {
	
	if( n->is_leaf ) {
		return n->keys[ n->num_keys-1 ];
	}
	
	node* last_node = retrieve_node( db, n->pointers[ n->num_keys ].child_node_id );
	key_t out = max_key( db, last_node );
	release_node( db, last_node );

	return out;
}

internal void check_tree_correctness( database* db, node* n ) {
	
	print("Checking correctness of node %lu", n->id );
	
	// for leaves, check if all keys are ascending
	if( n->is_leaf ) {
		
		for(size_t i=0; i<n->num_keys-1; i++ ) {
			assert( n->keys[i] < n->keys[i+1]);
		}
		print("node %lu OK", n->id);
		return;
	}
	

	for(size_t i=0; i<n->num_keys; i++ ) {
		// for every key, the max
		node* temp = retrieve_node( db, n->pointers[i].child_node_id ); 
		key_t max = max_key( db, temp );
		release_node( db, temp );
		print("key[%lu] = 0x%lx, max from left node = 0x%lx", i, n->keys[i], max );
		assert( max < n->keys[i] );
	}
	
	// check the final one
	node* temp = retrieve_node( db, n->pointers[ n->num_keys ].child_node_id ); 
	key_t max = max_key( db, temp );
	release_node( db, temp );
	print("key[%lu] = 0x%lx, max from right node = 0x%lx", n->num_keys-1, n->keys[ n->num_keys-1], max );
	assert( n->keys[ n->num_keys-1] <= max );
	print("node %lu OK", n->id);
	
}
