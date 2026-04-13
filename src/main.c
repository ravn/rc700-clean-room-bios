/*
 * RC702 CP/M 2.2 BIOS — entry points with IOBYTE-routed I/O.
 *
 * Per RC702_BIOS_SPECIFICATION.md Sections 5, 6, 8, 9.
 */

#include <stddef.h>
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
#include "floppy.h"
#include "hwinit.h"
#include "interrupt.h"
#include "jtvars.h"

/* CP/M system addresses — must match assembled CCP+BDOS */
#define CCP_BASE    0xAA00
#define BDOS_BASE   0xB206  /* BDOS entry = BDOS ORG + 6 */
/* BIOS_BASE defined in jtvars.h (currently 0xC000) */

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
#ifdef __SDCC
/* Z80: display buffer is memory-mapped at 0xF800 for the 8275 CRT DMA */
#define DISPLAY_BUF  ((byte *)DISPLAY_ADDR)
#else
/* Native: static array for testing */
static byte native_display_buf[SCREEN_SIZE];
#define DISPLAY_BUF  native_display_buf
#endif

/* Character conversion tables */
static chartab_t chartab_state;

/* Serial channels and keyboard */
static serial_ch_t sio_a;   /* SIO Channel A: data/printer port */
static serial_ch_t sio_b;   /* SIO Channel B: console/terminal port */
static keyboard_t  keyboard;

/* Floppy driver */
static floppy_t floppy_state;

/* JTVARS */
static jtvars_t jtvars_local;  /* C-side copy; Z80 build has _jtvars in crt0 */

/* Drive format table (alias into JTVARS) */
#define MAX_DRIVES  16
static byte last_format;      /* last selected format code (0xFF = none) */

/* Current format parameters for selected drive */
static const fspa_t *cur_fspa;
static const fdf_t  *cur_fdf;

/* CP/M DPH structures — one per drive (only A and B used initially) */
/* DPH: XLT(2), scratch(6), DIRBF(2), DPB(2), CSV(2), ALV(2) = 16 bytes */
static byte  dirbuf[128];            /* shared directory buffer */
static byte  csv[2][32];             /* check vectors for drives A, B */
static byte  alv[2][71];             /* allocation vectors for drives A, B */
static word  dph_dpb_ptr[2];         /* DPB pointer in each DPH */

/* Host-sector physical I/O through the floppy driver.
 * These are the callbacks for the deblocking algorithm. */
static int host_disk_read(byte disk, word track, byte sector, byte *buf) {
    if (disk >= MAX_DRIVES || !cur_fdf) return 1;

    /* Map host sector to head + physical sector (Section 10.3) */
    byte head = 0;
    byte phys_sec;
    if (sector < cur_fdf->eot) {
        phys_sec = sector_translate(cur_fspa->tran_table,
                                    cur_fspa->tran_size, sector);
    } else {
        head = 1;
        phys_sec = sector_translate(cur_fspa->tran_table,
                                    cur_fspa->tran_size,
                                    sector - cur_fdf->eot);
    }
    if (phys_sec == 0) return 1;  /* translation error */

    return floppy_read_with_retry(&floppy_state, disk,
                                  (byte)track, head, phys_sec,
                                  cur_fdf, (word)(size_t)buf);
}

static int host_disk_write(byte disk, word track, byte sector,
                           const byte *buf) {
    if (disk >= MAX_DRIVES || !cur_fdf) return 1;

    byte head = 0;
    byte phys_sec;
    if (sector < cur_fdf->eot) {
        phys_sec = sector_translate(cur_fspa->tran_table,
                                    cur_fspa->tran_size, sector);
    } else {
        head = 1;
        phys_sec = sector_translate(cur_fspa->tran_table,
                                    cur_fspa->tran_size,
                                    sector - cur_fdf->eot);
    }
    if (phys_sec == 0) return 1;

    return floppy_write_with_retry(&floppy_state, disk,
                                   (byte)track, head, phys_sec,
                                   cur_fdf, (word)(size_t)buf);
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

#ifdef __SDCC
extern void z80_setup_im2(void);
#endif

/* ---- ISR handlers ---- */
/* On SDCC, __interrupt generates register save/restore + RETI.
 * On native builds, these are plain functions (called from tests). */

#ifdef __SDCC
#define ISR_FUNC  __interrupt
#else
#define ISR_FUNC
#endif

void isr_keyboard_handler(void) ISR_FUNC {
    keyboard_isr(&keyboard);
}

void isr_sio_b_tx(void) ISR_FUNC   { serial_tx_isr(&sio_b); }
void isr_sio_b_ext(void) ISR_FUNC  { serial_ext_isr(&sio_b); }
void isr_sio_b_rx(void) ISR_FUNC   { serial_rx_isr(&sio_b); }
void isr_sio_b_spec(void) ISR_FUNC {
    serial_special_isr(&sio_b);
    ringbuf_reset(&sio_b.rx_ring);  /* flush on error per Section 4.9 */
}

void isr_sio_a_tx(void) ISR_FUNC   { serial_tx_isr(&sio_a); }
void isr_sio_a_ext(void) ISR_FUNC  { serial_ext_isr(&sio_a); }
void isr_sio_a_rx(void) ISR_FUNC   {
    serial_rx_isr(&sio_a);
    serial_a_check_rts(&sio_a);  /* check high-water mark */
}
void isr_sio_a_spec(void) ISR_FUNC { serial_special_isr(&sio_a); }

/* Forward declaration */
void bios_wboot(void);

/* Signon message */
static const char signon[] = "\x0C" "RC702 Clean Room BIOS v0.1\r\n";

/* BOOT: Cold boot (Section 13.1) */
void bios_boot(void) {
    /* Step 1: Display + IM2 — get screen working first */
    console_init(&console_state, DISPLAY_BUF);

    /* Set display ISR state BEFORE enabling our interrupts */
    display_isr_state.display_buf = console_state.display;
    display_isr_state.cursor_dirty = 1;

#ifdef BIOS_WITH_CRT0
    z80_setup_im2();  /* switches to our ISR vectors — ISR must see valid display_buf */
#endif
    hal_ei();

    /* Step 2: Signon */
    for (const char *p = signon; *p; p++)
        crt_output((byte)*p);

    /* Step 3: Initialize remaining subsystems */
    jtvars_init(&jtvars_local, confi_defaults);
    console_state.adrmod = jtvars_local.adrmod;
    chartab_init_identity(&chartab_state);
    deblock_init(&deblock_state, host_disk_read, host_disk_write);
    serial_ch_init(&sio_a, SIO_A_CTRL, SIO_A_DATA);
    sio_a.wr5_base = jtvars_local.wr5a;
    serial_ch_init(&sio_b, SIO_B_CTRL, SIO_B_DATA);
    sio_b.wr5_base = jtvars_local.wr5b;
    keyboard_init(&keyboard);
    floppy_init(&floppy_state);

    /* Step 4: FDC — the PROM already sent SPECIFY.
     * Recalibrate drive 0 to get to a known state. */
    fdc_recalibrate(&floppy_state, 0);

    /* Step 5: Remaining setup */
    last_format = 0xFF;
    cur_fspa = NULL;
    cur_fdf = NULL;

    byte rr0 = hal_in(SIO_B_RR0_PORT);
    iobyte_val = (rr0 & SIO_B_DCD_BIT) ? IOBYTE_JOINED : IOBYTE_LOCAL;

    cur_disk = 0;
    cur_track = 0;
    cur_sector = 0;
    cur_dma = (byte *)0x0080;

    /* Fall through to warm boot — loads CCP+BDOS from track 1 */
    bios_wboot();
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

/* SELDSK: Select disk — format dispatch (Section 10.2) */
word bios_seldsk(byte disk) {
    if (disk >= MAX_DRIVES) return 0;

    byte fmt = jtvars_local.fd0[disk];
    if (fmt == DRIVE_NOT_PRESENT) return 0;
    if (fmt >= 32) return 0;  /* hard disk — not supported yet */

    byte idx = dpb_format_index(fmt);
    if (idx >= FMT_COUNT) return 0;

    /* Flush dirty buffer if format changed */
    if (fmt != last_format)
        deblock_flush(&deblock_state);
    last_format = fmt;

    /* Select format parameters */
    cur_fspa = &fspa_table[idx];
    cur_fdf = &fdf_table[idx];
    cur_disk = disk;

    /* Update deblocking parameters */
    deblock_state.secmsk = cur_fspa->sector_mask;
    deblock_state.secshf = cur_fspa->sector_shift;

    /* Return DPH address — for now return a non-zero sentinel.
     * Real implementation needs proper DPH structures at fixed addresses. */
    return 1;  /* non-zero = success */
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

/* CCP+BDOS size: 0x1600 = 5632 bytes = 176 x 128-byte records */
#define CPML       0x1600
#define NSECTS     (CPML / 128)

/* WBOOT: Warm boot (Section 13.2) — IOBYTE is preserved */
void bios_wboot(void) {
    hal_ei();

    /* Select drive A */
    bios_seldsk(0);
    deblock_state.unacnt = 0;

    /* Home drive (flush + recalibrate) */
    deblock_flush(&deblock_state);
    deblock_state.hstact = 0;
    bios_home();

    /* Load CCP+BDOS from track 1 */
    cur_dma = (byte *)CCP_BASE;
    bios_settrk(1);
    for (word sec = 0; sec < NSECTS; sec++) {
        bios_setsec(sec);
        deblock_state.sekdsk = cur_disk;
        deblock_state.sektrk = cur_track;
        deblock_state.seksec = (byte)sec;
        deblock_state.dmaadr = cur_dma;
        if (deblock_read(&deblock_state) != 0) {
            /* Disk read error — display message and halt */
            const char *msg = "Disk read error - reset\r\n";
            for (const char *p = msg; *p; p++)
                crt_output((byte)*p);
            for (;;) hal_ei();
        }
        cur_dma += 128;
    }

    /* Reset DMA to default */
    cur_dma = (byte *)0x0080;

    /* Install CP/M entry vectors */
#ifdef __SDCC
    /* 0x0000: JP BIOS+3 (warm boot) */
    *(byte *)0x0000 = 0xC3;
    *(word *)0x0001 = BIOS_BASE + 3;
    /* 0x0005: JP BDOS */
    *(byte *)0x0005 = 0xC3;
    *(word *)0x0006 = BDOS_BASE;
#endif

    /* Jump to CCP with current disk in C */
#ifdef __SDCC
    __asm
        ld   a, (_cur_disk)
        ld   c, a
        jp   0xAA00
    __endasm;
#endif
}

/* ---- Extended Entry Points (Section 14) — stubs ---- */

void bios_wfitr(void) { /* stub */ }
byte bios_reads(void) { return 0x00; /* stub */ }
byte bios_linsel(byte port, byte line) { (void)port; (void)line; return 0x00; }
void bios_exit(word callback, word count) { (void)callback; (void)count; }
void bios_clock(byte mode, word de, word hl) { (void)mode; (void)de; (void)hl; }
void bios_hrdfmt(void) { /* stub */ }

#ifdef SMOKE_TEST
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

    /* Test SELDSK — drive A should succeed (format 0x08 = DD MFM) */
    if (bios_seldsk(0) == 0) return 13;
    if (cur_fspa != &fspa_table[FMT_8DD_MFM]) return 14;
    if (cur_fdf != &fdf_table[FMT_8DD_MFM]) return 15;

    /* SELDSK with invalid drive should fail */
    if (bios_seldsk(15) != 0) return 16;

    /* Test READER with RDR:UC1 (parked) — returns EOF */
    iobyte_val = (iobyte_val & 0xF3) | (RDR_UC1 << 2);
    if (bios_reader() != 0x1A) return 17;

    return 0;
}
#endif /* SMOKE_TEST */
