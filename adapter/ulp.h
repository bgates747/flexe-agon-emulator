/* Classic ESP32 ULP FSM execution for the firmware's PS/2 controller.
 * Copyright 2026 Brandon Gates; MIT. Instruction encoding: ESP-IDF ulp.h.
 * No firmware symbols, hooks, mailbox offsets or native keyboard decoding.
 */
#ifndef FLEXE_AGON_ULP_H
#define FLEXE_AGON_ULP_H
#include "memory.h"
#include "peripherals.h"
#include <stdbool.h>
typedef struct {
    uint16_t r[4], pc;
    uint8_t stage;
    bool running, zero, overflow, fault;
    uint64_t ticks, instructions;
} agon_ulp_t;
/* Execute one instruction. Returns its ULP clock cost, zero on failure. */
unsigned agon_ulp_step(agon_ulp_t *u, xtensa_mem_t *m, esp32_periph_t *p);
#endif
