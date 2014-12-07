#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "base.h"
#include "utils.h"
#include "c4types.h"
#include "board63.h"
#include "board.h"
#include "bplustree.h"

global_variable struct bpt_counters counters;

internal void database_store_row( database* db, size_t row_index, board* b );
internal void database_store_node( database* db, node* data );
internal void read_database_header( database* db );
internal void write_database_header( database* db );
internal off_t file_offset_from_node( size_t id );
internal void database_set_filenames( database* db, const char* name );

off_t file_offset_from_node( size_t id ) {
	
	off_t offset = sizeof(database_header);
	offset += (id * sizeof(node));
	return offset;
	
}

void database_store_row( database* db, size_t row_index, board* b ) {

	print("storing board %p on disk as row %lu", b, row_index );

	/* I'm always confused:
		‘r+’ Open an existing file for both reading and writing. The initial contents
		of the file are unchanged and the initial file position is at the beginning
		of the file.
	
		Currently we always append, in later stages we will probably end up deleting rows,
		so we can't use 'a'.
	*/
	off_t offset = (off_t)row_index * (off_t)BOARD_SERIALIZATION_NUM_BYTES;
	FILE* out = open_and_seek( db->table_filename, "r+", offset );

	// move the file cursor to the initial byte of the row
	print("storing %lu bytes at offset %llu", BOARD_SERIALIZATION_NUM_BYTES, offset );
	
	write_board_record( b, out );
	
	fclose( out );
	
}



void database_store_node( database* db, node* n ) {

	print("writing node %lu (%p) to disk (%s)", n->id, n, db->index_filename );
	
	off_t offset = file_offset_from_node( n->id );
	size_t node_block_bytes = sizeof( node );
	
	FILE* out = open_and_seek( db->index_filename, "r+", offset );
	
	print("storing %lu bytes at offset %llu", node_block_bytes, offset );
	
	size_t written = fwrite( n, node_block_bytes, 1, out );
	if( written != 1 ) {
		perror("frwite()");
		exit( EXIT_FAILURE );
	}
	
	fclose( out );

}

// stuff that deals with the fact we store things on disk

void write_database_header( database* db ) {

	print("%p", db->index_file );
	fseek( db->index_file, 0, SEEK_SET );
	size_t objects_written = fwrite( db->header, sizeof(database_header), 1, db->index_file );
	if( objects_written != 1 ) {
		perror("frwite()");
		exit( EXIT_FAILURE );
	}
	prints("done");
}

void read_database_header( database* db ) {
	
	database_header* header = (database_header*) malloc( sizeof(database_header) );
	size_t objects_read = fread( header, sizeof(database_header), 1, db->index_file );
	if( objects_read != 1 ) {
		perror("fread()");
		exit( EXIT_FAILURE );
	}
	
	db->header = header;
	
}

void database_set_filenames( database* db, const char* name ) {

	int written = snprintf( db->index_filename, DATABASE_FILENAME_SIZE, "%s%s", name, ".c4_index" );
	assert( written < DATABASE_FILENAME_SIZE );

	written = snprintf( db->table_filename, DATABASE_FILENAME_SIZE, "%s%s", name, ".c4_table" );
	assert( written < DATABASE_FILENAME_SIZE );
	
}


database* database_create( const char* name ) {
	
	database* db = (database*) malloc( sizeof(database) );
	if( db == NULL ) {
		perror("malloc()");
		exit( EXIT_FAILURE );
	}
	
	db->header = (database_header*) malloc( sizeof(database_header) );
	
	db->header->table_row_count = 0;
	db->header->node_count = 0;

	// create a new bpt
	db->index = new_bptree( db->header->node_count++ ); // initial node ID is 0 when creating a new db
	db->header->root_node_id = db->index->id;
	
	database_set_filenames( db, name );

	// create the index file
	create_empty_file( db->index_filename );
	db->index_file = fopen( db->index_filename, "r+" );
	print("created index file %s", db->index_filename);
	
	// create the table file
	create_empty_file( db->table_filename );
	db->table_file = fopen( db->table_filename, "r+" );
	
	print("created table file %s", db->table_filename);
	
	write_database_header( db );
	
	// it's empty, but just in case you call create/close or something	
	database_store_node( db, db->index );
	
	return db;
}

database* database_open( const char* name ) {
	
	database* db = (database*) malloc( sizeof(database) );
	if( db == NULL ) {
		perror("malloc()");
		exit( EXIT_FAILURE );
	}
	
	database_set_filenames( db, name );

	// open the index file
	db->index_file = fopen( db->index_filename, "r+" );
	print("opened index file %s", db->index_filename);
	
	// open the table file
	db->table_file = fopen( db->table_filename, "r+" );
	print("opened table file %s", db->table_filename);
	
	// read the root node, node_count and row_count
	read_database_header( db );
	print("nodes: %lu, rows: %lu, root node ID: %lu", db->header->node_count, db->header->table_row_count, db->header->root_node_id );
	
	db->index = load_node_from_file( db->index_file, db->header->root_node_id );
	
	return db;
	
}

void database_close( database* db ) {
	
	// TODO(fix) don't think we need this when stuff is on disk
	print("closing %s and %s", db->table_filename, db->index_filename);
	
	write_database_header( db );

	// now we have to free all nodes we have in memory, which is at least the root node
	free( db->index );
	// TODO(bug): keep track of loaded nodes somewhere and release them here
	
	free( db->header );
	
	fclose( db->index_file );
	fclose( db->table_file );
	
	free( db );
	
	prints("closed");
}

bool database_put( database* db, board* b ) {
	
	// the index knows if we already have this record,
	// but in order to store it we need to have the offset in the table
	// which we know as we can just keep track of that
	
	board63* board_key = encode_board( b );
	print("key for this board: %lu", board_key->data );
	
	record r = { .key = board_key->data, .value.table_row_index = db->header->table_row_count };

	// TODO(bug): this should not overwrite, that means wasting space in the table file, also return inserted/dupe
	counters.key_inserts++;

	bool inserted = bpt_insert_or_update( db, db->index, r );

	// tree might have grown, and since it grows upward *root might not point at the
	// actual root anymore. But since all parent pointers are set we can traverse up
	// to find the actual root
	while( db->index->parent != NULL ) {
		print("%p is not the root, moving up to %p", db->index, db->index->parent );
		db->index = db->index->parent;
	}
	db->header->root_node_id = db->index->id;
	
	print( "root node id: %lu (%p)", db->header->root_node_id, db->index );

	
	// now write the data as a "row" to the table file
	if( inserted ) {
		database_store_row( db, db->header->table_row_count, b );
		db->header->table_row_count++;
	}
	
	return inserted;
	
}

/*
	Search keys for a target_key.
	returns
		BINSEARCH_FOUND: target_key is an element of keys, key_index is set to the location
		BINSEARCH_INSERT: target_key is NOT an element of keys, key_index is set to the index where you would need to insert target_key
		BINSEARCH_ERROR: keys is NULL, key_index is unchanged

*/
#define BINSEARCH_ERROR 0
#define BINSEARCH_FOUND 1
#define BINSEARCH_INSERT 2

internal unsigned char binary_search( key_t* keys, size_t num_keys, key_t target_key, size_t* key_index ) {

	// no data at all
	if( keys == NULL ) {
		return BINSEARCH_ERROR;
	}
	
	// empty array, or insert location should be initial element
	if( num_keys == 0 || target_key < keys[0] ) {
		*key_index = 0;
		return BINSEARCH_INSERT;
	}
	
	size_t span = num_keys;
	size_t mid = num_keys / 2;
	size_t large_half;
	while( span > 0 ) {

		counters.key_compares++;
		if( target_key == keys[mid] ) {
			*key_index = mid;
			return BINSEARCH_FOUND;
		}
		
		span = span/2; // half the range left over
		large_half = span/2 + (span % 2);// being clever. But this is ceil 

		counters.key_compares++;
		if( target_key < keys[mid] ) {
			mid -= large_half;
		} else {
			mid += large_half;
		}
		
	}

	// target_key is not an element of keys, but we found the closest location
	counters.key_compares++;
	if( mid == num_keys ) { // after all other elements
		*key_index = num_keys;
	} else if( target_key < keys[mid] ) {
		*key_index = mid; // displace, shift the rest right
	} else if( target_key > keys[mid] ) {
		*key_index = mid+1; // not sure if these two are both possible
	} else {
		assert(0); // cannot happen
	}

	return BINSEARCH_INSERT;
}

bpt* new_bptree( size_t node_id ) {
	
	bpt* out = (bpt*)malloc( sizeof(bpt) );
	if( out == NULL ) {
		perror("malloc()");
		exit( EXIT_FAILURE );
	}
	
	out->id = node_id;
	
	out->parent = NULL;
	
	out->num_keys = 0;
	out->is_leaf = true;
	
	print("created node ID: %lu (%p)", out->id, out );
	counters.creates++;
	
	return out;
}



void free_bptree( bpt* b ) {

	printf("free_bptree: %p\n", b);
	if( !b->is_leaf ) {
		for( size_t i=0; i<=b->num_keys; i++ ) { // Note: <= since pointers is num_keys+1
			free_bptree( b->pointers[i].node_ptr );
		}
	}
	free( b );
	counters.frees++;
}

void bpt_dump_cf() {
	printf("BPT ORDER: %d\n", ORDER);
	printf("Total creates: %llu\n", counters.creates);
	printf("Total frees: %llu\n", counters.frees);
	printf("Total key inserts: %llu\n", counters.key_inserts);
	printf("Total get calls: %llu\n", counters.get_calls);
	printf("Total splits: %llu\n", counters.splits);
	printf("Total insert calls: %llu\n", counters.insert_calls);
	printf("Total parent_inserts: %llu\n", counters.parent_inserts);
	printf("Total generic key compares: %llu\n", counters.key_compares);
	printf("Total leaf key compares: %llu\n", counters.leaf_key_compares);
	printf("Total node key compares: %llu\n", counters.node_key_compares);
	if( counters.key_inserts > 0 ) {
		printf("Key compares (leaf+node) per key insert: %llu\n", counters.key_compares / counters.key_inserts );
	}
	printf("Anycounter: %llu\n", counters.any);
	assert( counters.creates == counters.frees );
}

void bpt_insert_node( database* db, bpt* n, key_t up_key, bpt* sibling ) {

	size_t k=0;
	while( k<n->num_keys && n->keys[k] < up_key ) { // TODO: this is dumb and should binsearch
		k++;
	}
	print("insert key %lu at position %zu, node at position %zu", up_key, k, k+1);
	// move keys over (could be 0 if at end)
	size_t elements_moving_right = n->num_keys - k;
//	printf("Moving %zu elements\n", elements_moving_right);
	
	// move keys right and set the key in the open space
	memmove( &n->keys[k+1], &n->keys[k], KEY_SIZE*elements_moving_right);
	n->keys[k] = up_key;
	
	// move pointers over (given that sibling is the right part of the split node,
	// move 1 more than the keys)
	memmove( &n->pointers[k+2], &n->pointers[k+1], sizeof(pointer)*elements_moving_right );
	n->pointers[k+1].node_ptr = sibling;

	n->num_keys++;

#ifdef VERBOSE
	prints("after insert:");
	bpt_print( n, 0 );
#endif

	// we might need to split (again)
	if( n->num_keys == ORDER ) {
		print("hit limit, have to split %p", n);
		bpt_split( db, n ); // propagate new node up
		return;
	} else {
		prints("did not split");
		database_store_node( db, n );
	}

}



void bpt_split( database* db, bpt* n ) {
	counters.splits++;
#ifdef VERBOSE	
	bpt_print( n, 0 );
#endif
	print("%s id:%lu (%p)", (n->is_leaf ? "leaf" : "node"), n->id, n );
	
	/* 
	Create a sibling node
	
	2 cases: (1) the node is a leaf, (2) the node isn't a leaf
	
	(1) - Leaf node
	
		Imagine a btp with ORDER=4. This means we split when we have 4 keys: [ 1 2 3 4 ]
		We split this into 2 nodes around key index 1 = [ 2 ]
		Result:	
					[ 2 ]
			 [ 1 ]    [ 2 3 4 ]
		This means key 2 is pushed up, and we still have key 2 in the sibling node since that's
		where we store the associated value.
		This means the split means copying 3 keys/values from the node to the sibling, and pushing key 2 up.
	
	(2) - Not a leaf node
	
		Example: [ 2 3 4 5 ]
		But now these hold keys, and the values are pointers to nodes that hold the same keys (and the values).
	
		Initial situation after inserting 7:
				[2]  [3]  [4]		    <-- keys + node pointers
			[1]  [2]  [3]  [4 5 6 7] <-- keys + values
	
		The end structure should look like this:
	
							[3]						<-- keys + node pointers
					[2]			[4] [5]			<-- keys + node pointers
				[1]  [2]    [3]  [4]  [5 6 7]	<-- keys + values
	
		So first we split the leaf node [4 5 6 7] like before, and we end up with:
	
				[2]  [3]  [4]  [5]		   <-- keys + node pointers
			[1]  [2]  [3]  [4]  [5 6 7]   <-- keys + values
	
		Now we need to split the node with keys + pointers, so call bpt_split() again.
	
		We need to push up key 3, but copy 2 keys: [4 5] to the sibling, and 3 node pointers: [3] [4] [5 6 7]
		The reason is that we don't need an intermediary key 3 since the key+value of 3 are already in a 
		leaf node at the bottom.
	
		Intermediate result:
	
			Node				Sibling
			[2]			 [4]  [5]
		[1]  [2]     [3]  [4]  [5 6 7]
	
		Now 2 more cases: (a) node above this level, (b) no node above this level
	
		(b) - No node above this level (it's easier)
	
		Create a new node with 1 key (3) and 2 node pointers to Node and Sibling
		Set the parent pointers of Node and Sibling to point to the new root.
	
		(a) - Node above this level
		
		This node must already have a key and a pointer to Node (and the key in this example must be 1)
		
		Insert key 3 in the parent node (shifting others if needed)
		Insert Sibling in the parent node (shifting others if needed)
		
		
	So in all cases we need to create a sibling node and copy items over:
	
	is_leaf:
			copy K keys: ORDER/2 + 1 (for even ORDER: half+1, for odd ORDER: the larger half)
			copy N values (same as the number of keys)
	
	!is_leaf:
			copy K keys: ORDER - ORDER/2 (for even ORDER: half, for odd ORDER: the larger half)
			copy N pointers: K+1
	*/

	// first step: create sibling and copy the right amount of keys/values/pointers
	// when splitting a !leaf: the middle item goes up (and we don't store that intermediate key)
	// to in case of an ODD keycount: just floor(order/2) (example: 3 -> 1, 9 -> 4)
	// in case of an EVEN keycount: the larger half: floor(order/2) (example: 4 -> 2, 10 -> 5)
	size_t keys_moving_right = n->is_leaf ? ORDER/2 + 1 : ORDER/2;
	size_t offset = n->is_leaf ? SPLIT_KEY_INDEX : SPLIT_NODE_INDEX;
	print("Moving %zu keys (%zu nodes) right from offset %zu (key = %lu)", keys_moving_right, keys_moving_right+1, offset, n->keys[offset] );

	bpt* sibling = new_bptree( db->header->node_count++ );	
	memcpy( &sibling->keys[0], &n->keys[offset], KEY_SIZE*keys_moving_right );
	memcpy( &sibling->pointers[0], &n->pointers[offset], sizeof(pointer)*(keys_moving_right+1) );
	
	// housekeeping
	sibling->parent = n->parent;
	sibling->num_keys = keys_moving_right;
	sibling->is_leaf = n->is_leaf; // if n was NOT a leaf, then sibling can't be either.
	
	// if the node had subnodes (!is_leaf) then the subnodes copied to the sibling need
	// their parent pointer updated
	// TODO: this looks dumb and is probably slow
	if( !n->is_leaf ) {
		for(size_t i=0; i < sibling->num_keys+1; i++ ) {
			sibling->pointers[i].node_ptr->parent = sibling;
		}
	}
	
	
	// when splitting a node a key and 2 nodes mode up
	n->num_keys = n->num_keys - keys_moving_right - (n->is_leaf ? 0 : 1);

#ifdef VERBOSE
	print("Created sibling %p", sibling);
	bpt_print( sibling, 0 );
	print("Node left over %p", n);
	bpt_print( n, 0 );
#endif
	
	// the key that moves up is the split key in the leaf node case, and otherwise
	// the key we didn't propagate to the sibling node and didn't keep in the node
	// which is the one we're splitting around... so in both cases this is the same.
	key_t up_key = n->keys[SPLIT_KEY_INDEX];
	print("Key that moves up: %lu", up_key);
	// now insert median into our parent, along with sibling
	// but if parent is NULL, we're at the root and need to make a new one
	if( n->parent == NULL ) {

		bpt* new_root = new_bptree( db->header->node_count++ );
		print("No parent, creating new root: %p", new_root);

		new_root->keys[0] = up_key; // since left must all be smaller
		new_root->pointers[0].node_ptr = n;
		new_root->pointers[1].node_ptr = sibling;

		new_root->is_leaf = false;
		new_root->num_keys = 1;

		database_store_node( db, new_root );

		n->parent = sibling->parent = new_root;

#ifdef VERBOSE
		print("new root %p", new_root);
		bpt_print( new_root, 0 );
#endif
	} else {
		print("inserting key %lu + sibling node %p into parent %p", up_key, sibling, n->parent );
		// so what we have here is (Node)Key(Node) so we need to insert this into the
		// parent as a package. Parent also has NkNkNkN and we've just replaced a Node
		// with a NkN. The parent is actually kkk, NNNN so finding where to insert the key
		// is easy and obvious, and the node we can just add the sibling beside our current node
		// (since in essence we represent the same data as before so must be adjacent)
		
		// now this is fairly doable, but it might lead to having to split the parent
		// as well
#ifdef VERBOSE
		prints("Parent node:");
		bpt_print( n->parent, 0 );
#endif		
		counters.parent_inserts++;
		bpt_insert_node( db, n->parent, up_key, sibling );

	}
	
	prints("writing changes to disk");
	database_store_node( db, n );
	database_store_node( db, sibling );
	
}

/*
	Insert a record into the B+ Tree.
	If a key already exists we ODKU (On Duplicate Key Update)
	Since this is used as an index so there is no use case for dupe keys.
	We occasionally update and never delete so this seems more useful.
	But maybe not Update but Ignore instead
*/
bool bpt_insert_or_update( database* db, bpt* root, record r ) {
	
	counters.insert_calls++;
	print("node %lu - %p", root->id, root);

//	printf("Insert %d:%d\n", r.key, r.value.value_int );
	size_t k=0;
	size_t insert_location = 0;

	if( root->is_leaf ) {

		// now we can't just go ahead and add it at the end, we need to keep
		// this shit sorted. (and remember to move the pointers as well)
		// So maybe instead of a keys and pointers array just have a
		// struct with key/pointer. But anyway.

		if( binary_search( root->keys, root->num_keys, r.key, &insert_location) ) {
			print("Leaf node - insert location: %lu", insert_location);
		} else {
			fprintf( stderr, "BS NOT FOUND\n" );
			assert(0);
		}
		
		// correctness checks:
		// 1. array has elements, and we should insert at the end, make sure the last element is smaller than the new one
		if( root->num_keys > 0 && insert_location == root->num_keys ) {
			assert( r.key > root->keys[root->num_keys-1] );
		}
		// 2. array has no elements
		if( root->num_keys == 0 ) {
			assert( insert_location == 0 );
		}
		// 3. array has elements, and we should insert at the beginning
		if( root->num_keys > 0 && insert_location == 0 ) {
			assert( r.key <= root->keys[insert_location] ); // could be equal!
		}
		// 4. insert somewhere in the middle
		if( insert_location > 0 && insert_location < root->num_keys ) {
			assert( r.key <= root->keys[insert_location] ); // insert shifts the rest right, so element right should be equal/larger
			assert( root->keys[insert_location-1] < r.key ); // element to the left is smaller
		}
		
		k = insert_location;
//		printf("Insertion location: keys[%d] = %d (atend = %d)\n", k, root->keys[k], k == root->num_keys );
		// if we're not appending but inserting, ODKU (we can't check on value since we might be at the end
		// and the value there could be anything)
		if( k < root->num_keys && root->keys[k] == r.key ) {
//			printf("Overwrite of keys[%d] = %d (value %d to %d)\n", k, r.key, root->pointers[k].value_int, r.value.value_int );
			root->pointers[k] = r.value;
			return false; // was not inserted but updated (or ignored)
		}
		
		// now insert at k, but first shift everything after k right
		// TODO: These don't overlap afaict, why not memcpy? src==dest maybe?
		memmove( &root->keys[k+1], &root->keys[k], KEY_SIZE*(root->num_keys - k) );
		memmove( &root->pointers[k+1], &root->pointers[k], sizeof(pointer)*(root->num_keys - k) );

		root->keys[k]  = r.key;
		root->pointers[k] = r.value;

		root->num_keys++;

		// split if full
		if( root->num_keys == ORDER ) {
			print("hit limit, have to split ID: %lu (%p)", root->id, root);
			bpt_split( db, root );
		} else {
			database_store_node( db, root );
		}
		
		return true; // an insert happened
	}


	// NOT a leaf node, recurse to insert
	// TODO: maybe just find_node()?
	if( binary_search( root->keys, root->num_keys, r.key, &insert_location) ) {
		print("Descend node - insert location: %lu", insert_location);
	} else {
		fprintf( stderr, "BS NOT FOUND\n" );
		assert(0);
	}

	// TODO: move these to the binary_search function?
	// correctness checks:
	// 1. array has elements, and we should insert at the end, make sure the last element is smaller than the new one
	if( root->num_keys > 0 && insert_location == root->num_keys ) {
		assert( r.key > root->keys[root->num_keys-1] );
	}
	// 2. array has no elements
	if( root->num_keys == 0 ) {
		assert( insert_location == 0 );
	}
	// 3. array has elements, and we should insert at the beginning
	if( root->num_keys > 0 && insert_location == 0 ) {
		assert( r.key <= root->keys[insert_location] ); // could be equal!
	}
	// 4. insert somewhere in the middle
	if( insert_location > 0 && insert_location < root->num_keys ) {
		assert( r.key <= root->keys[insert_location] ); // insert shifts the rest right, so element right should be equal/larger
		assert( root->keys[insert_location-1] < r.key ); // element to the left is smaller
	}

	// descend a node
	print("Must be in left pointer of keys[%lu] = %lu", insert_location, root->keys[insert_location] );
	return bpt_insert_or_update( db, root->pointers[insert_location].node_ptr, r );
	prints("Inserted into child node");

}

/*
	Return the leaf node that should have key in it.
*/
internal node* bpt_find_node( database* db, bpt* root, key_t key ) {
	
	node* current = root;

	while( !current->is_leaf ) {
		print("checking node %p", current);
		
		size_t node_index;
		switch( binary_search( current->keys, current->num_keys, key, &node_index) ) {
			case BINSEARCH_FOUND:
				node_index++; // if we hit the key, the correct node is to the right of that key
				break; // needless break here
			case BINSEARCH_INSERT:
				// insert means we determined the target is to the left of this key (which is the correct node) 
				break;
			case BINSEARCH_ERROR:
			default:
				fprintf( stderr, "Somehow have a node with a NULL keys array\n");
				assert(0);
		}

		// correctness checks
		if( node_index == 0 ) { // target should be smaller than key 0
			assert( key < current->keys[node_index] );
		} else { // target should be equal/bigger than the key to the left of that node 
			assert( key >= current->keys[node_index-1] );
		}

		current = current->pointers[node_index].node_ptr;
	}
	
	// now we have an index to a leaf, which is on disk
	size_t leaf_node_index = 0;
	node* leaf_node = load_node_from_file( db->index_file, leaf_node_index );
	free( leaf_node );
	
	print("returning node %p", current);
	return current;
}

node* load_node_from_file( FILE* index_file, size_t node_id ) {
	print("retrieve node %lu", node_id);

	size_t node_block_bytes = sizeof( node );
	off_t node_block_offset = file_offset_from_node( node_id );

	// move the file cursor to the initial byte of the row
	// fseek returns nonzero on failure
	if( fseek( index_file, (long) node_block_offset, SEEK_SET ) ) {
		perror("fseek()");
		exit( EXIT_FAILURE );
	}
	
	node* n = (node*) malloc( sizeof(node) );
	size_t objects_read = fread( n, node_block_bytes, 1, index_file );
	if( objects_read != 1 ) {
		perror("fread()");
		return NULL; // couldn't read a node
	}
	
	return n;

}

record* bpt_get( database* db, bpt* root, key_t key ) {
	
	counters.get_calls++;
	print("key %lu", key);
	node* dest_node = bpt_find_node( db, root, key );
	print("Found correct node %p", dest_node);
	//bpt_print( dest_node, 0 );
	
	if( dest_node == NULL ) {
		fprintf( stderr, "No node found at all WTF\n");
		exit( EXIT_FAILURE );
	}
	
	// now we have a leaf that *could* contain our key, but it's sorted
	size_t key_index;
	if( BINSEARCH_FOUND != binary_search( dest_node->keys, dest_node->num_keys, key, &key_index ) ) {
		prints("Key not found");
		return NULL;
	}
	print("Index for key we're looking for: %lu", key_index);
	
	record* r = (record*)malloc( sizeof(record) );
	if( r == NULL ) {
		perror("malloc()");
		exit( EXIT_FAILURE );
	}
	r->key = key;
	r->value = dest_node->pointers[key_index]; 
	
	return r;
	
}

internal void bpt_print_leaf( node* n, int indent ) {
	
	char ind[100] = "                               END";
	ind[indent*2] = '\0';
	
	printf("%sL-[ ", ind);
	for( size_t i=0; i<n->num_keys; i++ ) {
		printf("%lu ", n->keys[i] );
	}
	printf("] - ID:%lu (%p) (parent %p)\n", n->id, n, n->parent);

}

void bpt_print( bpt* root, int indent ) {

	char ind[100] = "                               END";
	ind[indent*2] = '\0';
	
	if( root->is_leaf ) {
		bpt_print_leaf( root, indent );
		return;
	}
	printf("%sN ID:%lu (%p) keys: %zu (parent %p)\n", ind, root->id, root, root->num_keys, root->parent);
		
	// print every key/node
	node* n;
	for( size_t i=0; i<root->num_keys; i++ ) {
		n = root->pointers[i].node_ptr;
		assert( n != NULL );

		if( n->is_leaf ) {
			bpt_print_leaf( n, indent+1 );
		} else {
			bpt_print( n, indent+1 );			
		}
		printf("%sK-[ %lu ]\n", ind, root->keys[i] );
	}
	// print the last node
	n = root->pointers[root->num_keys].node_ptr;
	assert( n != NULL );
	bpt_print( n, indent + 1 );

}

// TODO: find out WTF is going on. According to iprofiler this is BY FAR the slowest thing here. WTF?
// update: it's not slow. doing a size after 300K items at ORDER 512 means around 1K nodes, AKA 1K calls
// so either keep the size in the root (make it different from the node) or make this not recursive?
size_t bpt_size( bpt* root ) {
	counters.any++;
	
	if( root->is_leaf ) {
		return root->num_keys;
	}
	
	size_t count = 0;
	for( size_t i=0; i<=root->num_keys; i++ ) { // 1 more pointer than keys
		count += bpt_size( root->pointers[i].node_ptr );
	}
	
	return count;
}

