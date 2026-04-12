/*
 * RC702 CP/M 2.2 BIOS — entry points with IOBYTE-routed I/O.
 *
 * Per RC702_BIOS_SPECIFICATION.md Sections 5, 6, 8, 9.
 */

#include "types.h"
#include "hal.h"
#include "iobyte.h"
#include "sector.h"
#include "dpb.h"
#include "config.h"
#include "deblock.h"
#include "console.h"
#include "chartab.h"
#include "ringbuf.h"
#include "serial.h"

/* CP/M system addresses for 56K configuration */
#define CCP_BASE    0xC400
#define BDOS_BASE   0xD480
#define BIOS_BASE   0xDA00

/* IOBYTE location */
#define IOBYTE_ADDR 0x0003
#define CDISK_ADDR  0x0004

/* Bell port */
#define BELL_PORT   0x1C

/* SIO-B DCD detection port and bit */
#define SIO_B_RR0_PORT  0x0B
#define SIO_B_DCD_BIT   0x08

/* ---- Global BIOS state ---- */

/* Current disk/track/sector/DMA */
static byte  cur_disk;
static word  cur_track;
static word  cur_sector;
static byte *cur_dma;

/* IOBYTE shadow (also at memory address 0x0003 on Z80) */
static byte iobyte_val;

/* Deblocking state */
static deblock_t deblock_state;

/* Console display state */
static console_t console_state;

/* Character conversion tables */
static chartab_t chartab_state;

/* Serial channels and keyboard */
static serial_ch_t sio_a;   /* SIO Channel A: data/printer port */
static serial_ch_t sio_b;   /* SIO Channel B: console/terminal port */
static keyboard_t  keyboard;

/* Stub disk I/O — to be replaced with real FDC driver */
static int stub_disk_read(byte disk, word track, byte sector, byte *buf) {
    (void)disk; (void)track; (void)sector; (void)buf;
    return 1;  /* not implemented yet */
}

static int stub_disk_write(byte disk, word track, byte sector, const byte *buf) {
    (void)disk; (void)track; (void)sector; (void)buf;
    return 1;  /* not implemented yet */
}

/* ---- CRT output (internal, always writes to display buffer) ---- */

static void crt_output(byte c) {
    if (c == 0x07) {
        hal_out(BELL_PORT, 0x00);
        return;
    }
    console_putchar(&console_state, c);
}

/* ---- BIOS Entry Points ---- */

/* LIST: Printer output — IOBYTE-routed (Section 6.3) */
void bios_list(byte c) {
    byte mode = iobyte_lst_mode(iobyte_val);

    switch (mode) {
    case LST_TTY:
        serial_send(&sio_b, c);
        break;
    case LST_CRT:
        crt_output(c);
        break;
    case LST_LPT:
        serial_send(&sio_a, c);
        break;
    case LST_UL1:
        /* parked — no output */
        break;
    }
}

/* BOOT: Cold boot */
void bios_boot(void) {
    /* Initialize all subsystems */
    console_init(&console_state);
    chartab_init_identity(&chartab_state);
    deblock_init(&deblock_state, stub_disk_read, stub_disk_write);
    serial_ch_init(&sio_a, SIO_A_CTRL, SIO_A_DATA);
    serial_ch_init(&sio_b, SIO_B_CTRL, SIO_B_DATA);
    keyboard_init(&keyboard);

    /* DCD auto-detection (Section 6.8) */
    byte rr0 = hal_in(SIO_B_RR0_PORT);
    if (rr0 & SIO_B_DCD_BIT)
        iobyte_val = IOBYTE_JOINED;   /* remote host detected */
    else
        iobyte_val = IOBYTE_LOCAL;    /* local CRT only */

    cur_disk = 0;
    cur_track = 0;
    cur_sector = 0;
    cur_dma = (byte *)0x0080;
}

/* WBOOT: Warm boot — IOBYTE is preserved */
void bios_wboot(void) {
    hal_ei();
    deblock_flush(&deblock_state);
    deblock_state.unacnt = 0;
}

/* CONST: Console status — returns 0xFF if char ready, 0x00 if not */
byte bios_const(void) {
    byte mode = iobyte_con_mode(iobyte_val);

    /* Check keyboard if allowed (CRT=1, UC1=3) */
    if (iobyte_keyboard_allowed(iobyte_val)) {
        if (ringbuf_has_data(&keyboard.ring))
            return 0xFF;
    }

    /* Check SIO-B if allowed (TTY=0, BAT=2, UC1=3) */
    if (iobyte_serial_allowed(iobyte_val)) {
        if (serial_rx_ready(&sio_b))
            return 0xFF;
    }

    (void)mode;
    return 0x00;
}

/* CONIN: Console input — waits until char available */
byte bios_conin(void) {
    byte ch;

    /* Wait for input from appropriate source(s) */
    for (;;) {
        /* Check keyboard if allowed */
        if (iobyte_keyboard_allowed(iobyte_val)) {
            if (ringbuf_has_data(&keyboard.ring)) {
                ch = ringbuf_get(&keyboard.ring);
                keyboard.status = ringbuf_has_data(&keyboard.ring) ? 0xFF : 0x00;
                break;
            }
        }

        /* Check SIO-B if allowed */
        if (iobyte_serial_allowed(iobyte_val)) {
            if (serial_rx_ready(&sio_b)) {
                ch = serial_receive(&sio_b);
                break;
            }
        }

        /* No data — halt and wait for interrupt */
        hal_ei();
        /* On Z80 this would be HALT; on native tests we just return */
#ifndef __SDCC
        return 0x00;  /* native test: don't spin forever */
#endif
    }

    /* Apply input conversion table, clear bit 7 */
    ch = chartab_input(&chartab_state, ch) & 0x7F;
    return ch;
}

/* CONOUT: Console output — IOBYTE-routed (Section 6.2) */
void bios_conout(byte c) {
    byte mode = iobyte_con_mode(iobyte_val);

    switch (mode) {
    case CON_TTY:
        /* SIO-B serial, then fall through to CRT display */
        serial_send(&sio_b, c);
        crt_output(c);
        break;
    case CON_CRT:
        /* CRT only */
        crt_output(c);
        break;
    case CON_BAT:
        /* Batch: send to LIST device */
        bios_list(c);
        break;
    case CON_UC1:
        /* Joined: SIO-B + CRT simultaneously */
        serial_send(&sio_b, c);
        crt_output(c);
        break;
    }
}

/* PUNCH: Punch output — IOBYTE-routed (Section 6.4) */
void bios_punch(byte c) {
    byte mode = iobyte_pun_mode(iobyte_val);

    switch (mode) {
    case PUN_TTY:
    case PUN_CRT:
        /* Both route to SIO-A */
        serial_send(&sio_a, c);
        break;
    case PUN_BAT:
        /* SIO-B (UP1) */
        serial_send(&sio_b, c);
        break;
    case PUN_UC1:
        /* parked — no output */
        break;
    }
}

/* READER: Reader input — IOBYTE-routed (Section 6.5) */
byte bios_reader(void) {
    byte mode = iobyte_rdr_mode(iobyte_val);

    switch (mode) {
    case RDR_TTY:
    case RDR_CRT:
        /* SIO-A (PTR) with RTS flow control */
        while (!serial_rx_ready(&sio_a)) {
            hal_ei();
#ifndef __SDCC
            return 0x1A;  /* native test: don't spin */
#endif
        }
        {
            byte ch = serial_receive(&sio_a);
            serial_a_reassert_rts(&sio_a);
            return ch;
        }
    case RDR_BAT:
        /* SIO-B (UR1) — no flow control */
        while (!serial_rx_ready(&sio_b)) {
            hal_ei();
#ifndef __SDCC
            return 0x1A;
#endif
        }
        return serial_receive(&sio_b);
    case RDR_UC1:
        /* Parked — return EOF */
        return 0x1A;
    }
    return 0x1A;
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

/* LISTST: Printer status — IOBYTE-routed (Section 6.6) */
byte bios_listst(void) {
    byte mode = iobyte_lst_mode(iobyte_val);

    switch (mode) {
    case LST_TTY:
        return sio_b.tx_ready;
    case LST_CRT:
        return 0xFF;  /* CRT always ready */
    case LST_LPT:
        return sio_a.tx_ready;
    case LST_UL1:
        return 0x00;  /* parked */
    }
    return 0x00;
}

/* SECTRAN: Sector translate — pass through unchanged per spec Section 10.8 */
word bios_sectran(word sector, word xlt) {
    (void)xlt;
    return sector;
}

/* ---- Accessors for testing ---- */

byte bios_get_iobyte(void) { return iobyte_val; }
void bios_set_iobyte(byte val) { iobyte_val = val; }
console_t *bios_get_console(void) { return &console_state; }
serial_ch_t *bios_get_sio_a(void) { return &sio_a; }
serial_ch_t *bios_get_sio_b(void) { return &sio_b; }
keyboard_t *bios_get_keyboard(void) { return &keyboard; }

/*
 * For the ticks simulator, main() exercises the BIOS logic.
 */
int main(void) {
    bios_boot();

    /* Verify init */
    if (console_state.curx != 0) return 1;
    if (console_state.cursy != 0) return 1;

    /* IOBYTE should be LOCAL (no DCD in ticks) */
    if (iobyte_val != IOBYTE_LOCAL) return 2;

    /* Force CRT mode for testing */
    iobyte_val = IOBYTE_LOCAL;

    /* CONOUT writes to display */
    bios_conout('H');
    if (console_state.display[0] != 'H') return 3;

    /* CONST returns 0 with no input */
    if (bios_const() != 0x00) return 4;

    /* LISTST with LST:LPT returns SIO-A tx_ready */
    if (bios_listst() != sio_a.tx_ready) return 5;

    /* Sector translate pass-through */
    if (bios_sectran(5, 0) != 5) return 6;

    /* Format index */
    if (dpb_format_index(8) != 1) return 7;

    /* SETTRK/SETSEC/SETDMA */
    bios_settrk(10);
    bios_setsec(3);
    bios_setdma(0x0100);
    if (cur_track != 10) return 8;
    if (cur_sector != 3) return 9;

    /* Baud rate */
    if (baud_rate_calc(1, 0x44) != 38400) return 10;

    /* Simulate keyboard input and test CONIN */
    ringbuf_put(&keyboard.ring, 'A');
    keyboard.status = 0xFF;
    if (bios_const() != 0xFF) return 11;

    byte ch = bios_conin();
    if (ch != 'A') return 12;

    /* Test LIST routing — LST:CRT should write to display */
    iobyte_val = (iobyte_val & 0x3F) | (LST_CRT << 6);
    bios_list('Z');
    /* 'H' at pos 0, cursor advanced to 1, then 'A' via conin doesn't write,
     * so 'Z' via LIST/CRT goes through crt_output to current cursor pos */

    /* Test READER with RDR:UC1 (parked) — returns EOF */
    iobyte_val = (iobyte_val & 0xF3) | (RDR_UC1 << 2);
    if (bios_reader() != 0x1A) return 13;

    return 0;
}
