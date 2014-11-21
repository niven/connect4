#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "base.h"
#include "bplustree.h"



internal int binary_search( key_t* keys, size_t num_keys, key_t target_key, size_t* key_index ) {
	
	// no data at all
	if( keys == NULL || num_keys == 0 ) {
		return -1;
	}
	
	// out of range
	if( target_key < keys[0] || target_key > keys[num_keys-1] ) {
		return -1;
	}
	
	size_t mid = num_keys / 2;
	size_t large_half;
	while( num_keys > 0 ) {

		if( target_key == keys[mid] ) {
			*key_index = mid;
			return true;
		}
		
		num_keys = num_keys/2; // half the range left over
		large_half = num_keys/2 + (num_keys % 2);// being clever. But this is ceil 

		if( target_key < keys[mid] ) {
			mid -= large_half;
		} else {
			mid += large_half;
		}
		
	}

	return false;
}

bpt* new_bptree() {
	
	bpt* out = (bpt*)malloc( sizeof(bpt) );
	if( out == NULL ) {
		perror("malloc()");
		exit( EXIT_FAILURE );
	}
	
	out->parent = NULL;
	
	out->num_keys = 0;
	out->is_leaf = true;
	
	return out;
}

void free_bptree( bpt* b ) {

//	printf("free_bptree: %p\n", b);
	if( !b->is_leaf ) {
		for( size_t i=0; i<=b->num_keys; i++ ) { // Note: <= since pointers is num_keys+1
			free_bptree( b->pointers[i].node_ptr );
		}
	}
	free( b );
	
}

void bpt_insert_node( bpt* node, key_t up_key, bpt* sibling ) {

	printf("insert_node(): inserting %lu into:\n", up_key);
	print_bpt( node, 0 );
	size_t k=0;
	while( k<node->num_keys && node->keys[k] < up_key ) { // TODO: this is dumb and should binsearch
		k++;
	}
	printf("insert_node(): should insert key %lu at position %zu\n", up_key, k);
	// move keys over (could be 0 if at end)
	size_t elements_moving_right = node->num_keys - k;
	printf("Moving %zu elements\n", elements_moving_right);
	
	// move keys right and set the key in the open space
	memmove( &node->keys[k+1], &node->keys[k], KEY_SIZE*elements_moving_right);
	node->keys[k] = up_key;
	
	// move pointers over (given that sibling is the right part of the split node,
	// move 1 more than the keys)
	memmove( &node->pointers[k+2], &node->pointers[k+1], sizeof(pointer)*elements_moving_right );
	node->pointers[k+1].node_ptr = sibling;

	node->num_keys++;

	printf("insert_node(): after insert:\n");
	print_bpt( node, 0 );

	// we might need to split (again)
	if( node->num_keys == ORDER ) {
		printf("insert_node(): hit limit, have to split\n");
		bpt_split( &node );
	}

}


void bpt_split( bpt** root ) {
	
	bpt* n = *root;
	printf("Split around key[%d] = %lu\n", SPLIT_KEY_INDEX, n->keys[SPLIT_KEY_INDEX] );
	print_bpt( n, 0 );
	
	// create a sibling node and copy everything from median to end
	bpt* sibling = new_bptree();	
	size_t elements_moving_right = (ORDER - ORDER/2) +1; // top half plus our new item
	memcpy( &sibling->keys[0], &n->keys[SPLIT_KEY_INDEX], KEY_SIZE*elements_moving_right );
	memcpy( &sibling->pointers[0], &n->pointers[SPLIT_KEY_INDEX], sizeof(pointer)*elements_moving_right );
	// housekeeping
	sibling->parent = n->parent;
	sibling->num_keys = elements_moving_right;
	sibling->is_leaf = n->is_leaf; // if n was NOT a leaf, then sibling can't be either.
	n->num_keys = n->num_keys - elements_moving_right; // 1 goes up

	printf("Created sibling %p\n", sibling);
	print_bpt( sibling, 0 );
	printf("Node left over %p\n", n);
	print_bpt( n, 0 );
	
	// now insert median into our parent, along with sibling
	// but if parent is NULL, we're at the root and need to make a new one
	if( n->parent == NULL ) {
		bpt* new_root = new_bptree();
		printf("No parent, creating new root: %p\n", new_root);

		new_root->keys[0] = sibling->keys[0]; // since left must all be smaller
		new_root->pointers[0].node_ptr = n;
		new_root->pointers[1].node_ptr = sibling;

		new_root->is_leaf = false;
		new_root->num_keys = 1;

		n->parent = sibling->parent = new_root;

		*root = new_root;
		
	} else {
		key_t up_key = n->keys[SPLIT_KEY_INDEX];
		printf("Inserting median(%lu) + sibling into parent\n", up_key );
		// so what we have here is (Node)Key(Node) so we need to insert this into the
		// parent as a package. Parent also has NkNkNkN and we've just replaced a Node
		// with a NkN. The parent is actually kkk, NNNN so finding where to insert the key
		// is easy and obvious, and the node we can just add the sibling beside our current node
		// (since in essence we represent the same data as before so must be adjacent)
		
		// now this is fairly doable, but it might lead to having to split the parent
		// as well, so I really need some better insert() function to do this
		// (insert a key+node kind of thing)
		print_bpt( sibling, 0 );
		bpt_insert_node( n->parent, up_key, sibling );
	}
	
}

/*
	Insert a record into the B+ Tree.
	If a key already exists we ODKU (On Duplicate Key Update)
	Since this is used as an index so there is no use case for dupe keys.
	We occasionally update and never delete so this seems more useful.
*/
void bpt_insert_or_update( bpt** tree, record r ) {
	
	bpt* root = (*tree);
//	printf("Insert %d:%d\n", r.key, r.value.value_int );

	if( root->is_leaf ) {

		// now we can't just go ahead and add it at the end, we need to keep
		// this shit sorted. (and remember to move the pointers as well)
		// So maybe instead of a keys and pointers array just have a
		// struct with key/pointer. But anyway.
		size_t k=0;
		while( k<root->num_keys && root->keys[k] < r.key ) { // TODO: this is dumb and should binsearch
			k++;
		}
//		printf("Insertion location: keys[%d] = %d (atend = %d)\n", k, root->keys[k], k == root->num_keys );
		// if we're not appending but inserting, ODKU (we can't check on value since we might be at the end
		// and the value there could be anything)
		if( k < root->num_keys && root->keys[k] == r.key ) {
//			printf("Overwrite of keys[%d] = %d (value %d to %d)\n", k, r.key, root->pointers[k].value_int, r.value.value_int );
			root->pointers[k] = r.value;
			return;
		}
		
		// now insert at k, but first shift everything after k right
		memmove( &root->keys[k+1], &root->keys[k], KEY_SIZE*(root->num_keys - k) );
		memmove( &root->pointers[k+1], &root->pointers[k], sizeof(pointer)*(root->num_keys - k) );

		root->keys[k]  = r.key;
		root->pointers[k] = r.value;

		root->num_keys++;

		// split if full
		if( root->num_keys == ORDER ) {
			bpt_split( tree );
		}
		
		return;
	}


	for( size_t i=0; i<root->num_keys; i++ ) {
//		printf("Checking %d against key %d\n", r.key, root->keys[i] );
		if( r.key < root->keys[i] ) {
//			printf("Must be in left pointer of keys[%d] = %d\n", i, root->keys[i] );
			bpt_insert_or_update( &(root->pointers[i].node_ptr), r );
			return;
		}
	}

//	printf("Wasn't in any left pointers, must be in right then\n");
	if( r.key >= root->keys[root->num_keys-1] ) {
		bpt_insert_or_update( &(root->pointers[root->num_keys].node_ptr), r );
		return;
	}

	fprintf( stderr, "BIG FAT FAIL: insert()\n");
}



internal node* bpt_find_node( bpt* root, key_t key ) {
	
	node* current = root;
	int found = false;
	while( !current->is_leaf ) {
		found = false;
		
		for( size_t i=0; i<current->num_keys; i++ ) {
//			printf("Checking %d against key %d\n", key, current->keys[i] );
			if( key < current->keys[i] ) {
				current = current->pointers[i].node_ptr;
				found = true;
				break;
			}
		}
		if( !found ) {
			current = current->pointers[ current->num_keys ].node_ptr;
		}
	}
	
	return current;
}

record* bpt_find( bpt* root, key_t key ) {
	
	node* dest_node = bpt_find_node( root, key );
//	printf("Found correct node:\n");
//	print_bpt( dest_node, 0 );
	
	if( dest_node == NULL ) {
		fprintf( stderr, "No node found at all WTF\n");
		exit( EXIT_FAILURE );
	}
	
	// now we have a leaf that *could* contain our key, but it's sorted
	size_t key_index;
//	printf("Index for key we're looking for: %d\n", key_index);
	if( !binary_search( dest_node->keys, dest_node->num_keys, key, &key_index ) ) {
		return NULL;
	}
	
	record* r = (record*)malloc( sizeof(record) );
	if( r == NULL ) {
		perror("malloc()");
		exit( EXIT_FAILURE );
	}
	r->key = key;
	r->value = dest_node->pointers[key_index]; 
	
	return r;
	
}

internal void print_bpt_leaf( node* n, int indent ) {
	
	char ind[100] = "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\tEND";
	ind[indent] = '\0';
	
	printf("%sL-[ ", ind);
	for( size_t i=0; i<n->num_keys; i++ ) {
		printf("%lu ", n->keys[i] );
	}
	printf("]\n");

}

void print_bpt( bpt* root, int indent ) {

	char ind[100] = "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\tEND";
	ind[indent] = '\0';
	
	if( root->is_leaf ) {
		print_bpt_leaf( root, indent );
		return;
	}
	printf("%sN %p keys: %zu\n", ind, root, root->num_keys);
		
	for( size_t i=0; i<root->num_keys; i++ ) {
		if( root->pointers[i].node_ptr->is_leaf ) {
			print_bpt_leaf( root->pointers[i].node_ptr, indent );
		} else {
			print_bpt( root->pointers[i].node_ptr, indent+1 );			
		}
		printf("%sK-[ %lu ]\n", ind, root->keys[i] );
	}
	print_bpt( root->pointers[root->num_keys].node_ptr, indent+1 );

}

unsigned long bpt_count_records( bpt* root ) {
	
	unsigned long count = 0;
	if( root->is_leaf ) {
		return count + root->num_keys;
	}
	
	for( size_t i=0; i<=root->num_keys; i++ ) { // 1 more pointer than keys
		count += bpt_count_records( root->pointers[i].node_ptr );
	}
	
	return count;
}

