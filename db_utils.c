#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "base.h"
#include "c4types.h"
#include "board.h"
#include "bplustree.h"

#define ROW_SIZE_BYTES 27

typedef enum command {

	ERROR,
	
	QUIT,
	
	OPEN_TABLE,
	ROW,

	OPEN_INDEX,
	NODE,
	
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
void print_index( FILE* in, size_t index_index );
void make_prompt( char* buf, char* table_file, char* index_file );

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

		if( p->count != 2 ) {
			strcpy( p->error, "open [table|index] filename");
			return ERROR;
		}
		
		if( strcmp( p->param[0], "table") == 0 ) {
			return OPEN_TABLE;
		}
		if( strcmp( p->param[0], "index") == 0 ) {
			return OPEN_INDEX;
		}

		strcpy( p->error, "open [table|index] filename");
		return ERROR;
	}
	
	
	return UNKNOWN_COMMAND; //dunno
}

void print_row( FILE* in, size_t row_index ) {
	
	off_t filepos = (off_t)row_index * ROW_SIZE_BYTES;

	struct stat s;
	fstat( fileno(in), &s );
	if( s.st_size <= filepos ) {
		printf("No row %lu, file size is %llu\n", row_index, s.st_size );
		return;
	}

	if( fseek( in, (long) filepos, SEEK_SET ) ) {
		perror("fseek()");
		printf("Most likely no such row: %lu\n", row_index );
		return;
	}
	
	
	board* b = read_board_record( in );
	char buf[200];
	sprintf( buf, "Row %lu", row_index );
	render( b, buf, false );
}


void print_index( FILE* in, size_t index_index ) {

	
	off_t filepos = (off_t)index_index * ROW_SIZE_BYTES;

	struct stat s;
	fstat( fileno(in), &s );
	if( s.st_size <= filepos ) {
		printf("No index %lu, file size is %llu\n", index_index, s.st_size );
		return;
	}

	if( fseek( in, (long) filepos, SEEK_SET ) ) {
		perror("fseek()");
		printf("Most likely no such index: %lu\n", index_index );
		return;
	}

	// TODO(API): do we want load node to just read, or figure out where to read from what? Maybe opp. for diff granularity
	node* n = bpt_load_node( in, index_index );
	printf("Node %lu - keys %lu\n", n->id, n->num_keys );
	
}



void make_prompt( char* buf, char* table_file, char* index_file ) {
	
	buf[0] = '\0';
	strcpy( buf, "c4db");
	if( table_file[0] != '\0' ) {
		strcat( buf, " T:" );
		strcat( buf, table_file);
	}
	if( index_file[0] != '\0' ) {
		strcat( buf, " I:" );
		strcat( buf, index_file);
	}
	
	strcat( buf, "> " );
}

int main(  ) {

	// REPL
	char buf[1024] = {0};
	bool keep_running = true;
	params p;
	FILE* table = NULL;
	char table_file[100];
	FILE* index = NULL;
	char index_file[100];
	size_t current_row = 0;
	size_t current_idx = 0;
	char prompt[256];
	
	do {
		make_prompt( prompt, table_file, index_file );
		printf( "%s", prompt );
		
		if( fgets( buf, 1024, stdin) == NULL ) {
			perror("fgets()");
			keep_running = false;
		} else {
			
			// strip newline form end of buf
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

				case OPEN_TABLE:
					if( table != NULL ) {
						fclose( table );
						table = NULL;
					}
					strcpy( table_file, p.param[1]);
					strcat( table_file, ".c4_table" );
					table = fopen( table_file, "r" );
					if( table == NULL ) {
						perror("fopen()");
					}
				break;
				
				case OPEN_INDEX:
					if( index != NULL ) {
						fclose( index );
						index = NULL;
					}
					strcpy( index_file, p.param[1]);
					strcat( index_file, ".c4_index" );
					index = fopen( index_file, "r" );
					if( index == NULL ) {
						perror("fopen()");
					}
				break;
				
				case ROW:
					if( table == NULL ) {
						printf("No table open.\n");
						break;
					}
					current_row = (size_t)atoi( p.param[0] );
					printf("Row %lu\n", current_row);
					print_row( table, current_row );
				break;

				case NODE:
					if( index == NULL ) {
						printf("No index open.\n");
						break;
					}
					current_idx = (size_t)atoi( p.param[0] );
					printf("Node %lu\n", current_idx);
					print_index( index, current_idx );
				break;
				
				case UNKNOWN_COMMAND:
					printf("Unknown command: '%s'\n", buf);
					break;
					
			}

			p.count = 0;
			free( p.param );
			
		}
		
	} while( keep_running );
	
	if( table != NULL ) {
		fclose( table );
	}
	if( index != NULL ) {
		fclose( index );
	}

	printf("Done.\n");
}
