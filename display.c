#include <fcntl.h>
#include <glob.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>

#include "base.h"
#include "utils.h"

#include "board.h"


int main( int argc, char** argv ) {

    uint64 index;
    if( argc != 3 ) {
        printf("Usage: show_board file index\n");
        exit( EXIT_SUCCESS );
    }

    index = (uint64) atoll( argv[2] );

    char title[255];
    sprintf( title, "%s/%lu", argv[1], index);

	entry_v boards = map( argv[1] );
    while( index --> 0 ) {
        entry_next( &boards );
    }

    printf("Board %016lx\n", boards.value);
    board board_decoded;
    decode_board63( boards.value, &board_decoded );

	// Note: mmap maps in multiples of page size, but why does unmap need it?
	if( munmap( (void*)boards.head, (size_t) sysconf(_SC_PAGESIZE) ) == -1 ){
		perror("munmap");
	}

    render( &board_decoded, title, false );

    exit( EXIT_SUCCESS );
}
