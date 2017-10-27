#ifndef BFCF_COUNTER_H
#define BFCF_COUNTER_H

typedef struct gen_counter {
	double cpu_time_used;
	unsigned long database_size;
	double cache_hit_ratio;
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
