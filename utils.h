#ifndef BFCF_UTILS_H
#define BFCF_UTILS_H

#include <stdarg.h>

#include <sys/types.h>

/**
 * @brief boards block file
 * 
 */
typedef struct entry_2 {
	FILE* 	file;
	uint64  value;
    uint64  remaining_bytes;
	uint64	read;
	uint64	consumed;
} entry_2;


/**
 * @brief boards block file
 * 
 */
typedef struct entry_v {
    uint8* 	head;
    uint8* 	pos;
	uint64  value;
    uint64  remaining_bytes;
	uint64	read;
	uint64	consumed;
} entry_v;



typedef struct gen_counter {
	double cpu_time_used;
	unsigned long total_boards;
	unsigned long wins_white;
	unsigned long wins_black;
	unsigned long draws;
} gen_counter;

void display_progress( size_t current, size_t total );
entry_v map( const char* file );
entry_2 open_board_stream( const char* file );
uint8 varint_write( uint64 n, FILE* dest );
void entry_next( entry_v* e );
void entry_next_2( entry_2* e );
void print_bits(unsigned char c);


#define JOIN( buf, type, type_fmt, elements, size, separator ) { \
buf = malloc( 256 ); \
char str[100]; \
for( size_t i=0; i<(size_t)size; i++ ) { \
	sprintf( str, #type_fmt, (type) (elements)[i] ); \
	strcat( buf, str ); \
	if( i < ((size_t)size)-1 ) { \
		strcat( buf, separator ); \
	} \
} \
}

#endif
