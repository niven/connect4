#ifdef __linux
	// in order to fileno() we need a non-C99 feature
	// The fileno() function is widely supported and is in POSIX from 2001:
	// http://pubs.opengroup.org/onlinepubs/009604599/functions/fileno.html
	// To satisfy the fileno() Feature Test Macro Requirements, as can
	// be found in the fileno() man(3) page for the LibC in use, e.g.:
	// https://manpages.debian.org/stretch/manpages-dev/fileno.3.en.html
	// we ensure that _POSIX_C_SOURCE is defined, as required by:
	// http://pubs.opengroup.org/onlinepubs/009604599/basedefs/xbd_chap02.html#tag_02_02_01
	// We gaurd against redefining it, as it may be larger, e.g.: 200809L:
	// http://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap02.html#tag_02_02_01
	#ifndef _POSIX_C_SOURCE
	#define _POSIX_C_SOURCE 200112L
	#endif
#endif

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

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

    uint16 pagesize = (uint16) sysconf(_SC_PAGESIZE);

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
    print("File %s elements %lu\n", file, (uint64) sb.st_size / sizeof(uint64));

	uint64 length = ( 1 + (uint64) sb.st_size / pagesize ) * pagesize;

    result.head = mmap(
        NULL, // kernel picks mapping location. We don't care
        length,
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
