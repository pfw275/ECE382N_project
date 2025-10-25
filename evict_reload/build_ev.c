// from the professor's code
#include "build_ev.h"

#include "core.h"
#include "cache/cache.h"
#include "sync.h"
#include <math.h>
#include <getopt.h>
#include "osc-common.h"

static evset_algorithm evalgo = EVSET_ALGO_DEFAULT;
static double cands_scaling = 3;
static size_t extra_cong = 1;
static size_t max_tries = 10, max_backtrack = 20, max_timeout = 0;
static bool l2_filter = true, single_thread = false, has_hugepage = false;
static helper_thread_ctrl hctrl;


int single_llc_evset(uint8_t *target, EVSet *l2_evset, EVSet *sf_evset) {
    // EVSet *l2_evset = NULL;
    u64 filter_duration = 0, start = 0, end = 0;

    start = time_ns();
    for (u32 r = 0; r < 10; r++) {
        l2_evset = build_l2_EVSet(target, &def_l2_ev_config, NULL);
        if (l2_evset && l2_evset->size == detected_l2->n_ways &&
            generic_evset_test(target, l2_evset) == EV_POS) {
            break;
        }
    }
    end = time_ns();
    filter_duration += end - start;

    if (!l2_evset) {
        _error("Failed to build an L2 evset\n");
        return EXIT_FAILURE;
    }

    if (has_hugepage) {
        cache_use_hugepage();
    }

    EVBuildConfig sf_config;
    default_skx_sf_evset_build_config(&sf_config, NULL, l2_evset, &hctrl);
    sf_config.algorithm = evalgo;
    sf_config.cands_config.scaling = cands_scaling;
    sf_config.algo_config.verify_retry = max_tries;
    sf_config.algo_config.max_backtrack = max_backtrack;
    sf_config.algo_config.retry_timeout = max_timeout;
    sf_config.algo_config.ret_partial = true;
    sf_config.algo_config.extra_cong = extra_cong;

    if (!l2_filter || has_hugepage) {
        sf_config.cands_config.filter_ev = NULL;
    }

    EVCands *cands = evcands_new(detected_l3, &sf_config.cands_config, NULL);
    if (!cands) {
        _error("Failed to allocate evcands\n");
        return EXIT_FAILURE;
    }

    start = time_ns();
    if (evcands_populate(page_offset(target), cands, &sf_config.cands_config)) {
        _error("Failed to populate evcands\n");
        return EXIT_FAILURE;
    }
    end = time_ns();
    filter_duration += end - start;
    if (l2_filter) {
        _info("L2 Filter Duration: %luus\n", filter_duration / 1000);
    }

    if (single_thread) {
        sf_config.test_config.traverse = skx_sf_cands_traverse_st;
        sf_config.test_config.need_helper = false;
    } else {
        start_helper_thread(sf_config.test_config.hctrl);
    }

    if (generic_test_eviction(target, cands->cands, cands->size,
                              &sf_config.test_config) != EV_POS) {
        _error("Not enough candidates due to filtering!\n");
        return EXIT_FAILURE;
    }

    reset_evset_stats();
    start = time_ns();
    sf_evset = build_skx_sf_EVSet(target, &sf_config, cands);
    end = time_ns();
    if (!sf_evset) {
        _error("Failed to build evset\n");
        pprint_evset_stats();
        return EXIT_FAILURE;
    }
    pprint_evset_stats();
    _info("Duration: %.3fms; Size: %u; Candidates: %lu\n", (end - start) / 1e6,
          sf_evset->size, sf_evset->cands->size);
    _info("LLC EV Test Level: %d\n", precise_evset_test(target, sf_evset));

    if (sf_evset->size > SF_ASSOC) {
        // truncate the evset to a minimal evset for SF test
        sf_evset->size = SF_ASSOC;
    }
    sf_config.test_config_alt.foreign_evictor = true;
    _info("SF EV Test Level: %d\n", precise_evset_test_alt(target, sf_evset));

    if (!single_thread) {
        stop_helper_thread(sf_config.test_config.hctrl);
    }

    if (cache_oracle_inited()) {
        u64 target_hash = llc_addr_hash(target), match = 0;
        printf("Target: %p; hash=%#lx\n", target, target_hash);
        for (u32 i = 0; i < sf_evset->size; i++) {
            u64 hash = llc_addr_hash(sf_evset->addrs[i]);
            printf("%2u: %p (hash=%#lx)\n", i, sf_evset->addrs[i], hash);
            match += hash == target_hash;
        }
        printf("Match: %lu\n", match);
    }

    return EXIT_SUCCESS;
}