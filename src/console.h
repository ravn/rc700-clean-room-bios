#ifndef CONSOLE_H
#define CONSOLE_H

#include <stdint.h>

/*
 * Console display logic — cursor tracking, control character dispatch,
 * XY escape addressing, scrolling, background bitmap.
 *
 * Per RC702_BIOS_SPECIFICATION.md Section 8.
 *
 * This module manages the logical display state only. The actual display
 * buffer (0xF800) and 8275 CRT controller are handled through the HAL.
 */

#define SCREEN_COLS   80
#define SCREEN_ROWS   25
#define SCREEN_SIZE   (SCREEN_COLS * SCREEN_ROWS)  /* 2000 bytes */

/* Background bitmap: 80*25/8 = 250 bytes, 10 bytes per row */
#define BGMAP_BYTES_PER_ROW  10
#define BGMAP_SIZE           (BGMAP_BYTES_PER_ROW * SCREEN_ROWS)  /* 250 */

/* XY escape state machine states */
#define XY_STATE_NORMAL  0
#define XY_STATE_BYTE2   1   /* waiting for second coordinate */
#define XY_STATE_BYTE1   2   /* waiting for first coordinate */

/* Background mode flags */
#define BG_OFF           0
#define BG_FOREGROUND    1
#define BG_BACKGROUND    2

/* Console state */
typedef struct {
    /* Display buffer (logical, not memory-mapped on native) */
    uint8_t display[SCREEN_SIZE];

    /* Cursor position */
    uint8_t  curx;       /* column 0..79 */
    uint8_t  cursy;      /* row 0..24 */
    uint16_t cury;       /* row offset = cursy * 80 */

    /* XY escape state machine */
    uint8_t  xflg;       /* XY_STATE_NORMAL/BYTE1/BYTE2 */
    uint8_t  adr0;       /* first coordinate saved between bytes */
    uint8_t  adrmod;     /* 0=XY (col,row), 1=YX (row,col) */

    /* Background mode */
    uint8_t  bg_flag;    /* BG_OFF, BG_FOREGROUND, BG_BACKGROUND */
    uint8_t  bgmap[BGMAP_SIZE];

    /* Cursor dirty flag (for deferred 8275 update) */
    uint8_t  cursor_dirty;
} console_t;

/* Initialize console state: clear screen, home cursor */
void console_init(console_t *con);

/* Process a character for CRT display output.
 * Handles control chars, XY escapes, and printable chars. */
void console_putchar(console_t *con, uint8_t ch);

/* Individual control character operations (also used internally) */
void console_clear_screen(console_t *con);
void console_home(console_t *con);
void console_cursor_left(console_t *con);
void console_cursor_right(console_t *con);
void console_cursor_up(console_t *con);
void console_linefeed(console_t *con);
void console_carriage_return(console_t *con);
void console_tab(console_t *con);
void console_erase_to_eol(console_t *con);
void console_erase_to_eos(console_t *con);
void console_insert_line(console_t *con);
void console_delete_line(console_t *con);
void console_enter_background(console_t *con);
void console_enter_foreground(console_t *con);
void console_clear_foreground(console_t *con);

#endif
