#include <fcntl.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <string.h>

#include <sys/mman.h>
#include <sys/stat.h>

#include <unistd.h>

#include "base.h"

int main( int argc, char** argv ) {

    srand( 23898645 );

    uint16 blocks = 4;
    uint64 elements = 128;
    if( argc == 3 ) {
        blocks = (uint16) atoi( argv[1] );
        elements = (uint64) atoi( argv[2] );
    }
    print("Making %hu blocks of %lu elements", blocks, elements );

    uint64 block[elements];

    for( uint16 i = 0; i<blocks; i++ ) {

        char filename[255];
        sprintf( filename, "sample_%06hu.block", i);
        print("Generating file: %s", filename);
        uint64 step = rand() % 100; // ensure some collisions between multiple sample files
        block[0] = rand() % 100; // ensure some overlap

        FILE* fp = fopen( filename, "w" );
        if( !fp ) {
            perror("File opening failed");
            exit( EXIT_FAILURE );
        }

        for( uint64 j = 1; j < elements; j++ ) {
            block[j] = block[ j-1 ] + step;
        }
        fwrite( block, sizeof(uint64), elements, fp );

        fclose(fp);
    }

    exit( EXIT_SUCCESS );
}
