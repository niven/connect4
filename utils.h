#ifndef __UTILS_H__
#define __UTILS_H__

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
