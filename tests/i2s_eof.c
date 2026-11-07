/* ESP32 I2S EOF witnesses survive intervening non-EOF DMA completions.
 * Original regression: Brandon Gates, MIT. Uses public memory/peripheral APIs.
 */
#include "peripherals.h"
#include "xtensa.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
volatile int emu_app_running=1;
#define EOF_BIT (1u<<12)
#define REQUIRE(c) do {if(!(c)){fprintf(stderr,"FAIL I2S EOF: port=%u early_clear=%u line=%d: %s\n",port,early_clear,__LINE__,#c);return 1;}}while(0)
static unsigned calls;
static uint8_t emitted[16];
static void capture(void *ctx,int port,const uint8_t *data,size_t n,uint32_t rate,uint8_t bits,uint8_t channels){
    (void)ctx;(void)port;(void)rate;(void)bits;(void)channels;
    if(n==4 && calls<4)memcpy(emitted+4*calls,data,4);
    calls++;
}
static int check(unsigned port,unsigned early_clear){
    xtensa_mem_t*m=mem_create();REQUIRE(m);esp32_periph_t*p=periph_create(m);REQUIRE(p);
    xtensa_cpu_t cpu;xtensa_cpu_init(&cpu);cpu.mem=m;periph_attach_cpus(p,&cpu,NULL);
    const uint32_t base=port?0x3ff6d000u:0x3ff4f000u,desc=0x3ffb0000u,buf=0x3ffb1000u;
    for(unsigned i=0;i<4;i++){
        mem_write32(m,desc+12*i,4u|(4u<<12)|(1u<<31)|((i&1)?0:(1u<<30)));
        mem_write32(m,desc+12*i+4,buf+4*i);
        mem_write32(m,desc+12*i+8,i==3?0:desc+12*(i+1));
        mem_write32(m,buf+4*i,0x04030201u+0x04040404u*i);
    }
    calls=0;memset(emitted,0,sizeof(emitted));
    REQUIRE(!periph_set_i2s_tx_callback(p,(int)port,capture,NULL));
    mem_write32(m,base+0xac,4u);mem_write32(m,base+0xb0,(8u<<12)|1u);
    mem_write32(m,base+0xa8,1u<<5);mem_write32(m,base+0x20,1u<<12);
    mem_write32(m,base+0x14,EOF_BIT);mem_write32(m,base+0x30,(desc&0xfffffu)|(1u<<29));
    mem_write32(m,base+8,1u<<4);
    REQUIRE(calls==1);REQUIRE(mem_read32(m,base+0x38)==desc);REQUIRE(mem_read32(m,base+0x40)==buf);
    REQUIRE(mem_read32(m,base+0x10)&EOF_BIT);
    if(early_clear)mem_write32(m,base+0x18,EOF_BIT);
    for(unsigned completed=2;completed<=4;completed++){
        for(unsigned n=0;calls<completed && n<32;n++){
            uint32_t deadline=cpu.periph_next_event(&cpu);REQUIRE(deadline!=UINT32_MAX);
            cpu.ccount=deadline;cpu.periph_event(&cpu);
        }
        REQUIRE(calls==completed);
        unsigned witness=completed>=3?2:0;
        REQUIRE(mem_read32(m,base+0x38)==desc+12*witness);
        REQUIRE(mem_read32(m,base+0x40)==buf+4*witness);
        REQUIRE(mem_read32(m,base+0x54)==desc+12*(completed-1));
        if(completed==2)REQUIRE(!!(mem_read32(m,base+0x10)&EOF_BIT)==!early_clear);
        if(completed==3){REQUIRE(mem_read32(m,base+0x10)&EOF_BIT);mem_write32(m,base+0x18,EOF_BIT);}
        if(completed==4)REQUIRE(!(mem_read32(m,base+0x10)&EOF_BIT));
    }
    for(unsigned i=0;i<16;i++)REQUIRE(emitted[i]==i+1);
    mem_write32(m,base+0x38,0);mem_write32(m,base+0x40,0);
    REQUIRE(mem_read32(m,base+0x38)==desc+24);REQUIRE(mem_read32(m,base+0x40)==buf+8);
    periph_destroy(p);mem_destroy(m);return 0;
}
int main(void){for(unsigned port=0;port<2;port++)for(unsigned clear=0;clear<2;clear++)if(check(port,clear))return 1;
    puts("PASS I2S EOF: both ports, pending/cleared IRQ, DMA payload and read-only witnesses");return 0;}
