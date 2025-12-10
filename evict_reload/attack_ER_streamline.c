// call as below:
// taskset -c 0,2,4,6 ./attack_ER 

// #include "../inline_asm.h"
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>
// #include <x86intrin.h>
#include <stdbool.h>
// #include find_eviction_set.h
#include "cache/evset.h"
#include "build_ev.h"

#define CACHE_PAGE_SIZE 4096
#define CACHE_LINE_SIZE 64
#define NUM_PAGE_PER_ALLOC 5
#define TARGET_PAGE_SIZE CACHE_PAGE_SIZE * NUM_PAGE_PER_ALLOC
#define SYMBOL_CNT (1 << (sizeof(char) * 8))

// #define EPOCH_LENGTH 2000000. // A very conservative epoch length
#define EPOCH_LENGTH 20000
// 20000 = ~14 kB/s
#define N_EPOCH_START_DELAY 100
// 40 sometimes works
// need to run a little to let them sync up
#define DUMMY_RUNS 30

#define WTD 11
#define WL2 16
#define NUM_REPEATS 1

#define NUMSTREAMS 16

// Message we want to send
// const uint64_t msg = 0x12345678910;
const uint64_t msg = 3141592653589793238;

// Code for the attack victim if not doing a side channel
/*
uint64_t sqr(uint64_t r){
	r = r * r ;
	return r;
}

uint64_t mod(uint64_t r, uint64_t m){
	r = r % m;
	return r;
}

uint64_t mul(uint64_t r, uint64_t b){
	r = r * b;
	return r;
}


// TODO CONFIRM THAT THIS IMPLEMENTATION OF SQUARE MULT WORKS
uint64_t square_multiply(uint64_t b, uint64_t m, uint64_t e, int n) {
	uint64_t r = 1; 
	for (int i = n - 1; i >= 0 ; i --){
		r = sqr(r);
		r = mod(r, m);
		if (e % 2 != 0){
			r = mul(r, b);
			r = mod(r, m);
		}
		e /= 2;
	}
	return r;
*/


// helper function for printing the results
void print_bin(uint64_t val){
	int num_bits = sizeof(uint64_t) * 8;
	for (int i = num_bits - 1; i >= 0; i--){
		printf("%lu", (val >> i) & 1);
	}
	printf("\n");
}

uint64_t count_mismatch(uint64_t xor_res){
	uint64_t count = 0;
	while (xor_res > 0){
		xor_res &=(xor_res-1);
		count ++;
	}
	return count;
}

// Busy wait until the timestamp counter passes a certain value
// There's no guarantee on how far we have gone passed the intended value
static inline void busy_wait_until(uint64_t until) {
    while (_rdtsc() < until) {
        __asm__ __volatile__("rep; nop" ::: "memory");
    }
}

uint64_t calibrate_latency(){
	// TODO IMPLEMENT CALIBRATING LATENCY
	uint64_t foo = 160;
	return foo;
}



void simple_side_channel_sender(uint64_t start_tsc, uint64_t msg_len, uint8_t **target_pages, uint64_t msg) {
	// Function pulled from the professor, for initial validation of attack
    uint64_t next_transmission = start_tsc + EPOCH_LENGTH / 3;
    bool transmit = false;
    for (uint64_t i = 0; i < msg_len + DUMMY_RUNS; i++) {
        busy_wait_until(next_transmission);
        next_transmission += EPOCH_LENGTH;

        // Sender transmits the information
		if (i >= DUMMY_RUNS) {
			transmit = msg & 1;
			msg = msg >> 1;
		}
        if (transmit){
        	_maccess(target_pages[(i - DUMMY_RUNS) % NUMSTREAMS]);
        }

        // Print the epoch information
        uint64_t now = _rdtsc();
        uint64_t epoch_id = (now - start_tsc) / EPOCH_LENGTH;
        printf("EPOCH: %" PRIu64 "; Transmitting: '%d' \n", epoch_id, transmit);
    }
}

void attack_helper(uint64_t start_tsc, uint64_t msg_len, uint8_t ***EVs_td, uint8_t cnt){
	// The helper threads continuously load pages in the EV so they are in shared, to control the replacement policy
	// stop the helper after a certain number of epochs
	uint64_t end_tsc = start_tsc + (EPOCH_LENGTH * (msg_len + 1 + DUMMY_RUNS));
	while (_rdtsc() < end_tsc){
        for (int ii = 0; ii < NUMSTREAMS; ii++) {
            for (int jj = 0; jj < cnt; jj++){
			    _maccess(EVs_td[ii][jj]);
            }
		}
	}
}

uint64_t attacker_side_channel(uint64_t start_tsc, uint64_t msg_len, uint8_t ***EVs_l2, uint8_t cnt, uint64_t threshold, uint8_t **target_pages){
	// setup handling incoming data
	uint64_t recv_int = 0;
	uint64_t recv_mask = 1;

	// setup the timers for the side channel
	uint64_t next_evict = start_tsc;
	uint64_t next_reload = start_tsc + 2 * EPOCH_LENGTH / 3;

	// Synchronize start of process
	busy_wait_until(start_tsc);

	// Setup program timing
	struct timespec start_stamp, end_stamp;
	clock_gettime(CLOCK_MONOTONIC_RAW, &start_stamp);

	//main receiver loop
	for (uint64_t i = 0; i < msg_len + DUMMY_RUNS; i++){
		// evict
		// TODO THE PAPER MENTIONS ACCESSING THESE 6 TIMES, WHY?
		// busy_wait_until(next_evict);
		// next_evict += EPOCH_LENGTH;
		// for (int j = 0; j < WL2; j ++){
		//	_maccess(EVl2_mul[j]);
		// }
		// TODO UNCOMMENT TRYING EXPERIMENT
		/*
		for (int j = 0; j < 6; j++){
			for (int m = 0; m < cnt; m++){
				_maccess(EVl2_mul[m]);
			}
		}
			*/
		// _lfence();
		 // tconf->traverse(cands, cnt, tconf);

		// wait for side channel transmission 
		busy_wait_until(next_reload);
		next_reload += EPOCH_LENGTH;

		// reload
		uint64_t start = _timer_start();
		_maccess(target_pages[(i - DUMMY_RUNS) % NUMSTREAMS]);
		uint64_t lat = _timer_end() - start;
		//printf("Receive Access Time: %" PRIu64 " \n", lat);
		bool received_val = (lat > threshold);
		

		// Print the results
		uint64_t now = _rdtsc();
		uint64_t epoch_id = (now - start_tsc) / EPOCH_LENGTH;
		printf("EPOCH: %" PRIu64 "; Receiving: '%d', latency: %lu \n", epoch_id, received_val, lat);

		// save results. Use mask to help
		if (i >= DUMMY_RUNS) {
			if (epoch_id < msg_len + DUMMY_RUNS){
				if (received_val){
					recv_int = recv_int | recv_mask;
				}
				recv_mask = recv_mask << 1;
			}
		}

	}

	clock_gettime(CLOCK_MONOTONIC_RAW, &end_stamp);
	uint64_t delta_ns = (end_stamp.tv_sec - start_stamp.tv_sec) * 1000000000 + (end_stamp.tv_nsec - start_stamp.tv_nsec);
	// uint64_t attack_end_time = _rdtsc();
	// uint64_t total_attack_time = attack_end_time - start_tsc;
	
	// print overall results
	printf("-----------------------------------------\n");
    printf("Expecting: ");
    print_bin(msg);
    printf("Received:  ");
    print_bin(recv_int);

	uint64_t difference = msg ^ recv_int;
	print_bin(difference);
	uint64_t num_mismatch = count_mismatch(difference);
	printf("mismatching bits: %lu\n", num_mismatch);
	uint64_t false_positives = ~msg & recv_int;
	uint64_t num_fp = count_mismatch(false_positives);
	printf("False Positives: %lu\n", num_fp);
	uint64_t false_negatives = msg & ~recv_int;
	uint64_t num_fn = count_mismatch(false_negatives);
	printf("False Negatives: %lu\n", num_fn);

	// Timing 
	printf("total attack time = %luns\n", delta_ns);
	uint64_t bandwidth = sizeof(msg) * 1000000000/ delta_ns;
	printf("bandwidth = %lu B/s\n", bandwidth);

	return num_mismatch;

}

int test_ev_set(uint8_t *target, uint8_t **EVl2_mul, uint8_t cnt_l2, uint8_t **EVtd, uint8_t cnt_td){
	//printf("Testing eviction\n");
	uint64_t avg_nonEvictLat = 0;
	uint64_t avg_evictLat = 0;
	for (int i =0; i < 5 ; i++){
		_maccess(target);
		uint64_t start = _timer_start();
		_maccess(target);
		uint64_t lat = _timer_end() - start;
		//printf("Non evict latency: %lu\n", lat);
		avg_nonEvictLat += lat;

		for (int j = 0 ; j < 6; j++){
			for (int m = 0; m < cnt_l2; m++){
				_maccess(EVl2_mul[m]);
			}
		}
		start = _timer_start();
		_maccess(target);
		lat = _timer_end() - start;
		//printf("L2 evict latency: %lu\n", lat);

		for (int j = 0 ; j < 6; j++){
			for (int m = 0; m < cnt_l2; m++){
				_maccess(EVl2_mul[m]);
			}
		}
		for (int j = 0 ; j < 6; j++){
			for (int m = 0; m < cnt_td; m++){
				_maccess(EVtd[m]);
			}
		}
		start = _timer_start();
		_maccess(target);
		lat = _timer_end() - start;
		//printf("td evict latency: %lu\n", lat);
		avg_evictLat += lat;
	}
	printf("Non evict latency: %lu\n", avg_nonEvictLat);
	printf("L2 evict latency: %lu\n", avg_evictLat);
	return (avg_evictLat > (1.5 * avg_nonEvictLat));
}

int main(){
	int ret = 0;
	// make the memory regions take up multiple pages, so they should not co locate
	uint64_t total_mismatch = 0;
	
	uint8_t **target_pages = malloc(NUMSTREAMS * sizeof(uint64_t *));

    for (int i = 0; i < NUMSTREAMS; i++) {
        // Allocate one page using mmap
        target_pages[i] = mmap(NULL, CACHE_PAGE_SIZE * NUM_PAGE_PER_ALLOC, PROT_READ, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
		

        if (target_pages[i] == MAP_FAILED) {
            perror("mmap");
            // Cleanup already allocated pages
            for (int j = 0; j < i; j++) {
                munmap(target_pages[j], CACHE_PAGE_SIZE * NUM_PAGE_PER_ALLOC);
            }
            free(target_pages);
            return EXIT_FAILURE;
        }
    }

	uint8_t *page_mul = mmap(NULL, NUMSTREAMS * TARGET_PAGE_SIZE, PROT_READ, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	uint8_t *page_sqr = mmap(NULL, NUMSTREAMS * TARGET_PAGE_SIZE, PROT_READ, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

	if (cache_env_init(1)) {
		_error("Failed to initialize cache env!\n");
		return EXIT_FAILURE;
	}

	cache_oracle_init();
	
	// allcoate pointers for the eviction sets
	// EVSet *EVtd = NULL;
	// EVSet *EVl2_mul = NULL;


	// TODO NEED TO CALL evcands_new to generate canditates in the main function, so dont have pauge fault issues
	// ACTUALLY, CAN PASS IN AN EVBUFFER THERe
	// evbuffer_new
	// allocate the ev buffer
	// TODO FREE THIS MEMORY
	EVCandsConfig temp_evcands_config = {.scaling = 3, .filter_ev = NULL};
	EVBuffer *evb_td = evbuffer_new(detected_l3, &temp_evcands_config);
	EVCands *evcand_l2 = evcands_new(detected_l2, &((&def_l2_ev_config)->cands_config), NULL);

	uint8_t **EVtd = calloc(WTD + 1, sizeof(uint8_t *));
	uint8_t **EVl2_mul = calloc(WL2 + 1, sizeof(uint8_t *));
	uint8_t cnt_td = WTD;
	uint8_t cnt_l2 = WL2;

	// calibrate the latency
	uint64_t threshold = calibrate_latency();

    uint8_t ***EVs_td = malloc(NUMSTREAMS * sizeof(uint8_t *));
	uint8_t ***EVs_l2 = malloc(NUMSTREAMS * sizeof(uint8_t *));
    if (EVs_td == NULL) {
        perror("Failed to allocate outer array");
        return EXIT_FAILURE;
    }

    // Allocate each inner array
    for (int i = 0; i < NUMSTREAMS; i++) {
        EVs_td[i] = malloc(WTD + 1 * sizeof(int));
		EVs_l2[i] = malloc(WL2 + 1 * sizeof(int));
        if (EVs_td[i] == NULL) {
            perror("Failed to allocate inner array");

            // Cleanup allocations done so far
            for (int j = 0; j < i; j++) {
                free(EVs_td[j]);
            }
            free(EVs_td);
            return EXIT_FAILURE;
        }
		if (EVs_l2[i] == NULL) {
            perror("Failed to allocate inner array");

            // Cleanup allocations done so far
            for (int j = 0; j < i; j++) {
                free(EVs_l2[j]);
            }
            free(EVs_l2);
            return EXIT_FAILURE;
        }
		evcands_populate(page_offset(target_pages[i]), evcand_l2, &((&def_l2_ev_config)->cands_config));
		single_llc_evset(target_pages[i], EVs_l2[i], &cnt_l2, EVs_td[i], &cnt_td, evb_td, evcand_l2);
		test_ev_set(target_pages[i], EVs_l2[i], cnt_l2, EVs_td[i], cnt_td);
    }

	page_mul = target_pages[0];
	EVl2_mul = EVs_l2[0];
	EVtd = EVs_td[0];

	// generate the eviction sets
	//evcands_populate(page_offset(page_mul), evcand_l2, &((&def_l2_ev_config)->cands_config));
	//single_llc_evset(page_mul, EVl2_mul, &cnt_l2, EVtd, &cnt_td, evb_td, evcand_l2);

	printf("cnt_td: %d\n", cnt_td);
	printf("cnt_l2: %d\n", cnt_l2);

	//_clflush(page_mul);
	//flush_array(EVl2_mul, cnt_l2);
	//flush_array(EVtd, cnt_td);

	for (int ii = 0; ii < NUMSTREAMS; ii++) {
		_clflush(target_pages[ii]);
		flush_array(EVs_l2[ii], cnt_l2);
		flush_array(EVs_td[ii], cnt_td);
	}
	
	// Test for unique sets
	char grid[NUMSTREAMS][NUMSTREAMS+1];
	for (int ii = 0; ii < NUMSTREAMS; ii++) {
		for (int jj = 0; jj < NUMSTREAMS; jj++) {
			printf("Testing : Page %d : EV Set %d\n", ii, jj);
			int test_ev = test_ev_set(target_pages[ii], EVs_l2[jj], cnt_l2, EVs_td[jj], cnt_td);
			printf("Result : %d\n", test_ev);
			if (ii == jj) {
				if (test_ev != 1) {
					test_ev = test_ev_set(target_pages[ii], EVs_l2[jj], cnt_l2, EVs_td[jj], cnt_td);
					printf("MISMATCH: Target Page (%d) - EV Set (%d)\n", ii, jj);	
					grid[ii][jj] = 'X';
				} else {
					grid[ii][jj] = 'o';
				}
			} else {
				if (test_ev == 1) {
					test_ev = test_ev_set(target_pages[ii], EVs_l2[jj], cnt_l2, EVs_td[jj], cnt_td);
					printf("SET OVERLAP: Target Page (%d) - EV Set (%d)\n", ii, jj);
					grid[ii][jj] = 'X';
				} else {
					grid[ii][jj] = 'o';
				}	
			}
		}
		grid[ii][10] = '\0';
	}

	for (int i = 0; i < NUMSTREAMS; i++) {
        printf("%s\n", grid[i]);
    }


	_lfence();
	//test_ev_set(page_mul, EVl2_mul, cnt_l2, EVtd, cnt_td);


	for (int i = 0; i < NUM_REPEATS; i ++ ){
		_lfence();
		// sizeof returns number of bytes in msg, so multiply by 8 to get the total number of bits
		uint64_t msg_len = sizeof(uint64_t) * 8;
		uint64_t start_tsc = (_rdtsc() / EPOCH_LENGTH + 10) * EPOCH_LENGTH + N_EPOCH_START_DELAY * EPOCH_LENGTH;
		// attack_helper(start_tsc, msg_len, EVtd);
		// attack_helper(start_tsc, msg_len, EVtd, cnt_td);
		// printf("Done with helper\n");
		// attacker_side_channel(start_tsc, msg_len, EVl2_mul, cnt_l2, threshold, page_mul);
		// printf("done with attacker\n");
		
		pid_t pid = fork();
		if (pid == 0){
			// Victim/ sender
			simple_side_channel_sender(start_tsc, msg_len, target_pages, msg);

		}
		else if (pid > 0){
			pid_t pid2 = fork();
			if (pid2 == 0){
				// helper 0 
				printf("Helper 0\n");
				attack_helper(start_tsc, msg_len, EVs_td, cnt_td);
				printf("Helper 0 done\n");
			}
			else if (pid2 > 0) {
				pid_t pid3 = fork();
				if (pid3 == 0){
					// helper 1
					printf("Helper 1\n");
					attack_helper(start_tsc, msg_len, EVs_td, cnt_td);
					printf("Helper 1 done\n");
				}
				else if (pid3 > 0){
					// primary process is attacker
					printf("Attacker\n");
					total_mismatch += attacker_side_channel(start_tsc, msg_len, EVs_l2, cnt_l2, threshold, target_pages);
					wait(NULL); //wait for child process
				}
				else{
					perror("fork");
					ret = errno;
					goto cleanup;
				}	
				wait(NULL); //wait for child process
			}
			else{
				perror("fork");
				ret = errno;
				goto cleanup;
			}
			wait(NULL); //wait for child process
		}
		else{
			perror("fork");
			ret = errno;
			goto cleanup;
		}
	}
	cleanup:
		for (int i = 0; i < NUMSTREAMS; i++) {
			if (munmap(target_pages[i], TARGET_PAGE_SIZE) == -1) {
				perror("munmap");
			}
		}

		free(EVs_td);
		free(EVs_l2);

		free(target_pages);

		munmap(page_sqr, CACHE_PAGE_SIZE * NUM_PAGE_PER_ALLOC);
		munmap(page_mul, CACHE_PAGE_SIZE * NUM_PAGE_PER_ALLOC);


		
	return ret;
}

