#include "../inline_asm.h"


bool check_conflict(uint8_t *target_addr, uint8_t **conflict_set, int conflict_set_size, 
	uint8_t **l2_occupy_set, int l2_occupy_set_size, uint64_t llc_miss_threshold){
	// TODO FIGURE OUT GETTING l2_occupy_set
	// TODO FIGURE OUT GETTING llc_miss_threshold

	// This comes from the paper
	_maccess(target_addr);
	for (int i = 0; i < conflict_set_size; i++){
		_maccess(conflict_set[i]);
	}
	// evict U from the L2 to the LLC
	for (int i = 0; i < l2_occupy_set_size; i ++){
		_maccess(l2_occupy_set[i]);
	}
	uint64_t start = _timer_start();
	_maccess(target_addr);
	uint64_t lat = _timer_end() - start;

	return lat >= llc_miss_threshold;

}


uint8_t ** find_EV(uint8_t **candidate_set, int candidate_size){
	// find eviction set function from the paper
	// for algorithm to work, all addr in CS must have the same LLC set index bits, and CS contains more than Wslice addresses for each slice
	// SHOULD RETURN WITH Wslice elements
	int max_rand = candidate_size - 10;
	int min_rand = 0;

	int rand_test_addr_index = (rand() % (max_rand - min_rand + 1)) + min_rand;
	uint8_t *test_addr = candidate_set[rand_test_addr_index];
	uint8_t ** EV ; //TODO ALLOCATE
	int EV_size = 0;

	// TODO lines 4-14 in the paper

	// this is lines 15-19 in the paper
	for (int i = 0; i < candidate_size; i++){
		if (check_conflict(candidate_set[i], EV, EV_size, ??, ??, ??)){

		}
	}

}

void generate_ev(uint8_t **EVtd, uint8_t **EVl2_mul){
	// Peter's attempt to do it
	// based on apendix of paper 
	// TODO GENERATE EVICTION SETS

	// TODO BE VERY CAREFUL ABOUT MEMORY HANDLING
}