/*
 * SDCC Z80 code generation bug: inline shift expression dropped
 * in function parameter passing when value comes from a struct
 * member pointer cast.
 *
 * Compile and run:
 *   zcc +test -compiler=sdcc -mz80 -SO3 --opt-code-size -create-app bug.c -o bug
 *   cp bug bug.bin && z88dk-ticks bug.bin
 *
 * Expected: exit code 0
 * Actual:   exit code 2 (high byte wrong)
 */

#include <stdint.h>
#include <stddef.h>

/* Simulate hal_out — records what was written */
static uint8_t log_port[8];
static uint8_t log_value[8];
static int log_idx;

void hal_out(uint8_t port, uint8_t value) {
    if (log_idx < 8) {
        log_port[log_idx] = port;
        log_value[log_idx] = value;
        log_idx++;
    }
}

/* State struct with a pointer member — matches the real bug context */
typedef struct {
    uint8_t *display_buf;
    uint8_t  other_field;
} isr_state_t;

static isr_state_t state;

/* Bug: inline cast+shift of struct member pointer as function parameter */
void reprogram_dma_buggy(void) {
    uint16_t disp_addr = (uint16_t)(size_t)state.display_buf;
    hal_out(0xF4, (uint8_t)disp_addr);           /* low byte */
    hal_out(0xF4, (uint8_t)(disp_addr >> 8));     /* high byte — may be dropped */
}

/* Workaround: pre-compute into local variables */
void reprogram_dma_fixed(void) {
    uint8_t addr_lo = (uint8_t)(uint16_t)(size_t)state.display_buf;
    uint8_t addr_hi = (uint8_t)((uint16_t)(size_t)state.display_buf >> 8);
    hal_out(0xF4, addr_lo);
    hal_out(0xF4, addr_hi);
}

int main(void) {
    state.display_buf = (uint8_t *)0xF800;

    /* Test buggy version */
    log_idx = 0;
    reprogram_dma_buggy();
    if (log_value[0] != 0x00) return 1;  /* low byte should be 0x00 */
    if (log_value[1] != 0xF8) return 2;  /* high byte should be 0xF8 */

    /* Test fixed version */
    log_idx = 0;
    reprogram_dma_fixed();
    if (log_value[0] != 0x00) return 3;
    if (log_value[1] != 0xF8) return 4;

    return 0;
}
