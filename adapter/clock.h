/* Guest-cycle conversion for host pacing. MIT, Brandon Gates. */
#ifndef AGON_FLEXE_CLOCK_H
#define AGON_FLEXE_CLOCK_H
#include <stdint.h>
static inline uint64_t agon_cycles_to_ns(uint64_t cycles) {
    const uint64_t hz=240000000ull, ns=1000000000ull;
    return cycles/hz*ns+(cycles%hz)*ns/hz;
}
#endif
