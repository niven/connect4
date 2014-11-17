#include <stdio.h>
#include <stdlib.h>

#include "c4_index.h"

int main(int argc, char** argv) {

	block_header* bh = c4_index_new( "test.index" );
	
	srand( 42 );
	
	int count = atoi( argv[1] );
	printf("storing %d numbers\n", count);
	
	for( int i=0; i<count; i++ ) {
		integral_type n = (integral_type) (rand() % MAX_NUM);
		printf("Storing %lu\n", (unsigned long)n );
		c4_index_store( bh, n, i );
	}

	integral_type dupe = 203;
	printf("Storing dupe 203: %lu\n", (unsigned long)dupe );
	unsigned long record_offset = 0;
	if( c4_index_retrieve( bh, dupe, &record_offset ) == 0 ) {
		printf("Not found, storing\n");
		c4_index_store( bh, 203, 1000 );
	} else {
		printf("Great success! DUPE!\n");
	}
	
	
	c4_index_free( bh );
	
	printf("Done\n");

	return 0;
}
