#ifndef __UTILS_H__
#define __UTILS_H__

#include <stdarg.h>

#include <sys/types.h>

FILE* open_and_seek( const char* filename, const char* mode, off_t offset );
void create_empty_file( const char* filename );
void print_bits(unsigned char c);
size_t hash(size_t i);

// GCD for only positive ints and not caring about m==n==0 returning 0
size_t gcd(size_t m, size_t n);

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
