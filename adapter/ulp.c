/* Copyright 2026 Brandon Gates; MIT. See ulp.h. */
#include "ulp.h"
#include <stdio.h>
unsigned agon_ulp_step(agon_ulp_t *u, xtensa_mem_t *m, esp32_periph_t *p) {
    if (!u->running) {
        uint32_t start = mem_read32(m, 0x3ff4882c);
        if ((start & 0x300) != 0x300) return 4;
        u->pc = (start >> 11) & 0x7ff;
        u->running = true;
        /* START is a strobe, not a perpetual restart after HALT. */
        mem_write32(m, 0x3ff4882c, start & ~0x200u);
    }
    if (u->pc >= 2048) goto fault;
    uint32_t i = mem_read32(m, 0x50000000u + 4u*u->pc);
    unsigned op=i>>28, sub=(i>>25)&7, d=i&3, s=(i>>2)&3;
    uint16_t next=u->pc+1;
    unsigned cost=4;
    switch(op) {
    case 1: case 2: {
        static const uint32_t base[]={0x3ff48000,0x3ff48400,0x3ff48800,0x3ff48c00};
        uint32_t addr=base[(i>>8)&3]+4*(i&255);
        unsigned lo=(i>>18)&31, hi=(i>>23)&31;
        if (hi<lo) goto fault;
        uint32_t mask=UINT32_MAX>>(31-(hi-lo));
        uint32_t value=mem_read32(m,addr);
        if(op==2) u->r[0]=(value>>lo)&mask;
        else mem_write32(m,addr,(value&~(mask<<lo))|(((i>>10)&255&mask)<<lo));
        cost=8; break;
    }
    case 4: cost=6+(i&65535); break;
    case 6: case 13: {
        unsigned addr=u->r[s]+((i>>10)&0x7ff);
        if(addr>=2048 || (op==6 && sub!=4)) goto fault;
        if(op==13) u->r[d]=mem_read32(m,0x50000000u+4u*addr)&65535;
        else mem_write32(m,0x50000000u+4u*addr,((uint32_t)u->pc<<21)|(d<<16)|u->r[d]);
        cost=8; break;
    }
    case 7: {
        unsigned sel=(i>>21)&15;
        if(sub==2) {
            unsigned imm=(i>>4)&255;
            if(sel==0) u->stage+=imm;
            else if(sel==1) u->stage-=imm;
            else if(sel==2) u->stage=0;
            else goto fault;
            break;
        }
        if(sub>1) goto fault;
        uint32_t a=u->r[s], b=sub?((i>>4)&65535):u->r[(i>>4)&3], v;
        switch(sel) {
        case 0: v=a+b; break;
        case 1: v=a-b; break;
        case 2: v=a&b; break;
        case 3: v=a|b; break;
        case 4: v=sub?b:a; break;
        case 5: v=b<16?a<<b:0; break;
        case 6: v=b<16?a>>b:0; break;
        default: goto fault;
        }
        u->r[d]=v; u->zero=u->r[d]==0;
        u->overflow=sel==0?v>65535:sel==1?a<b:false;
        break;
    }
    case 8: {
        bool take;
        if(sub==0) {
            unsigned type=(i>>22)&7;
            if(type>2) goto fault;
            take=type==0 || (type==1?u->zero:u->overflow);
            if(take) next=(i&(1u<<21))?u->r[d]:(i>>2)&0x7ff;
        } else if(sub==1 || sub==2) {
            if(sub==1) take=(i&(1u<<16))?u->r[0]>=(i&65535):u->r[0]<(i&65535);
            else {
                unsigned cmp=(i>>15)&3, imm=i&255;
                if(cmp>2) goto fault;
                take=cmp==0?u->stage<imm:cmp==1?u->stage>=imm:u->stage<=imm;
            }
            if(take) { int off=(i>>17)&127; next=u->pc+((i&(1u<<24))?-off:off); }
        } else goto fault;
        break;
    }
    case 9:
        if(sub!=0) goto fault;
        if(i&1) periph_ulp_wake(p);
        break;
    case 11: u->running=false; break;
    default: goto fault;
    }
    u->pc=next; u->instructions++; return cost;
fault:
    fprintf(stderr,"[flexe-agon-emulator] unsupported ULP instruction at word %u\n",u->pc);
    u->fault=true; return 0;
}
