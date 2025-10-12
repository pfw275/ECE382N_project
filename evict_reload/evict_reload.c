#include "../inline_asm.h"
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
#include <x86intrin.h>

#define PAGE_SIZE 4096
#define CACHE_LINE_SIZE 64
#define NUM_PAGE_PER_ALLOC 5
#define SYMBOL_CNT (1 << (sizeof(char) * 8))

#define EPOCH_LENGTH 2000000. // A very conservative epoch length

#define WTD 11
#define WL2 16

// Message we want to send
const uint64_t msg = 0x12345678910;


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
	for (int i = n - 1; i > 0 ; i --){
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
	uint64_t print_mask = 1 << sizeof(uint64_t) * CHAR_BIT - 1;
	while (print_mask){
		bool print_val = 0;
		if (print_mask & val){
			print_val = 1;
		}
		printf("%d", print_val);
		print_mask >>= 1;
	}
	printf("\n")''
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
}


void generate_ev(uint8_t **EVtd, uint8_t **EVl2_mul, uint8_t *page_mul){
	// TODO GENERATE EVICTION SETS

	// TODO BE VERY CAREFUL ABOUT MEMORY HANDLING
}

void simple_side_channel_sender(uint64_t start_tsc, uint msg_len, uint8_t *page_sqr, uint8_t *page_mul, uint64_t e) {
	// Function pulled from the professor, for initial validation of attack
    uint64_t next_transmission = start_tsc + EPOCH_LENGTH / 3;
    bool transmit_value;
    for (uint i = 0; i < msg_len; i++) {
        busy_wait_until(next_transmission);
        next_transmission += EPOCH_LENGTH;

        // Sender transmits the information
        _maccess(page_sqr);
        transmit_value = (e % 2 != 0);
        if (transmit_value){
        	_maccess(page_mul);
        }
        e /= 2;


        // Print the epoch information
        uint64_t now = _rdtsc();
        uint64_t epoch_id = (now - start_tsc) / EPOCH_LENGTH;
        printf("EPOCH: %" PRIu64 "; Transmitting: '%d' \n", epoch_id, transmit_value);
    }
}

void attack_helper(uint64_t start_tsc, uint msg_len, uint8_t **EVtd){
	// The helper threads continuously load pages in the EV so they are in shared, to control the replacement policy

	// stop the helper after a certain number of epochs
	uint64_t end_tsc = start_tsc + (EPOCH_LENGTH * (msg_len + 1));
	while (_rdtsc() < end_tsc){
		for (int i = 0; i < WTD; i++){
			// repeatedly access pages in the eviction set for the directory
			_maccess(EVtd[i])
		}
	}
}

void attacker_side_channel(uint64_t start_tsc, uint msg_len, uint8_t **EVl2_mul, uint64_t threshold, uint8_t *page_mul){

	// setup handling incoming data
	uint64_t recv_int = 0;
	uint64_t recv_mask = 1;

	// setup the timers for the side channel
	uint64_t next_evict = start_tsc;
	uint64_t next_reload = start_tsc + 2 * EPOCH_LENGTH / 3;

	//main receiver loop
	for (uint i = 0; uint i < msg_len; i++){
		// evict
		busy_wait_until(next_evict);
		next_evict += EPOCH_LENGTH;
		for (int j = 0; j < WL2; j ++){
			_maccess(EVl2_mul[j]);
		}

		// wait for side channel transmission 
		busy_wait_until(next_reload);
		next_reload += EPOCH_LENGTH;

		// reload
		uint64_t start = _timer_start();
		_maccess(page_mul);
		uint64_t lat = _timer_end() - start;
		bool received_val = (lat < threshold);
		

		// Print the results
		uint64_t now = _rdtsc();
		uint64_t epoch_id = (now - start_tsc) / EPOCH_LENGTH;
		printf("EPOCH: %" PRIu64 "; Receiving: '%d' \n", epoch_id, received_val);

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

}

int main(){
	int ret = 0;
	// make the memory regions take up multiple pages, so they should not co locate
	uint8_t *page_sqr = mmap(NULL, PAGE_SIZE * NUM_PAGE_PER_ALLOC, PROT+READ, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	uint8_t *page_mul = mmap(NULL, PAGE_SIZE * NUM_PAGE_PER_ALLOC, PROT+READ, MAP_SHARED | MAP_ANONYMOUS, -1, 0);

	// allcoate pointers for the eviction sets
	uint8_t **EVtd = calloc(WTD + 1, sizeof(uint8_t *));
	uint8_t **EVl2_mul = calloc(WL2 + 1, sizeof(uint8_t *));

	// calibrate the latency
	uint64_t threshold = calibrate_latency();

	// generate the eviction sets
	generate_ev(EVtd, EVl2_mul, page_mul);


	// sizeof returns number of bytes in msg, so multiply by 8 to get the total number of bits
	uint msg_len = sizeof(uint64_t) * 8;
    uint64_t start_tsc = (_rdtsc() / EPOCH_LENGTH + 10) * EPOCH_LENGTH;
	pid_t pid = fork();
	if (pid == 0){
		// Victim/ sender
		simple_side_channel_sender(start_tsc, msg_len, page_sqr, page_mul, msg);
		goto cleanup;

	}
	else if (pid > 0){
		pid_t pid2 = fork();
		if (pid2 == 0){
			// helper 0 
			attack_helper(start_tsc, msg_len, EVtd);
		}
		else if (pid2 > 0) {
			pid_t pid3 = fork();
			if (pid3 == 0){
				// helper 1
				attack_helper(start_tsc, msg_len, EVtd);
			}
			else if (pid3 > 0){
				// primary process is attacker
				attacker_side_channel(start_tsc, msg_len, EVl2_mul, threshold, page_mul);
				wait(NULL); //wait for child process
			}
			else{
				perror("fork");
				ret = errorno;
				goto cleanup;
			}	
			wait(NULL); //wait for child process
		}
		else{
			perror("fork");
			ret = errorno;
			goto cleanup;
		}
		wait(NULL); //wait for child process
	}
	else{
		perror("fork");
		ret = errorno;
		goto cleanup;
	}

	cleanup:
		munmap(page_sqr, PAGE_SIZE * NUM_PAGE_PER_ALLOC);
		munmap(page_mul, PAGE_SIZE * NUM_PAGE_PER_ALLOC);

		// clean up the eviction sets, need to iterate through each
		for (int i =0; i < WTD + 1; i++){
			// TODO FREE EACH ITEM IN EVtd
		}
		for (int i =0; i < WL2 + 1; i++){
			// TODO FREE EACH ITEM IN EVl2_mul
		}
		free(EVtd);
		free(EVl2_mul);
		return ret;
}

