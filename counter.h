#ifndef __C4_COUNTER_H__
#define __C4_COUNTER_H__

typedef struct gen_counter {
	double cpu_time_used;
	unsigned long total_boards;
	unsigned long unique_boards;
	unsigned long wins_white;
	unsigned long wins_black;
	unsigned long draws;
} gen_counter;

void print_counter( gen_counter* c );

gen_counter* read_counter( const char* filename );

void write_counter( gen_counter* c, const char* filename );

#endif
