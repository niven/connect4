#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/mman.h>
#include <sys/stat.h>

#include "base.h"
#include "utils.h"

// MurmurHash3 integer finalizer MOD cache buckets (32/64 bit versions)
#ifdef BUILD_32_BITS

size_t hash( size_t i ) {

	size_t h = i;
	h ^= h >> 16;
	h *= 0x85ebca6b;
	h ^= h >> 13;
	h *= 0xc2b2ae35;
	h ^= h >> 16;

	return h;
}

#else

size_t hash( size_t i ) {

	assert( sizeof(size_t) == 8 ); // check 64 bits

	size_t h = i;
	h ^= h >> 33;
	h *= 0xff51afd7ed558ccd;
	h ^= h >> 33;
	h *= 0xc4ceb9fe1a85ec53;
	h ^= h >> 33;

	return h;
}
#endif

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

	FILE* f;
	FOPEN_CHECK( f, filename, mode )

	// fseek returns nonzero on failure
	if( fseek( f, offset, SEEK_SET ) ) {
		perror("fseek()");
		return NULL;
	}

	return f;
}

void create_empty_file( const char* filename ) {

	FILE* out;
	FOPEN_CHECK( out, filename, "w" )

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


entry map( const char* file ) {

    uint16 pagesize = (uint16) getpagesize();

    print("map %s", file);

    entry result;

    FILE* fp = fopen( file, "r" );
    int fd = fileno( fp );
    struct stat sb;
    /* To obtain file size */
    if( fstat(fd, &sb) == -1 ) {
        perror("Could not fstat");
        exit( EXIT_FAILURE );
    }
    printf("File %s elements %lu\n", file, (uint64) sb.st_size / sizeof(uint64));

    result.head = mmap(
        NULL, // kernel picks mapping location. We don't care
        pagesize,
        PROT_READ,
        MAP_PRIVATE,
        fd,
        0 // start of the file
    );
    fclose(fp); // mmap adds a ref


    if( result.head == MAP_FAILED ) {
        perror("Could not mmap");
        exit( EXIT_FAILURE );
    }

    result.current = result.head;
    result.remaining = (uint64) sb.st_size / sizeof(uint64);

    return result;
}
