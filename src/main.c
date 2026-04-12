/*
 * RC702 CP/M 2.2 BIOS — Z80 entry point and jump table.
 *
 * Per RC702_BIOS_SPECIFICATION.md Section 5:
 * The BIOS provides a jump table at 0xDA00 with 18 standard CP/M entries
 * plus extended entries starting at +0x4A.
 *
 * This is a stub implementation — each entry point is wired up but most
 * functions are placeholders that will be filled in as the implementation
 * progresses.
 */

#include "types.h"
#include "iobyte.h"
#include "sector.h"
#include "dpb.h"
#include "config.h"
#include "deblock.h"
#include "console.h"
#include "chartab.h"

/* CP/M system addresses for 56K configuration */
#define CCP_BASE    0xC400
#define BDOS_BASE   0xD480  /* CCP_BASE + 0x1080 */
#define BIOS_BASE   0xDA00

/* IOBYTE location */
#define IOBYTE_ADDR 0x0003
#define CDISK_ADDR  0x0004

/* Current disk/track/sector/DMA set by SETTRK/SETSEC/SETDMA */
static byte  cur_disk;
static word  cur_track;
static word  cur_sector;
static byte *cur_dma;

/* Deblocking state */
static deblock_t deblock_state;

/* Console state */
static console_t console_state;

/* Character conversion tables */
static chartab_t chartab_state;

/* Stub disk I/O — to be replaced with real FDC driver */
static int stub_disk_read(byte disk, word track, byte sector, byte *buf) {
    (void)disk; (void)track; (void)sector; (void)buf;
    return 1;  /* error — not implemented yet */
}

static int stub_disk_write(byte disk, word track, byte sector, const byte *buf) {
    (void)disk; (void)track; (void)sector; (void)buf;
    return 1;  /* error — not implemented yet */
}

/*
 * BIOS entry point implementations.
 * These are called from the jump table (defined in crt0 or linker script).
 */

/* BOOT: Cold boot */
void bios_boot(void) {
    console_init(&console_state);
    chartab_init_identity(&chartab_state);
    deblock_init(&deblock_state, stub_disk_read, stub_disk_write);
    cur_disk = 0;
    cur_track = 0;
    cur_sector = 0;
    cur_dma = (byte *)0x0080;
}

/* WBOOT: Warm boot */
void bios_wboot(void) {
    deblock_flush(&deblock_state);
    deblock_state.unacnt = 0;
}

/* CONST: Console status */
byte bios_const(void) {
    return 0x00;  /* stub — no input yet */
}

/* CONIN: Console input (waits) */
byte bios_conin(void) {
    return 0x00;  /* stub */
}

/* CONOUT: Console output */
void bios_conout(byte c) {
    console_putchar(&console_state, c);
}

/* LIST: Printer output */
void bios_list(byte c) {
    (void)c;  /* stub */
}

/* PUNCH: Punch output */
void bios_punch(byte c) {
    (void)c;  /* stub */
}

/* READER: Reader input */
byte bios_reader(void) {
    return 0x1A;  /* EOF */
}

/* HOME: Home disk */
void bios_home(void) {
    cur_track = 0;
}

/* SELDSK: Select disk, returns DPH address or 0 */
word bios_seldsk(byte disk) {
    cur_disk = disk;
    /* stub — return 0 (error) for now */
    return 0;
}

/* SETTRK: Set track */
void bios_settrk(word track) {
    cur_track = track;
}

/* SETSEC: Set sector */
void bios_setsec(word sector) {
    cur_sector = sector;
}

/* SETDMA: Set DMA address */
void bios_setdma(word dma) {
    cur_dma = (byte *)dma;
}

/* READ: Read sector */
byte bios_read(void) {
    deblock_state.sekdsk = cur_disk;
    deblock_state.sektrk = cur_track;
    deblock_state.seksec = (byte)cur_sector;
    deblock_state.dmaadr = cur_dma;
    return (byte)deblock_read(&deblock_state);
}

/* WRITE: Write sector */
byte bios_write(byte wrtyp) {
    deblock_state.sekdsk = cur_disk;
    deblock_state.sektrk = cur_track;
    deblock_state.seksec = (byte)cur_sector;
    deblock_state.dmaadr = cur_dma;
    return (byte)deblock_write(&deblock_state, wrtyp);
}

/* LISTST: Printer status */
byte bios_listst(void) {
    return 0x00;  /* stub — not ready */
}

/* SECTRAN: Sector translate — pass through unchanged per spec Section 10.8 */
word bios_sectran(word sector, word xlt) {
    (void)xlt;
    return sector;
}

/*
 * For the ticks simulator test, main() exercises the BIOS logic
 * and returns 0 on success.
 */
int main(void) {
    bios_boot();

    /* Verify console init worked */
    if (console_state.curx != 0) return 1;
    if (console_state.cursy != 0) return 1;

    /* Write a character */
    bios_conout('H');
    if (console_state.display[0] != 'H') return 2;
    if (console_state.curx != 1) return 3;

    /* Test IOBYTE parsing */
    if (iobyte_con_mode(0x97) != CON_UC1) return 4;
    if (iobyte_lst_mode(0x95) != LST_LPT) return 5;

    /* Test sector translate (pass-through) */
    if (bios_sectran(5, 0) != 5) return 6;

    /* Test format index */
    if (dpb_format_index(8) != 1) return 7;

    /* Test SETTRK/SETSEC/SETDMA */
    bios_settrk(10);
    bios_setsec(3);
    bios_setdma(0x0100);
    if (cur_track != 10) return 8;
    if (cur_sector != 3) return 9;

    /* Test baud rate calc */
    if (baud_rate_calc(1, 0x44) != 38400) return 10;

    return 0;  /* all tests passed */
}
