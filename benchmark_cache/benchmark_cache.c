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
#include <time.h>

#define N_TRIALS 1000000
#define MAX_SIZE_MB 64
#define STEP_KB 64
#define CACHELINE_SIZE 64
// Cache sizes in cache lines
// L1 Cache Dimensions
#define L1_SIZE 500  // same for data/instruction
#define L1_SETS 64
#define L1_WAYS 8
// L2 Cache Dimensions
#define L2_SIZE 16000
#define L2_SETS 64
#define L2_WAYS 8
// L3 Cache Dimensions
#define L3_SIZE 352000
#define L3_SETS 64
#define L3_WAYS 8


uint64_t calibrate_l1_latency() {
    printf("Calibrate L1 Cache Latency\n");
    uint64_t hit = 0, rep = 1000;
    uint8_t *data = malloc(8);
    assert(data); // Lazy "exception" handling

    // Measure l1 cache hit latency
    _maccess(data);
    for (uint32_t n = 0; n < rep; n++) {
        uint64_t start = _timer_start();
        _maccess(data);
        hit += _timer_end() - start;
        //printf("Hit: %" PRIu64 "\n", hit);
    }
    hit /= rep;

    free(data);
    return hit;
}

uint64_t calibrate_l2_latency() {
    printf("Calibrate L2 Cache Latency\n");
    uint64_t hit = 0, rep = 1000;
    uint8_t *data = malloc(8);
    assert(data); // Lazy "exception" handling
    int N = 1000;
    uint8_t *eviction_set = malloc(sizeof(uint8_t) * L1_SETS * L1_WAYS * CACHELINE_SIZE * N);
    assert(eviction_set); // Lazy "exception" handling

    // Measure l1 cache hit latency
    for (uint32_t n = 0; n < rep; n++) {
        _maccess(data);
        for (int ii = 0; ii < (L1_SETS * L1_WAYS * N); ii++) {
            //printf("Eviction Set Addr: %p\n", (void*)&eviction_set[ii * sizeof(uint8_t) * CACHELINE_SIZE]);
            _maccess(&eviction_set[ii * sizeof(uint8_t) * CACHELINE_SIZE]);
        }
        uint64_t start = _timer_start();
        _maccess(data);
        hit += _timer_end() - start;
        //printf("Hit: %" PRIu64 "\n", hit);
    }
    hit /= rep;

    free(data);
    free(eviction_set);
    return hit;
}

uint64_t calibrate_l3_latency() {
    printf("Calibrate L3 Cache Latency\n");
    uint64_t hit = 0, rep = 1000;
    uint8_t *data = malloc(8);
    assert(data); // Lazy "exception" handling
    int N = 50000;
    uint8_t *eviction_set = malloc(sizeof(uint8_t) * L2_SETS * L2_WAYS * CACHELINE_SIZE * N);
    assert(eviction_set); // Lazy "exception" handling

    // Measure l1 cache hit latency
    for (uint32_t n = 0; n < rep; n++) {
        _maccess(data);
        for (int ii = 0; ii < (L2_SETS * L2_WAYS * N); ii++) {
            //printf("Eviction Set Addr: %p\n", (void*)&eviction_set[ii * sizeof(uint8_t) * CACHELINE_SIZE]);
            _maccess(&eviction_set[ii * sizeof(uint8_t) * CACHELINE_SIZE]);
        }
        uint64_t start = _timer_start();
        _maccess(data);
        hit += _timer_end() - start;
        //printf("Hit: %" PRIu64 "\n", hit);
    }
    hit /= rep;

    free(data);
    free(eviction_set);
    return hit;
}


uint64_t calibrate_mem_latency() {
    printf("Calibrate Mem Cache Latency\n");
    uint64_t miss = 0, rep = 1000;
    uint8_t *data = malloc(8);
    assert(data); // Lazy "exception" handling

   // Measure memory cache latency
    for (uint32_t n = 0; n < rep; n++) {
        _mm_clflush(data);
        uint64_t start = _timer_start();
        _maccess(data);
        miss += _timer_end() - start;
        //printf("Hit: %" PRIu64 "\n", miss);
    }
    miss /= rep;

    free(data);
    return miss;
}

/*
// Measure access latency for random accesses within an array of given size
double measure_latency(size_t array_size_bytes) {
    size_t n_elements = array_size_bytes / sizeof(uint64_t);
    uint64_t *arr = aligned_alloc(CACHELINE_SIZE, array_size_bytes);
    if (!arr) {
        perror("aligned_alloc");
        exit(1);
    }

    // Initialize array with random access pattern
    for (size_t i = 0; i < n_elements; i++) {
        arr[i] = rand() % n_elements;
    }

    uint64_t start, end;
    uint64_t idx = 0;
    uint64_t total_cycles = 0;

    // Access repeatedly to measure timing
    for (size_t t = 0; t < N_TRIALS; t++) {
        start = __rdtsc();
        idx = arr[idx];
        end = __rdtsc();
        total_cycles += (end - start);
    }

    free(arr);
    return (double)total_cycles / N_TRIALS;
}
*/

int main() {
    printf("Cache Access Latency Benchmark\n");
    printf("-----------------------------------\n");
    printf("Array Size (KB)\tAvg Latency (cycles)\n");

    srand(time(NULL));

    //for (size_t size_kb = 4; size_kb <= MAX_SIZE_MB * 1024; size_kb += STEP_KB) {
    //    size_t bytes = size_kb * 1024;
    //    double latency = measure_latency(bytes);
    //    printf("%zu\t\t%.2f\n", size_kb, latency);
    //    fflush(stdout);
    // }
   
    uint64_t l1_time = 0;
    uint64_t l2_time = 0;
    uint64_t l3_time = 0;
    uint64_t mem_time = 0;

    l1_time = calibrate_l1_latency();
    l2_time = calibrate_l2_latency();
    l3_time = calibrate_l3_latency();
    mem_time = calibrate_mem_latency();

    printf("Calibration Results:\n");
    printf("L1  : %" PRIu64 ",\n", l1_time);
    printf("L2  : %" PRIu64 ",\n", l2_time);
    printf("L3  : %" PRIu64 ",\n", l3_time);
    printf("Mem : %" PRIu64 ",\n", mem_time);

    return 0;
}
