
internal void bpt_print_leaf( node* n, int indent ) {

	char ind[100] = "                               END";
	ind[indent*2] = '\0';

	printf("%sL(%u)-[ ", ind, n->id);
	for( size_t i=0; i<n->num_keys; i++ ) {
		printf("0x%lx ", n->keys[i] );
	}
	printf("] - (%p) (#keys: %u, parent id %u)\n", (void *)n, n->num_keys, n->parent_node_id);

}


internal inline void bpt_print( database* db, node* start, int indent ) {

	char ind[100] = "                               END";
	ind[indent*2] = '\0';

	if( start->is_leaf ) {
		bpt_print_leaf( start, indent );
		return;
	}

	printf("%sN(%u) (%p) keys: %u (parent id %u)\n", ind, start->id, (void *)start, start->num_keys, start->parent_node_id);

	// print every key/node
	node* n;
	for( uint32 i=0; i<start->num_keys; i++ ) {
		n = retrieve_node( db, start->pointers[i].child_node_id  );
		assert( n != NULL );

		if( n->is_leaf ) {
			bpt_print_leaf( n, indent+1 );
		} else {
			bpt_print( db, n, indent+1 );
		}
		printf("%sK[%u]-[ 0x%lx ]\n", ind, i, start->keys[i] );
		release_node( db, n );

	}
	// print the last node
	n = retrieve_node( db, start->pointers[start->num_keys].child_node_id  );
	assert( n != NULL );
	bpt_print( db, n, indent + 1 );
	release_node( db, n );

}

internal inline board63 max_key( database* db, node* n ) {

	if( n->is_leaf ) {
		return n->keys[ n->num_keys-1 ];
	}

	node* last_node = retrieve_node( db, n->pointers[ n->num_keys ].child_node_id );
	board63 out = max_key( db, last_node );
	release_node( db, last_node );

	return out;
}

#ifdef VERBOSE
internal void check_tree_correctness( database* db, node* n ) {

	print("Checking correctness of node %u", n->id );

	// for leaves, check if all keys are ascending
	if( n->is_leaf ) {

		for(uint32 i=0; i<n->num_keys-1; i++ ) {
			assert( n->keys[i] < n->keys[i+1]);
		}
		print("node %u OK", n->id);
		return;
	}


	for(uint32 i=0; i<n->num_keys; i++ ) {
		// for every key, the max
		node* temp = retrieve_node( db, n->pointers[i].child_node_id );
		board63 max = max_key( db, temp );
		release_node( db, temp );
		print("key[%u] = 0x%lx, max from left node = 0x%lx", i, n->keys[i], max );
		assert( max < n->keys[i] );
	}

	// check the final one
	node* temp = retrieve_node( db, n->pointers[ n->num_keys ].child_node_id );
	board63 max = max_key( db, temp );
	release_node( db, temp );
	print("key[%u] = 0x%lx, max from right node = 0x%lx", n->num_keys-1, n->keys[ n->num_keys-1], max );
	assert( n->keys[ n->num_keys-1] <= max );
	print("node %u OK", n->id);
}
#else
#define check_tree_correctness( db, n ) // nothing
#endif
