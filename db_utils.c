#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "base.h"
#include "board.h"
#include "bplustree.h"

typedef enum command {

	ERROR,
	
	QUIT,

	OPEN_DB,
	ROW,
	NODE,
	KEYS,
	
	UNKNOWN_COMMAND,
	
	COMMAND_COUNT
} command;

typedef struct params {
	size_t count;
	char** param;
	char error[256];
} params;

command parse_command( char* s, params* p );
void print_row( FILE* in, size_t row_index );
node* get_node( database* db, size_t node_id );
void make_prompt( char* buf, char* db_name, node* cur );
void print_keys( node* n );
void open_database( char* name, FILE** table, FILE** index );

command parse_command( char* s, params* p ) {
	
//	printf("Parsing: '%s'\n", s);
	
	// just strtok 'command name' 'param0' 'param1'
	// commands can figure out what to do
	size_t param_count = 0;
	for( size_t i=0; i<strlen(s); i++ ) {
		if( s[i] == ' ' ) { // every param is preceded by space
			p->count++;
		}
	}

	p->param = (char**) malloc( p->count * sizeof(char*) );

	char* command = strtok( s, " " );
	char* element = strtok( NULL, " " );
//	printf("Command: '%s'\n", command );
	param_count = 0;
	while( element != NULL ) {
		p->param[ param_count ] = element;
//		printf("Param[%lu] '%s'\n", param_count, p->param[ param_count ] );
		param_count++;
		element = strtok( NULL, " ");
	}
	p->count = param_count;

	
	if( strcmp( command, "quit") == 0 ) {
		return QUIT;
	}
	
	if( strcmp( command, "keys") == 0 ) {
		return KEYS;
	}
	
	if( strcmp( command, "row") == 0 ) {

		if( p->count != 1 ) {
			strcpy( p->error, "row row_index");
			return ERROR;
		}
		return ROW;
	}
	
	if( strcmp( command, "node") == 0 ) {

		if( p->count != 1 ) {
			strcpy( p->error, "node ID");
			return ERROR;
		}
		return NODE;
	}
	
	if( strcmp( command, "open") == 0 ) {

		if( p->count != 1 ) {
			strcpy( p->error, "open [database]");
			return ERROR;
		}
		return OPEN_DB;
	}
	
	
	return UNKNOWN_COMMAND; //dunno
}

void print_row( FILE* in, size_t row_index ) {
	
	// off_t filepos = (off_t)row_index * (off_t)BOARD_SERIALIZATION_NUM_BYTES;
	//
	// struct stat s;
	// fstat( fileno(in), &s );
	// if( s.st_size <= filepos ) {
	// 	printf("No row %lu, file size is %llu\n", row_index, s.st_size );
	// 	return;
	// }
	//
	// if( fseek( in, (long) filepos, SEEK_SET ) ) {
	// 	perror("fseek()");
	// 	printf("Most likely no such row: %lu\n", row_index );
	// 	return;
	// }
	//
	//
	// board* b = read_board_record( in );
	// char buf[200];
	// sprintf( buf, "Row %lu - key 0x%lx", row_index, encode_board( b ) );
	// render( b, buf, false );
}


node* get_node( database* db, size_t node_id ) {

	if( node_id == 0 ) {
		printf("Node 0 is reserved, node IDs start at 1\n");
		return NULL;
	}

	// TODO(API): do we want load node to just read, or figure out where to read from what? Maybe opp. for diff granularity
	node* n = load_node_from_file( db, node_id );
	if( n == NULL ) {
		printf("Could not load node %lu\n", node_id );
	} else {
		printf("Node %lu - %lu key(s)%s\n", n->id, n->num_keys, n->is_leaf ? " {leaf}": "" );
	}
	return n;
}

// TODO(fix): make these print node ids
void print_keys( node* n ) {
	
	printf("Node %lu - %lu key(s)%s\n", n->id, n->num_keys, n->is_leaf ? " {leaf}": "" );
	printf("key                %s\n", n->is_leaf ? "Table row index" : "Left node id");
	for(size_t i=0; i<n->num_keys; i++) {
		printf("0x%017lx  %lu\n", n->keys[i], n->is_leaf ? n->pointers[i].board_data_index : n->pointers[i].child_node_id );
	}
	if( !n->is_leaf ) {
		printf("Right node id      %lu\n", n->pointers[n->num_keys].child_node_id );
	}
}

void make_prompt( char* buf, char* db_name, node* cur ) {
	
	buf[0] = '\0';
	strcpy( buf, "c4db");
	if( db_name[0] != '\0' ) {
		strcat( buf, " [" );
		strcat( buf, db_name);
		strcat( buf, "]" );
	}

	if( cur != NULL ) {
		strcat( buf, " [" );
		char nodeidstr[100];
		sprintf( nodeidstr, "%lu", cur->id );
		strcat( buf, nodeidstr );	
		strcat( buf, "]" );
	}

	
	strcat( buf, "> " );
}

int main( int argc, char** argv  ) {

	// REPL
	char buf[1024] = {0};
	bool keep_running = true;
	params p;
	database* db = NULL;
	
	size_t current_row = 0;
	size_t node_id = 0;
	node* current_node = NULL;
	char prompt[256];
	char db_name[256] = {0};
	
	if( argc == 2 ) {
		db = database_open( argv[1] );
		printf("Nodes %lu, Rows: %lu, Root Node ID: %lu\n", db->header->node_count, db->header->table_row_count, db->header->root_node_id);
	}
	
	do {
		make_prompt( prompt, db_name, current_node );
		printf( "%s", prompt );
		
		if( fgets( buf, 1024, stdin) == NULL ) {
			perror("fgets()");
			keep_running = false;
		} else {
			
			// strip newline from end of buf
			buf[ strlen(buf)-1 ] = '\0';
			command c = parse_command( buf, &p );
			
			switch( c ) {
				
				case COMMAND_COUNT:
					// removes warning
				break;
				
				case ERROR:
					printf( "%s\n", p.error );
				break;

				case QUIT:
					printf("Bye.\n");
					exit( EXIT_SUCCESS );

				case OPEN_DB:
					strcpy( db_name, p.param[0] );
					if( current_node != NULL ) {
						free( current_node );
						current_node = NULL;
					}
					db = database_open( db_name );

				break;
				
				case ROW:
					if( db == NULL ) {
						printf("No database open.\n");
						break;
					}
					current_row = (size_t)atoi( p.param[0] );
					print_row( db->table_file, current_row );
				break;

				case NODE:
					if( db == NULL ) {
						printf("No database open.\n");
						break;
					}
					if( current_node != NULL ) {
						free( current_node );
						current_node = NULL;
					}
					node_id = (size_t)atoi( p.param[0] );
					current_node = get_node( db, node_id );
				break;
				
				case KEYS:
					if( current_node == NULL ) {
						printf("No node selected, set one with 'node [id]'\n");
						break;
					}
					// print all keys from the current node
					print_keys( current_node );
				break;
				
				case UNKNOWN_COMMAND:
					printf("Unknown command: '%s'\n", buf);
					break;
					
			}

			p.count = 0;
			free( p.param );
			
		}
		
	} while( keep_running );
	
	if( db != NULL ) {
		database_close( db );
	}

	printf("Done.\n");
}
