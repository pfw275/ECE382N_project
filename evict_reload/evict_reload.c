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
#define SYMBOL_CNT (1 << (sizeof(char) * 8))

#define EPOCH_LENGTH 2000000. // A very conservative epoch length

#define WTD 11
#define WL2 16

// Message we want to send
const char *msg = "Testing Testing 123";


// TODO CONFIRM THAT THIS IMPLEMENTATION OF SQUARE MULT WORKS
/*
uint64_t square_multiply(uint64_t b, uint64_t m, uint64_t e, int n) {
	uint64_t r = 1; 
	for (int i = n - 1; i > 0 ; i --){
		r = r * r;
		r = r % m ;
		if (e % 2 != 0){
			r = r * b;
			r = r % m;
		}
		e /= 2;
	}
	return r;
*/


// Busy wait until the timestamp counter passes a certain value
// There's no guarantee on how far we have gone passed the intended value
static inline void busy_wait_until(uint64_t until) {
    while (_rdtsc() < until) {
        __asm__ __volatile__("rep; nop" ::: "memory");
    }
}

void simple_side_channel_sender(uint64_t start_tsc, size_t msg_len, uint8_t *pages) {
	// TODO NEED TO REFORMAT SO IT IS MORE STRUCTURED LIKE SQUARE MULT, IN THAT HAVE SECRET DEPENDENT MEMORY ACCESSES
	// IE IF SECRET THEN ACCESS SOMETHING. ELSE ACCESS OTHER
	// Function pulled from the professor, for initial validation of attack
    uint64_t next_transmission = start_tsc + EPOCH_LENGTH / 3;
    for (size_t i = 0; i < msg_len; i++) {
        busy_wait_until(next_transmission);
        next_transmission += EPOCH_LENGTH;

        // Sender transmits the information
        _maccess(pages + (unsigned char)msg[i] * PAGE_SIZE);

        // Print the epoch information
        uint64_t now = _rdtsc();
        uint64_t epoch_id = (now - start_tsc) / EPOCH_LENGTH;
        printf("EPOCH: %" PRIu64 "; Transmitting: '%c' (ASCII=%#x)\n", epoch_id, msg[i], msg[i]);
    }
}

void attack_helper(uint64_t start_tsc, size_t msg_len, uint8_t **EVtd){
	// The helper threads continuously load pages in the EV so they are in shared, to control the replacement policy

	// stop the helper after a certain number of epochs
	// TODO NOT SURE IF NEED EXTRA, BUT ADDING + 1 JUST IN CASE
	uint64_t end_tsc = start_tsc + (EPOCH_LENGTH * (msg_len + 1));
	while (_rdtsc() < end_tsc){
		for (int i = 0; i < WTD; i++){
			// repeatedly access pages in the eviction set for the directory
			_maccess(EVtd[i])
		}
	}
}

void attacker(){
	// TODO IMPLEMENT THE ATTACKER
}

int main(){
	// TODO MUST SETUP THE MEMORY THAT WILL BE USED


	size_t msg_len = strlen(msg);
    uint64_t start_tsc = (_rdtsc() / EPOCH_LENGTH + 10) * EPOCH_LENGTH;
	pid_t pid = fork();
	if (pid == 0){
		// Victim/ sender
		simple_side_channel_sender(start_tsc, msg_len, pages);
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
				// TODO ATTACKER
				attacker(start_tsc, msg_len, EVl2);
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
		// TODO HANDLE UNMAPPING MEMORY
}

