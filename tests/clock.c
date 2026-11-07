/* Long-session pacing arithmetic, including the old overflow boundary. MIT. */
#include "clock.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    assert(agon_cycles_to_ns(0)==0);
    assert(agon_cycles_to_ns(1)==4);
    assert(agon_cycles_to_ns(239999999)==999999995);
    const uint64_t seconds[]={1,60,76,77,78,3600,86400,3153600000ull};
    for(unsigned i=0;i<sizeof(seconds)/sizeof(seconds[0]);i++)
        assert(agon_cycles_to_ns(seconds[i]*240000000ull)==seconds[i]*1000000000ull);
    const uint64_t edge=UINT64_MAX/1000000000ull;
    for(uint64_t cycles=edge-3;cycles<=edge+3;cycles++) {
        uint64_t expected=(uint64_t)(((__uint128_t)cycles*1000000000ull)/240000000ull);
        assert(agon_cycles_to_ns(cycles)==expected);
        assert(agon_cycles_to_ns(cycles+1)>agon_cycles_to_ns(cycles));
    }
    assert(77ull*240000000ull*1000000000ull/240000000ull!=77000000000ull);
    puts("PASS host pacing: subsecond rounding and long-session overflow boundary");
}
