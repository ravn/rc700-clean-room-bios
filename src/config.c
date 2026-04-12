#include "config.h"
#include <stddef.h>

/* Baud rate table per Section 17.3 */
const baud_entry_t baud_table[BAUD_TABLE_SIZE] = {
    { 38400,   1, 0x44 },
    { 19200,   2, 0x44 },
    {  9600,   4, 0x44 },
    {  4800,   8, 0x44 },
    {  2400,  16, 0x44 },
    {  1200,  32, 0x44 },
    {   600,  64, 0x44 },
    {   300,  32, 0xC4 },
    {   150,  64, 0xC4 },
    {   110,  87, 0xC4 },
    {    75, 128, 0xC4 },
    {    50, 192, 0xC4 },
};

/* Default CONFI block per Section 16.2 */
const byte confi_defaults[CONFI_SIZE] = {
    /* +0x00 CTC */
    0x47, 0x01, 0x47, 0x01, 0xD7, 0x01, 0xD7, 0x01,
    /* +0x08 SIO-A (9 bytes) */
    0x18, 0x04, 0x44, 0x03, 0xE1, 0x05, 0x60, 0x01, 0x1B,
    /* +0x11 SIO-B (11 bytes) */
    0x18, 0x02, 0x10, 0x04, 0x44, 0x03, 0xE1, 0x05, 0x60, 0x01, 0x1F,
    /* +0x1C DMA mode (4 bytes) */
    0x48, 0x49, 0x4A, 0x4B,
    /* +0x20 CRT 8275 params (4 bytes) */
    0x4F, 0x98, 0x7A, 0x6D,
    /* +0x24 FDC SPECIFY (4 bytes) */
    0x03, 0x03, 0xDF, 0x28,
    /* +0x28 User settings */
    0x00,       /* cursor */
    0x00,       /* conversion table */
    0x06,       /* baud A index */
    0x06,       /* baud B index */
    0x00,       /* XY flag */
    0xFA, 0x00, /* motor timer = 250 (little-endian) */
    /* +0x2F Drive table (17 bytes: A, B, C, D..P, terminator) */
    0x08, 0x08, 0x20,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF,
    /* +0x40 HD partition (4 bytes) */
    0x02, 0x02, 0x00, 0x00,
    /* +0x44 CTC2 (3 bytes) */
    0xD7, 0x01, 0x03,
    /* +0x47 Boot device */
    0x00,
    /* +0x48..0x7F reserved (zero) */
    0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,
};

byte confi_get_ctc_mode(const byte *confi, byte channel) {
    if (channel > 3) return 0;
    return confi[CONFI_CTC_CH0_MODE + channel * 2];
}

byte confi_get_ctc_count(const byte *confi, byte channel) {
    if (channel > 3) return 0;
    return confi[CONFI_CTC_CH0_COUNT + channel * 2];
}

byte confi_get_cursor(const byte *confi) {
    return confi[CONFI_CURSOR];
}

byte confi_get_xy_flag(const byte *confi) {
    return confi[CONFI_XY_FLAG];
}

word confi_get_motor_timer(const byte *confi) {
    return (word)confi[CONFI_MOTOR_TIMER]
         | ((word)confi[CONFI_MOTOR_TIMER + 1] << 8);
}

byte confi_get_drive_format(const byte *confi, byte drive) {
    if (drive >= CONFI_DRIVE_COUNT) return DRIVE_NOT_PRESENT;
    return confi[CONFI_DRIVE_TABLE + drive];
}

byte confi_get_boot_device(const byte *confi) {
    return confi[CONFI_BOOT_DEVICE];
}

/* CTC input clock: 19.6608 MHz / 32 = 614400 Hz */
#define CTC_INPUT_CLOCK  614400UL

uint32_t baud_rate_calc(byte ctc_count, byte wr4) {
    /* SIO clock mode from WR4 bits 7:6: 01=x16, 11=x64 */
    byte sio_mode = (wr4 >> 6) & 0x03;
    word clock_div;
    if (sio_mode == 1)
        clock_div = 16;
    else if (sio_mode == 3)
        clock_div = 64;
    else
        return 0;  /* invalid */

    /* CTC count of 0 means 256 */
    word count = ctc_count ? ctc_count : 256;

    return CTC_INPUT_CLOCK / ((uint32_t)count * clock_div);
}

const baud_entry_t *baud_lookup(byte index) {
    if (index >= BAUD_TABLE_SIZE) return NULL;
    return &baud_table[index];
}
