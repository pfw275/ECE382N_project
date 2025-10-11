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


#define WTD 11
#define WL2 16


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


void simple_side_channel_sender(uint64_t start_tsc, size_t msg_len, uint8_t *pages) {
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
        printf("EPOCH: %" PRIu64 "; Transmitting: '%c' (ASCII=%#x)\n", epoch_id,
               msg[i], msg[i]);
    }
}

void attack_helper(){
	// TODO IMPLEMENT HELPER THAT KEEPS Wtd lines in shared
}

void attacker(){
	// TODO IMPLEMENT THE ATTACKER
}

int main(){
	// TODO MUST SETUP THE MEMORY THAT WILL BE USED


	pid_t pid = fork();
	if (pid == 0){
		// TODO VICTIM
	}
	else if (pid == 1){
		// TODO ATTACKER
	}
	else if (pid > 1){
		// TODO HELPER
	}
	else{
		perror("fork");
		ret = errorno;
		goto cleanup;
	}

	cleanup:
		// TODO HANDLE UNMAPPING MEMORY
}

