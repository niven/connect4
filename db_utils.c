#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "base.h"
#include "c4types.h"
#include "board.h"

#define ROW_SIZE_BYTES 27

typedef enum command {

	ERROR,
	
	QUIT,
	
	OPEN_TABLE,
	ROW,

	OPEN_INDEX,
	
	COMMAND_COUNT
} command;

typedef struct params {
	size_t count;
	char** param;
	char error[256];
} params;

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
	
	
	return -1; //dunno
}

void print_row( FILE* in, size_t row_index ) {
	
	size_t filepos = row_index * ROW_SIZE_BYTES;

	struct stat s;
	fstat( fileno(in), &s );
	if( s.st_size < filepos ) {
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

int main( int argc, char** argv ) {

	// REPL
	char buf[1024] = {0};
	bool keep_running = true;
	params p;
	FILE* table = NULL;
	char table_file[100];
	size_t current_row = 0;
	do {
		printf("c4db %s> ", (table == NULL ? "" : table_file));
		
		if( fgets( buf, 1024, stdin) == NULL ) {
			perror("fgets()");
			keep_running = false;
		} else {
			
			// strip newline form end of buf
			buf[ strlen(buf)-1 ] = '\0';
			command c = parse_command( buf, &p );
			
			switch( c ) {
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
				case ROW:
					if( table == NULL ) {
						printf("No table open.\n");
						break;
					}
					current_row = atoi( p.param[0] );
					printf("Row %lu\n", current_row);
					print_row( table, current_row );
					break;
				default:
				printf("Unknown command: '%s'\n", buf);
			}

			p.count = 0;
			free( p.param );
			
		}
		
	} while( keep_running );
	
	if( table != NULL ) {
		fclose( table );
	}

	printf("Done.\n");
}