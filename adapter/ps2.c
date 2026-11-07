/* Copyright 2026 Brandon Gates; MIT. See ps2.h. */
#include "ps2.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#define CLK 33
#define DAT 32
static void put(agon_ps2_t *k, uint8_t b, bool reply) {
    unsigned cap=reply?32:4096, n=reply?k->rsize:k->size, h=reply?k->rhead:k->head;
    if(n==cap) {k->fault=true;return;}
    (reply?k->replies:k->bytes)[(h+n)%cap]=b;
    if(reply) k->rsize++; else k->size++;
}
static void scan(agon_ps2_t *k, unsigned key, bool down) {
    if(key==0x62) {
        static const uint8_t pause[]={0xe1,0x14,0x77,0xe1,0xf0,0x14,0xf0,0x77};
        if(down) for(unsigned i=0;i<sizeof(pause);i++) put(k,pause[i],false);
    } else if(key==0x17c) {
        static const uint8_t make[]={0xe0,0x12,0xe0,0x7c}, brk[]={0xe0,0xf0,0x7c,0xe0,0xf0,0x12};
        const uint8_t *b=down?make:brk;unsigned n=down?sizeof(make):sizeof(brk);
        for(unsigned i=0;i<n;i++)put(k,b[i],false);
    } else {
        if(key&0x100) put(k,0xe0,false);
        if(!down) put(k,0xf0,false);
        put(k,key&255,false);
    }
}
static void defaults(agon_ps2_t *k) {
    k->enabled=true;k->delay_us=500000;k->period_us=91667;
    k->repeat_key=0;k->leds=0;
}
void agon_ps2_init(agon_ps2_t *k, esp32_periph_t *p) {
    memset(k,0,sizeof(*k)); defaults(k);k->initialized=true;k->clock=true;
    periph_gpio_set_input(p,CLK,1);periph_gpio_set_input(p,DAT,1);
    /* The unused mouse connector has pull-ups too. A floating-low port
     * would look like an endless incoming frame to the firmware's ULP. */
    periph_gpio_set_input(p,26,1);periph_gpio_set_input(p,27,1);
}
void agon_ps2_event(agon_ps2_t *k, uint16_t code, bool down, uint64_t us) {
    if(!code || (code>255 && (code&0xff00)!=0xe000)) return;
    unsigned key=(code&255)|((code&0xff00)?0x100:0);
    if(k->held[key]==down) return; /* Host repeat never sets device typematic. */
    k->held[key]=down;
    if(k->enabled) scan(k,key,down);
    if(!down) {if(k->repeat_key==key) k->repeat_key=0;return;}
    switch(code) {
    case 0x12:case 0x59:case 0x14:case 0xe014:case 0x11:case 0xe011:
    case 0xe01f:case 0xe027:case 0x58:case 0x77:case 0x7e:case 0x62:case 0xe07c:return;
    }
    k->repeat_key=key;k->repeat_at=us+k->delay_us;
}
static void command(agon_ps2_t *k, uint8_t b) {
    if(getenv("AGON_FLEXE_PS2_TRACE")) fprintf(stderr,"[ps2] command %02x\n",b);
    k->received++;
    if(k->parameter) {
        unsigned cmd=k->parameter;k->parameter=0;put(k,0xfa,true);
        if(cmd==0xed) k->leds=b&7;
        else if(cmd==0xf3) {
            k->delay_us=250000ull*(1+((b>>5)&3));
            k->period_us=1000000ull*(8+(b&7))*(1<<((b>>3)&3))/240;
        } else if(cmd==0xf0 && b==0) put(k,2,true);
        else if(cmd==0xf0 && b!=2) {k->rsize--;put(k,0xfe,true);}
        return;
    }
    switch(b) {
    case 0xff:
        k->head=k->size=0;defaults(k);put(k,0xfa,true);put(k,0xaa,true);break;
    case 0xf2:put(k,0xfa,true);put(k,0xab,true);put(k,0x83,true);break;
    case 0xed:case 0xf0:case 0xf3:k->parameter=b;put(k,0xfa,true);break;
    case 0xee:put(k,0xee,true);break;
    case 0xf4:k->enabled=true;put(k,0xfa,true);break;
    case 0xf5:k->enabled=false;k->head=k->size=0;k->repeat_key=0;put(k,0xfa,true);break;
    case 0xf6:defaults(k);put(k,0xfa,true);break;
    case 0xfe:put(k,k->last,true);break;
    default:put(k,0xfe,true);break;
    }
}
static unsigned parity(uint8_t b) {
    unsigned p=1;for(unsigned i=0;i<8;i++)p^=(b>>i)&1;return p;
}
static void sent(agon_ps2_t *k) {
    uint8_t b=(k->word>>1)&255;k->last=b;k->sent++;
    if(getenv("AGON_FLEXE_PS2_TRACE")) fprintf(stderr,"[ps2] sent %02x\n",b);
    if(k->response){k->rhead=(k->rhead+1)%32;k->rsize--;}
    else {k->head=(k->head+1)%4096;k->size--;}
}
void agon_ps2_tick(agon_ps2_t *k, esp32_periph_t *p, uint64_t us) {
    bool host_clk=!periph_gpio_output_enabled(p,CLK)||periph_gpio_pin_level(p,CLK);
    bool host_dat=!periph_gpio_output_enabled(p,DAT)||periph_gpio_pin_level(p,DAT);
    if(!host_clk) {
        /* Host may inhibit immediately after sampling the stop bit's falling
         * edge. That completes the byte even without a final rising edge. */
        if(k->phase && !k->receiving && k->bit==10 && !k->clock) sent(k);
        if(!k->inhibit) k->inhibit=us?us:1;
        k->phase=0;k->clock=true;
        periph_gpio_set_input(p,CLK,1);periph_gpio_set_input(p,DAT,1);
        return;
    }
    if(k->inhibit) {
        bool request=!host_dat && us-k->inhibit>=100;
        k->inhibit=0;
        if(request) {
            k->phase=1;k->receiving=true;k->bit=0;k->word=0;k->clock=true;k->next=us+40;
        }
    }
    if(!k->phase) {
        if(us<k->next) return; /* Allow host ULP to switch TX back to RX. */
        if(k->enabled && k->repeat_key && us>=k->repeat_at) {
            scan(k,k->repeat_key,true);k->repeat_at=us+k->period_us;
        }
        if(!k->rsize && !k->size) return;
        k->response=k->rsize>0;
        if(!k->response && !k->enabled) return;
        uint8_t b=k->response?k->replies[k->rhead]:k->bytes[k->head];
        k->word=((uint16_t)b<<1)|(parity(b)<<9)|(1u<<10);
        k->phase=1;k->receiving=false;k->bit=0;k->clock=true;k->next=us+40;
        periph_gpio_set_input(p,DAT,0);
    }
    if(us<k->next)return;
    k->next=us+40;k->clock=!k->clock;
    if(!k->clock) {
        if(k->receiving && k->bit==10) periph_gpio_set_input(p,DAT,0); /* wire ACK */
        periph_gpio_set_input(p,CLK,0);
    } else {
        if(k->receiving && k->bit<10 && host_dat) k->word|=1u<<k->bit;
        periph_gpio_set_input(p,CLK,1);
        if(++k->bit==11) {
            if(k->receiving) {
                uint8_t b=k->word&255;
                if(((k->word>>8)&1)!=parity(b) || !(k->word&(1u<<9))) put(k,0xfe,true);
                else command(k,b);
            } else {
                sent(k);
            }
            k->phase=0;periph_gpio_set_input(p,DAT,1);k->next=us+100;
        } else if(!k->receiving) periph_gpio_set_input(p,DAT,(k->word>>k->bit)&1);
    }
}
