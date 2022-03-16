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

void display_progress( size_t current, size_t total ) {

	if( current % 10000 == 0 ) {
		printf("\r%.2f%%\t", 100. * (double)current / (double)total);
	}
}


entry_v map( const char* file ) {

    uint16 pagesize = (uint16) sysconf(_SC_PAGESIZE);

    print("map %s", file);

    entry_v result;

    FILE* fp = fopen( file, "r" );
    int fd = fileno( fp );
    struct stat sb;
    /* To obtain file size */
    if( fstat(fd, &sb) == -1 ) {
        perror("Could not fstat");
        exit( EXIT_FAILURE );
    }
    print("File %s size %lu\n", file, (uint64) sb.st_size );

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

    result.pos = result.head;
    result.value = 0;
    result.read = 0;
    result.consumed = 0;
    result.remaining_bytes = (uint64) sb.st_size;
    
    entry_next( &result );

    return result;
}

void entry_next( entry_v* e ) {

    if( e->remaining_bytes == 0 ) {
        print("Stream empty after %lu reads", e->read);
        return;
    }
    uint8 i = 0;
    uint64 diff = 0;
    while( *(e->pos + i) > 0x7f ) {
        // print("Varint read byte: %02x", *(e->pos + i) );
        diff |= (uint64) ( *(e->pos + i) & 0x7f ) << ( 7 * i );
        i++;
    }

    diff |= (uint64) ( *(e->pos + i) & 0x7f ) << ( 7 * i );
    // print("Varint read byte: %02x", *(e->pos + i) );

    e->value += diff;
    print("Varint result: %016lx", e->value);
    e->pos += i + 1;
    e->remaining_bytes -= (i + 1);
    e->read++;

    print("Remaining bytes: %lu read: %lu consumed: %lu", e->remaining_bytes, e->read, e->consumed );
}


uint8 varint_write( uint64 n, FILE* dest ) {

    // print("Varint write: %016lx", n);
    uint8 varint[10]; // worst care scenario is uint64 max which is 9 * 7 bits + final byte
    uint8 count = 0;

    while( n > 0x7f ) {
        // print("Varint write byte: %02x", (uint8) ( ( n & 0x7f ) | 0x80 ) );
        varint[count++] = (uint8) ( ( n & 0x7f ) | 0x80 ); // keep the bottom 7 bits, set the next byte bit
        n >>= 7;
    }
    varint[count++] = (uint8) ( n & 0x7f );
    // print("Varint write byte: %02x", (uint8) ( ( n & 0x7f ) ) );
    
    return (uint8) fwrite( varint, sizeof(uint8), count, dest );
}
