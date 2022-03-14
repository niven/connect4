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

    int index;
    if( argc != 3 ) {
        printf("Usage: show_board file index\n");
        exit( EXIT_SUCCESS );
    }

    index = atoi( argv[2] );


    board63 board_encoded;
    FILE* in;
    FOPEN_CHECK( in, argv[1], "r" )
    fseek( in, (long) (sizeof(board63) * (uint64) index), SEEK_SET );
    fread( &board_encoded, sizeof(board63), 1, in );
    fclose( in );

    board board_decoded;
    decode_board63( board_encoded, &board_decoded );
    char title[255];
    sprintf( title, "%s/%d", argv[1], index);

    render( &board_decoded, title, false );

    exit( EXIT_SUCCESS );
}
