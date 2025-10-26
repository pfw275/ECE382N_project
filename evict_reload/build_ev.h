#ifndef EV_CREATION_FUNCS
#define EV_CREATION_FUNCS
#include "core.h"
#include "cache/cache.h"
// #include "sync.h"
#include <math.h>
#include <getopt.h>
// #include "osc-common.h"

//int single_llc_evset(uint8_t *target, EVSet *l2_evset, EVSet *sf_evset, EVBuildConfig sf_config);
int single_llc_evset(uint8_t *target, uint8_t **EVl2_mul, uint8_t *cnt_l2, uint8_t **EVtd, uint8_t *cnt_td, EVBuffer *evb_td, EVCands *evcand_l2);

#endif // EV_CREATION_FUNCS