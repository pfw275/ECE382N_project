// call as below:
// taskset -c 0,2,4,6 ./force_exclusive 

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
// #include <x86intrin.h>
#include <stdbool.h>
// #include find_eviction_set.h
#include "cache/evset.h"
#include "build_ev.h"

#define CACHE_PAGE_SIZE 4096
#define CACHE_LINE_SIZE 64
#define NUM_PAGE_PER_ALLOC 5
#define SYMBOL_CNT (1 << (sizeof(char) * 8))

#define EPOCH_LENGTH 2000000. // A very conservative epoch length

#define WTD 11
#define WL2 16
#define NUM_REPEATS 1
#define ENABLE_HELPERS 0

// Message we want to send
// const uint64_t msg = 0x12345678910;
//const uint64_t msg = 3141592653589793238;
//uint64_t msg = ((uint64_t)rand() << 32) | rand(); 

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



void process_sender(uint64_t start_tsc, uint64_t msg_len, uint8_t *target_page, uint64_t msg) {
	// Function pulled from the professor, for initial validation of attack
    uint64_t next_transmission = start_tsc + EPOCH_LENGTH / 3;
    bool transmit;
    for (uint64_t i = 0; i < msg_len; i++) {
        busy_wait_until(next_transmission);
        next_transmission += EPOCH_LENGTH;

        // Sender transmits the information
        transmit = msg & 1;
        if (transmit){
        	_maccess(target_page);
        }
        msg = msg >> 1;


        // Print the epoch information
        uint64_t now = _rdtsc();
        uint64_t epoch_id = (now - start_tsc) / EPOCH_LENGTH;
        printf("EPOCH: %" PRIu64 "; Transmitting: '%d' \n", epoch_id, transmit);
    }
}

void process_receiver_helper(uint64_t start_tsc, uint64_t msg_len, uint8_t **EVtd, uint8_t cnt){
	// The helper threads continuously load pages in the EV so they are in shared, to control the replacement policy
	// stop the helper after a certain number of epochs
	if (ENABLE_HELPERS == 1) {
		uint64_t end_tsc = start_tsc + (EPOCH_LENGTH * (msg_len + 1));
		while (_rdtsc() < end_tsc){
			for (int i = 0; i < cnt; i++){
				_maccess(EVtd[i]);
			}
		}
	}
}

uint64_t process_receiver(uint64_t start_tsc, uint64_t msg_len, uint8_t **EVtd, uint8_t **EVl2_mul, uint8_t cnt_td, uint8_t cnt_l2, uint64_t threshold, uint8_t *target_page, uint64_t msg){

	// setup handling incoming data
	uint64_t recv_int = 0;
	uint64_t recv_mask = 1;

	// setup the timers for the side channel
	uint64_t next_evict = start_tsc;
	uint64_t next_reload = start_tsc + 2 * EPOCH_LENGTH / 3;
	//_maccess(target_page);
	//_lfence();
	
	// EVTestConfig *tconf = &EVl2_mul->config->test_config;
	// u8 **cands = EVl2_mul->addrs;

	//main receiver loop
	for (uint64_t i = 0; i < msg_len; i++){
		// evict
		// TODO THE PAPER MENTIONS ACCESSING THESE 6 TIMES, WHY?
		busy_wait_until(next_evict);
		next_evict += EPOCH_LENGTH;
		
		// Prime
		for (int ii = 0; ii < cnt_l2; ii ++){
			_maccess(EVl2_mul[ii]);
		}
		
		for (int jj = 0; jj < 6; jj++){
			for (int ii = 0; ii < cnt_td; ii++){
				_maccess(EVtd[ii]);
			}
		}
		
		_lfence();
		 // tconf->traverse(cands, cnt, tconf);

		// wait for side channel transmission 
		busy_wait_until(next_reload);
		next_reload += EPOCH_LENGTH;

		// Probe
		int ev_kicked_count = 0;
		for (int ii = 0; ii < cnt_l2; ii ++){
			uint64_t start = _timer_start();
			_maccess(EVl2_mul[ii]);
			uint64_t lat = _timer_end() - start;
		    if (lat > threshold) {
				ev_kicked_count++;
			}
		}
		
		for (int ii = 0; ii < cnt_td; ii++){
			uint64_t start = _timer_start();
			_maccess(EVtd[ii]);
			uint64_t lat = _timer_end() - start;
		    if (lat > threshold) {
				ev_kicked_count++;
			}
		}

		bool received_val = (ev_kicked_count > 1);

		//uint64_t start = _timer_start();
		//_maccess(target_page);
		//uint64_t lat = _timer_end() - start;
		//bool received_val = (lat > threshold);
		

		// Print the results
		uint64_t now = _rdtsc();
		uint64_t epoch_id = (now - start_tsc) / EPOCH_LENGTH;
		//printf("EPOCH: %" PRIu64 "; Receiving: '%d', latency: %lu, EV Kicked Count: %d \n", epoch_id, received_val, lat, ev_kicked_count);
		printf("EPOCH: %" PRIu64 "; Receiving: '%d', EV Kicked Count: %d \n", epoch_id, received_val, ev_kicked_count);

		// save results. Use mask to help
		if (epoch_id < msg_len){
			if (received_val){
				recv_int = recv_int | recv_mask;
			}
			recv_mask = recv_mask << 1;
		}

	}
	
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
	return num_mismatch;

}

void test_ev_set(uint8_t *target, uint8_t **EVl2_mul, uint8_t cnt_l2, uint8_t **EVtd, uint8_t cnt_td){
	printf("Testing eviction\n");

	for (int i =0; i < 5 ; i++){
		_maccess(target);
		uint64_t start = _timer_start();
		_maccess(target);
		uint64_t lat = _timer_end() - start;
		printf("Non evict latency: %lu\n", lat);

		for (int j = 0 ; j < 6; j++){
			for (int m = 0; m < cnt_l2; m++){
				_maccess(EVl2_mul[m]);
			}
		}
		start = _timer_start();
		_maccess(target);
		lat = _timer_end() - start;
		printf("L2 evict latency: %lu\n", lat);

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
		printf("td evict latency: %lu\n", lat);
	}

}

int main(){
	uint64_t msg = ((uint64_t)rand() << 32) | rand(); 

	int ret = 0;
	// make the memory regions take up multiple pages, so they should not co locate
	uint64_t total_mismatch = 0;
	
	uint8_t *target_page = mmap(NULL, CACHE_PAGE_SIZE * NUM_PAGE_PER_ALLOC, PROT_READ, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

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
	evcands_populate(page_offset(target_page), evcand_l2, &((&def_l2_ev_config)->cands_config));


	uint8_t **EVtd = calloc(WTD + 1, sizeof(uint8_t *));
	uint8_t **EVl2_mul = calloc(WL2 + 1, sizeof(uint8_t *));
	uint8_t cnt_td = WTD;
	uint8_t cnt_l2 = WL2;

	// calibrate the latency
	uint64_t threshold = calibrate_latency();

	// generate the eviction sets
	single_llc_evset(target_page, EVl2_mul, &cnt_l2, EVtd, &cnt_td, evb_td, evcand_l2);

	_clflush(target_page);
	flush_array(EVl2_mul, cnt_l2);
	flush_array(EVtd, cnt_td);
	_lfence();

	printf("cnt_td: %d\n", cnt_td);
	printf("cnt_l2: %d\n", cnt_l2);
	
	test_ev_set(target_page, EVl2_mul, cnt_l2, EVtd, cnt_td);

	for (int i = 0; i < NUM_REPEATS; i ++ ){
		_lfence();
		// sizeof returns number of bytes in msg, so multiply by 8 to get the total number of bits
		uint64_t msg_len = sizeof(uint64_t) * 8;
		uint64_t start_tsc = (_rdtsc() / EPOCH_LENGTH + 10) * EPOCH_LENGTH;
		// process_receiver_helper(start_tsc, msg_len, EVtd);
		// process_receiver_helper(start_tsc, msg_len, EVtd, cnt_td);
		// printf("Done with helper\n");
		// process_receiver(start_tsc, msg_len, EVl2_mul, cnt_l2, threshold, target_page);
		// printf("done with attacker\n");
		
		pid_t pid = fork();
		if (pid == 0) {
			// Victim/ sender
			printf("Sender\n");
			process_sender(start_tsc, msg_len, target_page, msg);
			printf("Sender Done\n");

		}
		else if (pid > 0) {
			pid_t pid2 = fork();
			if (pid2 == 0) {
				// helper 0 
				printf("Helper 0\n");
				process_receiver_helper(start_tsc, msg_len, EVtd, cnt_td);
				printf("Helper 0 done\n");
			}
			else if (pid2 > 0) {
				pid_t pid3 = fork();
				if (pid3 == 0){
					// helper 1
					printf("Helper 1\n");
					process_receiver_helper(start_tsc, msg_len, EVtd, cnt_td);
					printf("Helper 1 done\n");
				}
				else if (pid3 > 0) {
					// primary process is attacker
					printf("Attacker\n");
					total_mismatch += process_receiver(start_tsc, msg_len, EVtd, EVl2_mul, cnt_td, cnt_l2, threshold, target_page, msg);
					wait(NULL); //wait for child process
				}
				else {
					perror("fork");
					ret = errno;
					goto cleanup;
				}	
				wait(NULL); //wait for child process
			}
			else {
				perror("fork");
				ret = errno;
				goto cleanup;
			}
			wait(NULL); //wait for child process
		}
		else {
			perror("fork");
			ret = errno;
			goto cleanup;
		}
	}
	cleanup:
		munmap(target_page, CACHE_PAGE_SIZE * NUM_PAGE_PER_ALLOC);

	return ret;
}

