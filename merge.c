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
    uint64 delta_saved;
    double cpu_time_used;
} merge_stats;



internal merge_stats merge( char* directory, glob_t files ) {

    merge_stats stats = { .read = 0, .emitted = 0, .skipped = 0, .delta_saved = 0, .cpu_time_used = 0.0 };

	clock_t cpu_time_start = clock();

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
                uint64 diff = *(target->current) - last_emitted;
                if( diff < 0x10 ) {
                    // 4 bit number 1 byte
                    print("Diff[1]: %016lx -> 0x0 . %02x\n", diff, (uint16) ((0x1 << 4) | diff));
                    stats.delta_saved += 8 - 0;
                } else if( diff < 0x1000 ) {
                    // 12 bit number 2 bytes
                    print("Diff[2]: %016lx -> %04x\n", diff, (uint16) ((0x1 << 12) | diff) );
                    stats.delta_saved += 8 - 1;
                } else if( diff < 0x100000 ) {
                    // 20 bit number 3 bytes
                    print("Diff[3]: %016lx -> 0x2 . %06x\n", diff, (uint32) diff);
                    stats.delta_saved += 8 - 2;
                } else if( diff < 0x10000000 ) {
                    // 28 bit number 4 bytes
                    print("Diff[4]: %016lx\n", diff);
                    stats.delta_saved += 8 - 3;
                } else if( diff < 0x1000000000 ) {
                    // 36 bit number 5 bytes
                    print("Diff[5]: %016lx\n", diff);
                    stats.delta_saved += 8 - 4;
                } else if( diff < 0x100000000000 ) {
                    // 44 bit number 6 bytes
                    print("Diff[6]: %016lx\n", diff);
                    stats.delta_saved += 8 - 5;
                } else if( diff < 0x10000000000000 ) {
                    // 52 bit number 7 bytes
                    print("Diff[7]: %016lx\n", diff);
                    stats.delta_saved += 8 - 6;
                } else if( diff < 0x1000000000000000 ) {
                    // 60 bit number 8 bytes
                    print("Diff[8]: %016lx\n", diff);
                    stats.delta_saved += 8 - 7;
                } else {
                    // 64 bit number 9 bytes
                    print("Diff[9]: %016lx\n", diff);
                    stats.delta_saved += 8 - 8;
                }


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

	stats.cpu_time_used = ((double)( clock() - cpu_time_start ) / CLOCKS_PER_SEC );

    printf("Bytes saved: %lu\n", stats.delta_saved);

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
	fprintf(stats, "Delta encoding bytes saved: %lu\n", result.delta_saved - result.emitted );
	fprintf(stats, "Delta encoding saved: %.2f%%\n",  100.0 * (double) ( result.delta_saved - result.emitted) /  (double)( sizeof(uint64) * result.emitted ) );
    
	fclose( stats );


    exit( EXIT_SUCCESS );
}
