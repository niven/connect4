#ifndef BFCF_UTILS_H
#define BFCF_UTILS_H

#include <stdarg.h>

#include <sys/types.h>

typedef struct entry_v {
    uint8* 	head;
    uint8* 	pos;
	uint64  value;
    uint64  remaining_bytes;
	uint64	read;
	uint64	consumed;
} entry_v;


typedef struct gen_counter {
	double cpu_time_generate;
	double cpu_time_sort;
	unsigned long total_boards;
	unsigned long wins_white;
	unsigned long wins_black;
	unsigned long draws;
} gen_counter;

void display_progress( size_t current, size_t total );
entry_v map( const char* file );
uint8 varint_write( uint64 n, FILE* dest );
void entry_next( entry_v* e );
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
