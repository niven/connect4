#include <fcntl.h>
#include <glob.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include "base.h"
#include "utils.h"


typedef struct merge_stats {
    uint64 read;
    uint64 emitted;
    uint64 skipped;
    uint64 delta_bytes;
    double cpu_time_used;
} merge_stats;



internal merge_stats merge( char* directory, glob_t files ) {

    merge_stats stats = { .read = 0, .emitted = 0, .skipped = 0, .delta_bytes = 0, .cpu_time_used = 0.0 };

	clock_t cpu_time_start = clock();

    uint16 count = (uint16) files.gl_pathc;
    entry_v board_stream[ count ];

    uint64 total_bytes = 0;
    for( uint16 i=0; i<count; i++ ) {
        board_stream[i] = map( files.gl_pathv[i] );
        total_bytes += board_stream[i].remaining_bytes;
    }

    char destination[255];
    sprintf( destination, "%s/boards", directory );
    FILE* out_delta = fopen( destination, "w" );

    entry_v* target = &board_stream[0];
    uint64 previous = 0;
    uint64 progress_counter = 0;

    while( target != NULL ) {

        target = NULL;

        for( uint16 i=0; i<count; i++ ) {

            print("Stream %hu: read: %lu consumed: %lu value: %016lx", i, board_stream[i].read, board_stream[i].consumed, board_stream[i].value );

            if( board_stream[i].remaining_bytes == 0 && board_stream[i].read == board_stream[i].consumed ) {
                print("Stream %hu empty", i);
                continue;
            }

            if( target == NULL ) {
                target = &board_stream[i];
                print("Pick: %016lx", target->value );
            } else if( board_stream[i].value < target->value ) {
                target = &board_stream[i];
                print("Lower: %016lx", target->value  );
            } else if( board_stream[i].value == target->value ) {
                print("Skip: %016lx from entry %hu", board_stream[i].value, i  );
                board_stream[i].consumed++;
                // CHECK IF THERE IS ANYHTING LEFT!
                entry_next( &board_stream[i] );
                stats.skipped++;
                stats.read++;
           }
        }

        if( target != NULL ) {

            // Skip anything we already have emitted. This happens if there are multiple the same values in a single file
            if( target->value != previous ) {

                // Varint encode the deltas to save space
                stats.delta_bytes += varint_write( target->value - previous, out_delta );

                previous = target->value;
                print("Emit: %016lx", previous );
                stats.emitted++;

            } else {
                print("Skip: %016lx from same stream", previous );
                stats.skipped++;
            }

            entry_next( target );
            target->consumed++;
            stats.read++;
            progress_counter++;
            if( progress_counter > 10000 ) {
                uint64 bytes_remaining = 0;
                for( uint16 i=0; i<count; i++ ) {
                    bytes_remaining += board_stream[i].remaining_bytes;
                }

                display_progress( total_bytes - bytes_remaining, total_bytes );
                progress_counter = 0;
            }
            print("Target read: %lu consumed: %lu value:%016lx", target->read, target->consumed, target->value );

        }

    }

    fclose( out_delta );

    for( uint16 i=0; i<count; i++ ) {
        if( munmap( (void*)board_stream[i].head, (size_t) sysconf(_SC_PAGESIZE) ) == -1 ){
            perror("munmap");
        }
    }

	stats.cpu_time_used = ((double)( clock() - cpu_time_start ) / CLOCKS_PER_SEC );

    print("Uncompressed bytes out: %lu\n", stats.emitted * sizeof(uint64) );
    print("Delta varint bytes out: %lu\n", stats.delta_bytes);

    return stats;
}

int main( int argc, char** argv ) {

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

    merge_stats result = merge( directory, glob_result );

	char stats_file[255];
	sprintf( stats_file, "%s/stats.txt", directory );
	FILE* stats;
	FOPEN_CHECK( stats, stats_file, "a" )
	fprintf(stats, "Merge CPU time: %f\n", result.cpu_time_used );
	fprintf(stats, "Merge reads: %lu\n", result.read );
	fprintf(stats, "Merge emits: %lu\n", result.emitted );
	fprintf(stats, "Merge skips: %lu\n", result.skipped );
	fprintf(stats, "Uncompressed bytes out: %lu\n", result.emitted * sizeof(uint64) );
	fprintf(stats, "Delta varint bytes out: %lu\n", result.delta_bytes );
	fprintf(stats, "Delta varint saved: %.2f%%\n",  100.0 * (double) ( result.emitted * sizeof(uint64) - result.delta_bytes ) /  (double)( sizeof(uint64) * result.emitted ) );
    
	fclose( stats );


    exit( EXIT_SUCCESS );
}
