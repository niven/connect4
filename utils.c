#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils.h"

// MurmurHash3 integer finalizer MOD cache buckets
// TODO(bug?): check if correct for size_t
size_t hash(size_t i) {

	size_t h = i;
	h ^= h >> 33;
	h *= 0xff51afd7ed558ccd;
	h ^= h >> 33;
	h *= 0xc4ceb9fe1a85ec53;
	h ^= h >> 33;

	return h;
}


// GCD for only positive ints and not caring about m==n==0 returning 0
size_t gcd(size_t m, size_t n) {
    if(m == 0 && n == 0)
        return 0;

    size_t r;
    while(n) {
        r = m % n;
        m = n;
        n = r;
    }
    return m;
}

FILE* open_and_seek( const char* filename, const char* mode, off_t offset ) {
	
	FILE* f = fopen( filename, mode );
	if( f == NULL ) {
		perror("fopen()");
		return NULL;
	}

	// fseek returns nonzero on failure
	if( fseek( f, offset, SEEK_SET ) ) {
		perror("fseek()");
		return NULL;
	}
	
	return f;
}

void create_empty_file( const char* filename ) {
	
	FILE* out = fopen( filename, "w" );
	if( out == NULL ) {
		perror("fopen()");
		exit( EXIT_FAILURE );
	}
	fclose( out );

}

void print_bits(unsigned char c) {
	char out[11];
	out[0] = '0';
	out[1] = 'b';
	for(int i=7; i>=0; i-- ) {
		out[2+i] = (c >> (7-i)) & 1 ? '1' : '0';
	}
	out[10] = '\0';
	printf("%s\n", out);
}

