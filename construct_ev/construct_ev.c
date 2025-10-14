#include "../inline_asm.h"
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <x86intrin.h>
#include <time.h>

#define LLC_MISS_THRESHOLD 200  // adjust experimentally

// -----------------------------
// Low-level timing primitives
// -----------------------------
static inline unsigned measure_access_time(uintptr_t addr) {
    unsigned aux;
    unsigned long start = __rdtscp(&aux);
    *(volatile unsigned char *)addr;
    unsigned long end = __rdtscp(&aux);
    return (unsigned)(end - start);
}

static inline void access_address(uintptr_t addr) {
    *(volatile unsigned char *)addr;
}

// -----------------------------
// Algorithm 2 (baseline)
// -----------------------------
// check_conflict_baseline(x, U):
//     access x
//     for each addr in U: access addr
//     t = measure time of accessing x
//     return (t >= LLC_miss_threshold)
bool check_conflict_baseline(uintptr_t x, uintptr_t *U, size_t U_size) {
    access_address(x);

    for (size_t i = 0; i < U_size; i++)
        access_address(U[i]);

    unsigned t = measure_access_time(x);
    return (t >= LLC_MISS_THRESHOLD);
}

// -----------------------------
// Algorithm 3 (non-inclusive)
// -----------------------------
// check_conflict_noninclusive(x, U, L2_occupy_set):
//     access x
//     for each addr in U: access addr
//     for each addr in L2_occupy_set: access addr   // flush U from L2 to LLC
//     t = measure time of accessing x
//     return (t >= LLC_miss_threshold)
bool check_conflict_noninclusive(uintptr_t x,
                                 uintptr_t *U, size_t U_size,
                                 uintptr_t *L2_occupy_set, size_t L2_size)
{
    access_address(x);

    // access all addresses in U
    for (size_t i = 0; i < U_size; i++)
        access_address(U[i]);

    // evict U from L2 by accessing L2_occupy_set
    for (size_t i = 0; i < L2_size; i++)
        access_address(L2_occupy_set[i]);

    unsigned t = measure_access_time(x);
    return (t >= LLC_MISS_THRESHOLD);
}

// -----------------------------
// find_EV()  (Appendix A)
// -----------------------------
size_t find_EV(uintptr_t *CS, size_t cs_size,
               uintptr_t *EV, size_t max_ev_size,
               bool use_noninclusive,
               uintptr_t *L2_occupy_set, size_t L2_size)
{
    if (cs_size == 0) return 0;

    srand((unsigned)time(NULL));
    uintptr_t test_addr = CS[rand() % cs_size];

    // make CS' = CS − test_addr
    uintptr_t *CS_prime = malloc((cs_size - 1) * sizeof(uintptr_t));
    size_t cs_prime_size = 0;
    for (size_t i = 0; i < cs_size; i++)
        if (CS[i] != test_addr)
            CS_prime[cs_prime_size++] = CS[i];

    bool conflict = use_noninclusive
        ? check_conflict_noninclusive(test_addr, CS_prime, cs_prime_size,
                                      L2_occupy_set, L2_size)
        : check_conflict_baseline(test_addr, CS_prime, cs_prime_size);

    if (!conflict) {
        free(CS_prime);
        return 0; // fail
    }

    size_t ev_size = 0;

    // --- main loop: identify contributing addresses ---
    for (size_t i = 0; i < cs_prime_size; i++) {
        uintptr_t addr = CS_prime[i];

        uintptr_t *tmp = malloc((cs_prime_size - 1) * sizeof(uintptr_t));
        size_t tmp_size = 0;
        for (size_t j = 0; j < cs_prime_size; j++)
            if (j != i) tmp[tmp_size++] = CS_prime[j];

        bool disappears = use_noninclusive
            ? !check_conflict_noninclusive(test_addr, tmp, tmp_size,
                                           L2_occupy_set, L2_size)
            : !check_conflict_baseline(test_addr, tmp, tmp_size);

        if (disappears) {
            if (ev_size < max_ev_size)
                EV[ev_size++] = addr;
        } else {
            // remove permanently
            for (size_t j = i; j + 1 < cs_prime_size; j++)
                CS_prime[j] = CS_prime[j + 1];
            cs_prime_size--;
            i--;
        }
        free(tmp);
    }

    // --- recheck phase ---
    for (size_t i = 0; i < cs_size; i++) {
        bool still_conflict = use_noninclusive
            ? check_conflict_noninclusive(test_addr, EV, ev_size,
                                          L2_occupy_set, L2_size)
            : check_conflict_baseline(test_addr, EV, ev_size);
        if (still_conflict && ev_size < max_ev_size)
            EV[ev_size++] = CS[i];
    }

    free(CS_prime);
    return ev_size;
}

// -----------------------------
// Example stub main()
// -----------------------------
int main(void) {
    const size_t N = 32;
    uintptr_t *candidate_set = malloc(N * sizeof(uintptr_t));
    for (size_t i = 0; i < N; i++)
        candidate_set[i] = (uintptr_t)aligned_alloc(64, 64);

    // Example L2 occupy set (in real experiments, these must map to same L2 set)
    uintptr_t L2_occupy[16];
    for (int i = 0; i < 16; i++)
        L2_occupy[i] = (uintptr_t)aligned_alloc(64, 64);

    uintptr_t EV[64];
    size_t ev_size = find_EV(candidate_set, N, EV, 64,
                             true,             // use_noninclusive version
                             L2_occupy, 16);   // pass L2 occupy set

    printf("Eviction set size: %zu\n", ev_size);

    for (size_t i = 0; i < N; i++) free((void *)candidate_set[i]);
    for (int i = 0; i < 16; i++) free((void *)L2_occupy[i]);
    free(candidate_set);
    return 0;
}
