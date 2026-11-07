/* Set-2 PS/2 keyboard device. Copyright 2026 Brandon Gates; MIT.
 * Clock/data go to GPIO33/32; guest firmware executes its own ULP and decoder.
 * Call all methods on the emulation thread, including queued host events.
 */
#ifndef FLEXE_AGON_PS2_H
#define FLEXE_AGON_PS2_H
#include "peripherals.h"
#include <stdbool.h>
#include <stdint.h>
typedef struct {
    uint8_t bytes[4096], replies[32];
    unsigned head,size,rhead,rsize;
    bool held[512], enabled, initialized, clock, receiving, response, fault;
    unsigned bit, phase, leds, parameter, repeat_key;
    uint16_t word;
    uint8_t command, last;
    uint64_t next, inhibit, repeat_at, delay_us, period_us;
    uint64_t sent, received;
} agon_ps2_t;
void agon_ps2_init(agon_ps2_t *k, esp32_periph_t *p);
void agon_ps2_event(agon_ps2_t *k, uint16_t code, bool down, uint64_t us);
void agon_ps2_tick(agon_ps2_t *k, esp32_periph_t *p, uint64_t us);
#endif
