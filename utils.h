#ifndef BFCF_UTILS_H
#define BFCF_UTILS_H

#include <stdarg.h>

#include <sys/types.h>

/**
 * @brief boards block file
 * 
 */
typedef struct entry {
    uint64* head;
    uint64* current;
	uint64  value;
    uint64  remaining;
} entry;


typedef struct gen_counter {
	double cpu_time_used;
	unsigned long total_boards;
	unsigned long wins_white;
	unsigned long wins_black;
	unsigned long draws;
} gen_counter;


entry map( const char* file );
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
