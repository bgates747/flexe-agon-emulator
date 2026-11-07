/* Functional ULP instruction/PS2 device regressions; MIT, Brandon Gates. */
#include "ulp.h"
#include "ps2.h"
#include <assert.h>
#include <stdio.h>
volatile int emu_app_running=1;
static void insn(agon_ulp_t *u,xtensa_mem_t *m,esp32_periph_t *p,uint32_t i){
    mem_write32(m,0x50000000+4*u->pc,i);assert(agon_ulp_step(u,m,p));
}
int main(void){
    xtensa_mem_t *m=mem_create();assert(m);esp32_periph_t *p=periph_create(m);assert(p);
    agon_ulp_t u={0};mem_write32(m,0x3ff4882c,0x300);
    insn(&u,m,p,0x728ffff0);assert(u.r[0]==65535); /* MOVI R0,65535 */
    insn(&u,m,p,0x72000010);assert(u.r[0]==0 && u.zero && u.overflow); /* ADDI R0,1 */
    insn(&u,m,p,0x724000f0);assert(u.r[0]==0); /* ANDI R0,15 */
    insn(&u,m,p,0x72812341);assert(u.r[1]==0x1234);
    insn(&u,m,p,0x68080001);assert((mem_read32(m,0x50000800)&65535)==0x1234);
    insn(&u,m,p,0xd0080002);assert(u.r[2]==0x1234);
    unsigned pc=u.pc;insn(&u,m,p,0x82040001);assert(u.pc==pc+2); /* R0<1 */
    /* RTC interrupt is latched while masked and obeys normal enable/clear. */
    insn(&u,m,p,0x90000001);assert(mem_read32(m,0x3ff48040)&32);assert(!(mem_read32(m,0x3ff48044)&32));
    mem_write32(m,0x3ff4803c,32);assert(mem_read32(m,0x3ff48044)&32);
    mem_write32(m,0x3ff48048,32);assert(!(mem_read32(m,0x3ff48044)&32));
    agon_ps2_t k;agon_ps2_init(&k,p);
    agon_ps2_event(&k,0xe075,true,0);agon_ps2_event(&k,0xe075,true,1);
    assert(k.size==2 && k.bytes[0]==0xe0 && k.bytes[1]==0x75);
    agon_ps2_event(&k,0xe075,false,2);assert(k.size==5 && k.bytes[3]==0xf0 && !k.repeat_key);
    agon_ps2_init(&k,p);agon_ps2_event(&k,0x62,true,0);agon_ps2_event(&k,0x62,false,1);
    assert(k.size==8 && !k.repeat_key && k.bytes[0]==0xe1);
    agon_ps2_init(&k,p);agon_ps2_event(&k,0xe07c,true,0);agon_ps2_event(&k,0xe07c,false,1);
    assert(k.size==10 && k.bytes[2]==0xe0 && k.bytes[6]==0x7c && !k.repeat_key);
    periph_destroy(p);mem_destroy(m);puts("PASS ULP/PS2 functional regressions");return 0;
}
