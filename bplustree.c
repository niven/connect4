#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "base.h"
#include "utils.h"
#include "board.h"
#include "bplustree.h"

global_variable struct bpt_counters counters;

internal void bpt_insert_node( database* db, node* n, key_t up_key, size_t node_to_insert_id );
internal bool bpt_insert_or_update( database* db, node* n, record r );
internal void bpt_split( database* db, node* node );
internal void database_open_files( database* db );
internal void database_set_filenames( database* db, const char* name );
internal void database_store_node( database* db, node* data );
internal void database_store_row( database* db, size_t row_index, board* b );
internal off_t file_offset_from_node( size_t id );
internal off_t file_offset_from_row( size_t row_index );
internal void read_database_header( database* db );
internal void write_database_header( database* db );


#include "node_cache.c"
#include "bplustree_utils.c"


/************** stuff that deals with the fact we store things on disk ********************/


// the initial node is 1 (0 is reserved)
off_t file_offset_from_node( size_t id ) {
	
	assert( id != 0 );
	off_t offset = sizeof(database_header);
	offset += ((id-1) * sizeof(node));
	return offset;
	
}

off_t file_offset_from_row( size_t row_index ) {
	
	return (off_t)row_index * (off_t)BOARD_SERIALIZATION_NUM_BYTES;
}

void database_store_row( database* db, size_t row_index, board* b ) {

	print("storing board 0x%lx on disk as row %lu", encode_board(b), row_index );

	/* I'm always confused:
		‘r+’ Open an existing file for both reading and writing. The initial contents
		of the file are unchanged and the initial file position is at the beginning
		of the file.
	
		Currently we always append, in later stages we will probably end up deleting rows,
		so we can't use 'a'.
	*/
	off_t offset = file_offset_from_row( row_index );

	// move the file cursor to the initial byte of the row
	// fseek returns nonzero on failure
	if( fseek( db->table_file, offset, SEEK_SET ) ) {
		perror("fseek()");
		exit( EXIT_FAILURE );
	}

	print("storing %lu bytes at offset %llu", BOARD_SERIALIZATION_NUM_BYTES, offset );
	
	write_board_record( b, db->table_file );
	
}

void database_store_node( database* db, node* n ) {

	// append_log( db, "database_store_node(): writing node %lu (parent %lu) to %s\n", n->id, n->parent_node_id, db->index_filename );
	print("writing node %lu (parent %lu) to %s", n->id, n->parent_node_id, db->index_filename );
	
	off_t offset = file_offset_from_node( n->id );
	size_t node_block_bytes = sizeof( node );

	// fseek returns nonzero on failure
	if( fseek( db->index_file, offset, SEEK_SET ) ) {
		perror("fseek()");
		exit( EXIT_FAILURE );
	}
	
	print("storing %lu bytes at offset %llu", node_block_bytes, offset );
	
	size_t written = fwrite( n, node_block_bytes, 1, db->index_file );
	if( written != 1 ) {
		perror("fwrite()");
		exit( EXIT_FAILURE );
	}
	
}


void write_database_header( database* db ) {


	print("nodes: %lu - rows: %lu - root node id: %lu", db->header->node_count, db->header->table_row_count, db->header->root_node_id );

	fseek( db->index_file, 0, SEEK_SET );

	size_t objects_written = fwrite( db->header, sizeof(database_header), 1, db->index_file );

	if( objects_written != 1 ) {
		perror("frwite()");
		exit( EXIT_FAILURE );
	}
	
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

void database_open_files( database* db ) {
	
	// open the index file
	db->index_file = fopen( db->index_filename, "r+" );
	print("opened index file %s", db->index_filename);
	
	// open the table file
	db->table_file = fopen( db->table_filename, "r+" );
	print("opened table file %s", db->table_filename);

}

database* database_create( const char* name ) {


	database* db = (database*) malloc( sizeof(database) );
	if( db == NULL ) {
		perror("malloc()");
		exit( EXIT_FAILURE );
	}
	
	db->name = name;
	
	db->header = (database_header*) malloc( sizeof(database_header) );
	
	db->header->table_row_count = 0;
	db->header->node_count = 0;

	db->node_cache = (cache*) malloc( sizeof(cache) );
	assert( db->node_cache->num_stored == 0 );
	assert( db->node_cache->free_list == NULL );
	
	
	// create a new bpt
	node* first_node = new_node( ++db->header->node_count ); // get the next node id, and update count
	db->header->root_node_id = first_node->id;
	
	database_set_filenames( db, name );

	// create the index file
	create_empty_file( db->index_filename );
	print("created index file %s", db->index_filename);

	// create the table file
	create_empty_file( db->table_filename );
	print("created table file %s", db->table_filename);

	database_open_files( db );
	
	write_database_header( db );

	// it's empty, but just in case you call create/close or something	
	database_store_node( db, first_node );
	release_node( db, first_node );
	

	return db;
}

database* database_open( const char* name ) {

	
	database* db = (database*) malloc( sizeof(database) );
	if( db == NULL ) {
		perror("malloc()");
		exit( EXIT_FAILURE );
	}
	
	database_set_filenames( db, name );

	database_open_files( db );
	
	// read the root node, node_count and row_count
	read_database_header( db );
	print("nodes: %lu, rows: %lu, root node ID: %lu", db->header->node_count, db->header->table_row_count, db->header->root_node_id );

	db->node_cache = (cache*) malloc( sizeof(cache) );
	assert( db->node_cache->num_stored == 0 );
	assert( db->node_cache->free_list == NULL );
	
	return db;	
}

void database_close( database* db ) {
	
	print("closing %s and %s", db->table_filename, db->index_filename);
	
	write_database_header( db );
	
	clear_cache( db->node_cache );
	free( db->node_cache );
	
	free( db->header );
	
	fclose( db->index_file );
	fclose( db->table_file );

	free( db );
	
	prints("closed");
}

// TODO(bug): find a good way to do this, and not chase bugs for hours.
// global_variable key_t stored_keys[4096];
// global_variable int num_stored_keys;

bool database_put( database* db, board* b ) {

	// the index knows if we already have this record,
	// but in order to store it we need to have the offset in the table
	// which we know as we can just keep track of that
	
	board63 board_key = encode_board( b );
	print("key for this board: 0x%lx", board_key );
	
	record r = { .key = board_key, .value.table_row_index = db->header->table_row_count };

	counters.key_inserts++;

	print("loading root node ID %lu", db->header->root_node_id );
	print_index( db );

	node* root_node = retrieve_node( db, db->header->root_node_id );
	bool inserted = bpt_insert_or_update( db, root_node, r );
	print("inserted: %s", inserted ? "true" : "false");
	release_node( db, root_node );
	
	// tree might have grown, and since it grows upward *root might not point at the
	// actual root anymore. But since all parent pointers are set we can traverse up
	// to find the actual root
	prints("finding possible new root after insert");
	root_node = retrieve_node( db, db->header->root_node_id );
	print("Current root node %lu (parent %lu)", root_node->id, root_node->parent_node_id );
	while( root_node->parent_node_id != 0 ) {
		print("%lu is not the root, moving up to node %lu", root_node->id, root_node->parent_node_id );
		node* up = retrieve_node( db, root_node->parent_node_id );
		release_node( db, root_node );
		root_node = up;
	}
	db->header->root_node_id = root_node->id;
	
	print( "after insert: root node id: %lu (%p)", db->header->root_node_id, root_node );
	check_tree_correctness (db, root_node );
	release_node( db, root_node );
	
	// now write the data as a "row" to the table file

	if( inserted ) {
		// assert( num_stored_keys < 4000 ); // THIS BS AGAIN
		// key_t latest_key = encode_board( b );
		// print("Latest key: 0x%lx", latest_key);
		// for(int i=0; i< num_stored_keys; i++) {
		// 	print("Stored[%02d]: 0x%lx", i, stored_keys[i]);
		// 	assert( stored_keys[i] != latest_key );
		// }
		// stored_keys[ num_stored_keys++ ] = latest_key;
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


	// correctness checks:
	// 1. array has elements, and we should insert at the end, make sure the last element is smaller than the new one
	if( num_keys > 0 && *key_index == num_keys ) {
		assert( target_key > keys[num_keys-1] );
	}
	// 2. array has no elements (we already check this above, but left for completeness)
	if( num_keys == 0 ) {
		assert( *key_index == 0 );
	}
	// 3. array has elements, and we should insert at the beginning
	if( num_keys > 0 && *key_index == 0 ) {
		assert( target_key < keys[0] ); // MUST be smaller, otherwise it would have been found if equal
	}
	// 4. insert somewhere in the middle
	if( *key_index > 0 && *key_index < num_keys ) {
		assert( target_key < keys[*key_index] ); // insert shifts the rest right, MUST be smaller otherwise it would have been found
		assert( keys[*key_index-1] < target_key ); // element to the left is smaller
	}

	return BINSEARCH_INSERT;
}



node* new_node( size_t node_id ) {
	
	node* out = (node*)malloc( sizeof(node) );
	if( out == NULL ) {
		perror("malloc()");
		exit( EXIT_FAILURE );
	}
	
	out->id = node_id;
	
	out->parent_node_id = 0;
	
	out->num_keys = 0;
	out->is_leaf = true;
	
	counters.node_creates++;
	
	print("created node ID: %lu (%p) (cr: %llu/ld: %llu/fr: %llu)", out->id, out, counters.node_creates, counters.node_loads, counters.node_frees  );
	return out;
}

void bpt_dump_cf() {
	printf("BPT ORDER: %d\n", ORDER);
	printf("Total cache hits: %llu\n", counters.cache_hits);
	printf("Total cache misses: %llu\n", counters.cache_misses);
	printf("Total cache evicts: %llu\n", counters.cache_evicts);
	printf("Total cache entry allocs: %llu\n", counters.cache_entry_allocs);
	printf("Total cache entry frees: %llu\n", counters.cache_entry_frees);
	printf("Total cache free entry allocs: %llu\n", counters.cache_free_entry_allocs);
	printf("Total cache free entry frees: %llu\n", counters.cache_free_entry_frees);
	printf("Total node creates: %llu\n", counters.node_creates);
	printf("Total node loads: %llu\n", counters.node_loads);
	printf("Total node frees: %llu\n", counters.node_frees);
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
	assert( (counters.node_creates + counters.node_loads) == counters.node_frees );
}

void bpt_insert_node( database* db, node* n, key_t up_key, size_t node_to_insert_id ) {

	assert( n->id != 0 );
	check_tree_correctness( db, n );

	size_t k=0;
	while( k<n->num_keys && n->keys[k] < up_key ) { // TODO(performance): use binsearch
		k++;
	}
	print("insert key 0x%lx at position %zu, node at position %zu", up_key, k, k+1);
	// move keys over (could be 0 if at end)
	size_t elements_moving_right = n->num_keys - k;
//	printf("Moving %zu elements\n", elements_moving_right);
	
	// move keys right and set the key in the open space
	memmove( &n->keys[k+1], &n->keys[k], KEY_SIZE*elements_moving_right);
	n->keys[k] = up_key;
	
	// move pointers over (given that sibling is the right part of the split node,
	// move 1 more than the keys)
	memmove( &n->pointers[k+2], &n->pointers[k+1], sizeof(pointer)*elements_moving_right );
	n->pointers[k+1].child_node_id = node_to_insert_id;

	n->num_keys++;

#ifdef VERBOSE
	prints("after insert:");
	print_index( db );
#endif

	check_tree_correctness( db, n );

	// we might need to split (again)
	if( n->num_keys == ORDER ) {
		print("hit limit, have to split node %lu (%p)", n->id, n);
		bpt_split( db, n ); // propagate new node up
		return;
	} else {
		prints("did not split");
		database_store_node( db, n );
	}

}



void bpt_split( database* db, node* n ) {
	counters.splits++;

	print("node %lu: %s (%p)", n->id, (n->is_leaf ? "leaf" : "node"), n );
	print_index( db ); // TODO(granularity): function to print starting from a specific node
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
	print("moving %zu keys (%zu nodes/values) right from offset %zu (key = 0x%lx)", keys_moving_right, keys_moving_right+1, offset, n->keys[offset] );

	node* sibling = new_node( ++db->header->node_count );	
	memcpy( &sibling->keys[0], &n->keys[offset], KEY_SIZE*keys_moving_right );
	memcpy( &sibling->pointers[0], &n->pointers[offset], sizeof(pointer)*(keys_moving_right+1) );
	
	// housekeeping
	sibling->parent_node_id = n->parent_node_id;
	sibling->num_keys = keys_moving_right;
	sibling->is_leaf = n->is_leaf; // if n was NOT a leaf, then sibling can't be either.
	
	// if the node had subnodes (!is_leaf) then the subnodes copied to the sibling need
	// their parent pointer updated
	// TODO(performance): keep the subnodes around so update parent pointer doesn't hit the disk
	if( !n->is_leaf ) {
		for(size_t i=0; i < sibling->num_keys+1; i++ ) {
			node* s = retrieve_node( db, sibling->pointers[i].child_node_id );
			s->parent_node_id = sibling->id;
			database_store_node( db, s );
			release_node( db, s );
		}
	}
	
	// when splitting a node a key and 2 nodes move up
	n->num_keys = n->num_keys - keys_moving_right - (n->is_leaf ? 0 : 1);

	database_store_node( db, n );
	database_store_node( db, sibling ); // store it first, in case bpt_insert_node needs to load us

#ifdef VERBOSE
	print("created sibling node %lu (%p)", sibling->id, sibling);
	print_index_from( db, sibling->id );
	print("node %lu left over (%p)", n->id, n);
	print_index_from( db, n->id );
#endif
	
	// the key that moves up is the split key in the leaf node case, and otherwise
	// the key we didn't propagate to the sibling node and didn't keep in the node
	// which is the one we're splitting around... so in both cases this is the same.
	key_t up_key = n->keys[SPLIT_KEY_INDEX];
	print("key that moves up: 0x%lx", up_key);
	// now insert median into our parent, along with sibling
	// but if parent is NULL, we're at the root and need to make a new one
	if( n->parent_node_id == 0 ) {

		print("no parent, creating new root: ID %lu", db->header->node_count + 1 );
		node* new_root = new_node( ++db->header->node_count );

		new_root->keys[0] = up_key; // since left must all be smaller
		new_root->pointers[0].child_node_id = n->id;
		new_root->pointers[1].child_node_id = sibling->id;

		new_root->is_leaf = false;
		new_root->num_keys = 1;

		n->parent_node_id = sibling->parent_node_id = new_root->id;

		database_store_node( db, new_root );
		release_node( db, new_root );

		print("n->p %lu s->p %lu", n->parent_node_id, sibling->parent_node_id);
		
		// TODO(performance): we potentially store n+sibling twice in a row here
		database_store_node( db, n );
		database_store_node( db, sibling );

	} else {
		print("inserting key 0x%lx + sibling node %lu into parent %lu", up_key, sibling->id, n->parent_node_id );

		// so what we have here is (Node)Key(Node) so we need to insert this into the
		// parent as a package. Parent also has NkNkNkN and we've just replaced a Node
		// with a NkN. The parent is actually kkk, NNNN so finding where to insert the key
		// is easy and obvious, and the node we can just add the sibling beside our current node
		// (since in essence we represent the same data as before so must be adjacent)
		
		// now this is fairly doable, but it might lead to having to split the parent
		// as well
#ifdef VERBOSE
		print("going to insert into parent node ID %lu", n->parent_node_id);
		print_index( db );
#endif		
		counters.parent_inserts++;
		// TODO(performance): node cache
		node* parent = retrieve_node( db, n->parent_node_id );
		bpt_insert_node( db, parent, up_key, sibling->id );
		release_node( db, parent );
	}
	check_tree_correctness (db, sibling );
	check_tree_correctness (db, n );

	release_node( db, sibling );
	
}

/*
	Insert a record into the B+ Tree.
	If a key already exists we ODKU (On Duplicate Key Update)
	Since this is used as an index so there is no use case for dupe keys.
	We occasionally update and never delete so this seems more useful.
	But maybe not Update but Ignore instead
*/
bool bpt_insert_or_update( database* db, node* root, record r ) {

	assert( root->id != 0 );
	
	counters.insert_calls++;
	print("node %lu - %p", root->id, root);

//	printf("Insert %d:%d\n", r.key, r.value.value_int );
	size_t k=0;
	size_t insert_location = 0;

	if( root->is_leaf ) {
		print_index_from( db, root->id );

		int bsearch_result = binary_search( root->keys, root->num_keys, r.key, &insert_location);
		assert( bsearch_result == BINSEARCH_FOUND || bsearch_result == BINSEARCH_INSERT );
		print("Leaf node - insert location: %lu", insert_location);
		
		k = insert_location;
//		printf("Insertion location: keys[%d] = %d (atend = %d)\n", k, root->keys[k], k == root->num_keys );
		// if we're not appending but inserting, ODKU (we can't check on value since we might be at the end
		// and the value there could be anything)
		if( k < root->num_keys && root->keys[k] == r.key ) {
			print("Overwrite of keys[%lu] = %lu (value %lu to %lu)", k, r.key, root->pointers[k].table_row_index, r.value.table_row_index );
			root->pointers[k] = r.value;
			
			return false; // was not inserted but updated (or ignored)
		}
		
		// now insert at k, but first shift everything after k right
		memmove( &root->keys[k+1], &root->keys[k], KEY_SIZE*(root->num_keys - k) );
		memmove( &root->pointers[k+1], &root->pointers[k], sizeof(pointer)*(root->num_keys - k) );

		root->keys[k]  = r.key;
		root->pointers[k] = r.value;

		root->num_keys++;

		// split if full 
		// TODO(bug?): figure out when to save the root node, or maybe not at all?
		if( root->num_keys == ORDER ) {
			print("hit limit, have to split node %lu (%p)", root->id, root);
			bpt_split( db, root );
		} else {
			database_store_node( db, root );
		}
		
		return true; // an insert happened
	}


	// NOT a leaf node, recurse to insert
	// TODO: maybe just find_node()?
	int binsearch_result = binary_search( root->keys, root->num_keys, r.key, &insert_location);
	assert( binsearch_result == BINSEARCH_FOUND || binsearch_result == BINSEARCH_INSERT );

	if( binsearch_result == BINSEARCH_FOUND ) {
		print("key 0x%lx is already stored, bailing out.", r.key );
		return false; // TODO(clarity): maybe constants for these
	}

	// descend a node
	// TODO(bug?): maybe we don't always pick the right node here?
	if( insert_location < root->num_keys ) {
		print("Must be in node to the left of keys[%lu] = 0x%lx (node %lu)", insert_location, root->keys[insert_location], root->pointers[insert_location].child_node_id );
	} else {
		print("Must be in node to the right of keys[%lu] = 0x%lx (node %lu)", insert_location-1, root->keys[insert_location-1], root->pointers[insert_location].child_node_id );
	}
	print("Descending into node %lu", root->pointers[insert_location].child_node_id);

	// TODO(performance): node cache
	node* target = retrieve_node( db, root->pointers[insert_location].child_node_id );
	bool was_insert = bpt_insert_or_update( db, target, r );
	print("inserted into child node (was_insert: %s)", ( was_insert ? "true" : "false") );
	release_node( db, target );
	
	return was_insert;
}

/*
	Return the leaf node that should have key in it.
*/
internal node* bpt_find_node( database* db, node* root, key_t key ) {
	
	assert( root->id != 0 );
	
	node* current = root;
	
	while( !current->is_leaf ) {

		print("checking node %lu", current->id);
		
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

		size_t next_node_id = current->pointers[node_index].child_node_id;
		print("Moving to child node %lu", next_node_id);
		// TODO(elegance): must be a way to do this more elegant and without the if
		if( current != root ) {
			release_node( db, current );
		}
		current = retrieve_node( db, next_node_id );
	}
	
	print("returning node %lu", current->id);
	return current;
}

// TODO(performance): probably mmap() the file
node* load_node_from_file( database* db, size_t node_id ) {

	size_t node_block_bytes = sizeof( node );
	off_t node_block_offset = file_offset_from_node( node_id );

	// move the file cursor to the initial byte of the row
	// fseek returns nonzero on failure
	if( fseek( db->index_file, (long) node_block_offset, SEEK_SET ) ) {
		perror("fseek()");
		exit( EXIT_FAILURE );
	}
	
	// TODO(bug): Feel this might cause hard ot find bugs. Maybe a check for filesize and error out on fread fail
	// (also, no malloc ok check)
	node* n = (node*) malloc( sizeof(node) );
	size_t objects_read = fread( n, node_block_bytes, 1, db->index_file );
	if( objects_read != 1 ) {
		perror("fread()");
		free( n );
		print("unable to load node %lu", node_id);
		return NULL; // couldn't read a node
	}

	counters.node_loads++;

	print("retrieved node %lu (%p) (cr: %llu/ld: %llu/fr: %llu)", node_id, n, counters.node_creates, counters.node_loads, counters.node_frees );
	return n;
}

board* load_row_from_file( FILE* in, off_t offset ) {

	if( fseek( in, (long) offset, SEEK_SET ) ) {
		perror("fseek()");
		return NULL;
	}
	
	return read_board_record( in );
}

board* database_get( database* db, key_t key ) {
	
	print("loading root node ID %lu", db->header->root_node_id );
	node* root_node = retrieve_node( db, db->header->root_node_id );
	record* r = bpt_get( db, root_node, key );
	release_node( db, root_node );
	
	if( r == NULL ) {
		return NULL;
	}
	
	off_t offset = file_offset_from_row( r->value.table_row_index );
	print("Row offset: %llu", offset);

	return load_row_from_file( db->table_file, offset );
}

record* bpt_get( database* db, node* root, key_t key ) {

	assert( root->id != 0 );
	
	counters.get_calls++;
	print("key 0x%lx", key);
	node* dest_node = bpt_find_node( db, root, key );
	print("Found correct node %lu", dest_node->id);
	//bpt_print( dest_node, 0 );
	
	if( dest_node == NULL ) {
		fprintf( stderr, "No node found at all WTF\n");
		exit( EXIT_FAILURE );
	}
	
	// now we have a leaf that *could* contain our key, but it's sorted
	size_t key_index;
	if( BINSEARCH_FOUND != binary_search( dest_node->keys, dest_node->num_keys, key, &key_index ) ) {
		prints("Key not found");
		if( dest_node->id != root->id ) {
			release_node( db, dest_node );
		}
		return NULL;
	}
	print("Key index: %lu", key_index);
	
	record* r = (record*)malloc( sizeof(record) );
	if( r == NULL ) {
		perror("malloc()");
		exit( EXIT_FAILURE );
	}
	r->key = key;
	r->value = dest_node->pointers[key_index]; 
	print("returning record { key = 0x%lx, value.table_row_index = %lu / value.child_node_id = %lu }", r->key, r->value.table_row_index, r->value.child_node_id);

	if( dest_node->id != root->id ) {
		print("dest id %lu root id %lu", dest_node->id, root->id);
		release_node( db, dest_node );
	}
	return r;
	
}


void print_index_from( database* db, size_t start_node_id ) {

#ifdef VERBOSE		
	
	node* start = retrieve_node( db, start_node_id );
	if( start->is_leaf ) {
		printf("+---------------------- LEAF NODE %lu ------------------------------+\n", start->id);
		bpt_print_leaf( start, 0 );
		printf("+---------------------- END LEAF NODE %lu --------------------------+\n", start->id);	
	} else {
		printf("+---------------------- NODE %lu ------------------------------+\n", start->id);
		bpt_print( db, start, 0 );
		printf("+---------------------- END NODE %lu --------------------------+\n", start->id);	
	}
	release_node( db, start );

#endif
}

void print_index( database* db ) {	
	print_index_from( db, db->header->root_node_id );
}


// NOTE(performance): this could be very slow, but I'm not sure that will ever be relevant
size_t bpt_size( database* db, node* start ) {
	counters.any++;
	
	if( start->is_leaf ) {
		return start->num_keys;
	}
	
	size_t count = 0;
	for( size_t i=0; i<=start->num_keys; i++ ) { // 1 more pointer than keys
		node* child = retrieve_node( db, start->pointers[i].child_node_id );
		count += bpt_size( db, child );
		release_node( db, child );		
	}
	
	return count;
}

