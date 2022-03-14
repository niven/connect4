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

typedef struct merge_stats {
    uint64 read;
    uint64 emitted;
    uint64 skipped;
} merge_stats;



internal merge_stats merge( char* directory, glob_t files ) {

    merge_stats stats = { .read = 0, .emitted = 0, .skipped = 0 };

    uint16 count = (uint16) files.gl_pathc;
    entry stuff[ count ];

    for( uint16 i=0; i<count; i++ ) {
        stuff[i] = map( files.gl_pathv[i] );
    }

    char destination[255];
    sprintf( destination, "%s/boards", directory );
    FILE* out = fopen( destination, "w" );
    entry* target = &stuff[0];
    uint64 last_emitted = 0;
    while( target != NULL ) {

        target = NULL;

        for( uint16 i=0; i<count; i++ ) {

            print("Entry %hu: r:%lu c:%016lx", i, stuff[i].remaining, *(stuff[i].current));

            if( stuff[i].remaining == 0 ) {
                continue;
            }

            if( target == NULL ) {
                target = &stuff[i];
                print("Pick: %016lx", *(target->current)  );
            } else if( *(stuff[i].current) < *(target->current) ) {
                target = &stuff[i];
                print("Lower: %016lx", *(target->current)  );
            } else if( *(stuff[i].current) == *(target->current) ) {
                print("Skip: %016lx from entry %hu", *(stuff[i].current), i  );
                stuff[i].current++;
                stuff[i].remaining--;
                stats.skipped++;
                stats.read++;
           }
        }

        if( target != NULL ) {

            // Skip anything we already have emitted. This happens if there are multiple the same values in a single file
            if( *(target->current) != last_emitted ) {
                last_emitted = *(target->current);
                print("Emit: %016lx", last_emitted );
                fwrite( target->current, sizeof(uint64), 1, out );
                stats.emitted++;
            } else {
                print("Skip: %016lx from same stream", last_emitted );
                stats.skipped++;
            }

            target->current++;
            target->remaining--;
            stats.read++;
        }

    }
    fclose( out );

    for( uint16 i=0; i<count; i++ ) {
        if( munmap( (void*)stuff[i].head, (size_t) sysconf(_SC_PAGESIZE) ) == -1 ){
            perror("munmap");
        }
    }

    return stats;
}

int main( int argc, char** argv ) {

    srand( 23898645 );

    char* directory;
    if( argc != 2 ) {
        printf("Usage: merge [directory]\n");
        exit( EXIT_SUCCESS );
    }

    directory = argv[1];
    struct stat path_stat;
    stat(directory, &path_stat);
    if( S_ISREG(path_stat.st_mode) ) {
        printf("%s is not a directory\n", directory);
        exit( EXIT_SUCCESS );
    }

    glob_t glob_result;
    char pattern[255];
    sprintf( pattern, "%s/*.block", directory );
    glob( pattern, GLOB_TILDE, NULL, &glob_result );
    printf("Merging %lu files from %s\n", glob_result.gl_pathc, directory);

    merge_stats stats = merge( directory, glob_result );
    printf("Read: %lu\t\tEmitted: %lu\t\tSkipped: %lu\n", stats.read, stats.emitted, stats.skipped);

    exit( EXIT_SUCCESS );
}
