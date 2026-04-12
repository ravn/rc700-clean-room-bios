#include "console.h"
#include <string.h>

static void set_cursor(console_t *con, byte col, byte row) {
    con->curx = col;
    con->cursy = row;
    con->cury = (word)row * SCREEN_COLS;
    con->cursor_dirty = 1;
}

/* Scroll display up by one line */
static void scroll_up(console_t *con) {
    /* Copy rows 1..24 to rows 0..23 */
    memmove(&con->display[0],
            &con->display[SCREEN_COLS],
            (SCREEN_ROWS - 1) * SCREEN_COLS);
    /* Clear row 24 */
    memset(&con->display[(SCREEN_ROWS - 1) * SCREEN_COLS],
           0x20, SCREEN_COLS);

    /* Scroll background bitmap if active */
    if (con->bg_flag != BG_OFF) {
        memmove(&con->bgmap[0],
                &con->bgmap[BGMAP_BYTES_PER_ROW],
                (SCREEN_ROWS - 1) * BGMAP_BYTES_PER_ROW);
        memset(&con->bgmap[(SCREEN_ROWS - 1) * BGMAP_BYTES_PER_ROW],
               0, BGMAP_BYTES_PER_ROW);
    }
}

static void set_bgmap_bit(console_t *con, byte col, byte row) {
    word bit_index = (word)row * SCREEN_COLS + col;
    con->bgmap[bit_index >> 3] |= (byte)(0x80 >> (bit_index & 7));
}

void console_init(console_t *con, byte *display_buf) {
    byte *saved_display = display_buf;
    memset(con, 0, sizeof(*con));
    con->display = saved_display;
    memset(con->display, 0x20, SCREEN_SIZE);
    con->cursor_dirty = 1;
}

void console_clear_screen(console_t *con) {
    memset(con->display, 0x20, SCREEN_SIZE);
    memset(con->bgmap, 0, BGMAP_SIZE);
    set_cursor(con, 0, 0);
}

void console_home(console_t *con) {
    set_cursor(con, 0, 0);
}

void console_cursor_left(console_t *con) {
    if (con->curx > 0) {
        con->curx--;
    } else {
        con->curx = SCREEN_COLS - 1;
        if (con->cursy > 0) {
            set_cursor(con, con->curx, con->cursy - 1);
            return;
        } else {
            set_cursor(con, con->curx, SCREEN_ROWS - 1);
            return;
        }
    }
    con->cury = (word)con->cursy * SCREEN_COLS;
    con->cursor_dirty = 1;
}

void console_cursor_right(console_t *con) {
    con->curx++;
    if (con->curx >= SCREEN_COLS) {
        con->curx = 0;
        if (con->cursy < SCREEN_ROWS - 1) {
            con->cursy++;
        } else {
            scroll_up(con);
        }
    }
    con->cury = (word)con->cursy * SCREEN_COLS;
    con->cursor_dirty = 1;
}

void console_cursor_up(console_t *con) {
    if (con->cursy > 0)
        set_cursor(con, con->curx, con->cursy - 1);
    else
        set_cursor(con, con->curx, SCREEN_ROWS - 1);
}

void console_linefeed(console_t *con) {
    if (con->cursy < SCREEN_ROWS - 1) {
        set_cursor(con, con->curx, con->cursy + 1);
    } else {
        scroll_up(con);
        con->cursor_dirty = 1;
    }
}

void console_carriage_return(console_t *con) {
    set_cursor(con, 0, con->cursy);
}

void console_tab(console_t *con) {
    for (int i = 0; i < 4; i++)
        console_cursor_right(con);
}

void console_erase_to_eol(console_t *con) {
    word pos = con->cury + con->curx;
    word end = con->cury + SCREEN_COLS;
    memset(&con->display[pos], 0x20, end - pos);
}

void console_erase_to_eos(console_t *con) {
    word pos = con->cury + con->curx;
    memset(&con->display[pos], 0x20, SCREEN_SIZE - pos);
}

void console_insert_line(console_t *con) {
    /* Shift rows from cursor row through row 23 down by one */
    word src = (word)(SCREEN_ROWS - 2) * SCREEN_COLS;
    word dst = (word)(SCREEN_ROWS - 1) * SCREEN_COLS;
    word cursor_row_offset = con->cury;

    for (word s = src; s >= cursor_row_offset && s <= src; s -= SCREEN_COLS) {
        memcpy(&con->display[s + SCREEN_COLS], &con->display[s], SCREEN_COLS);
    }
    /* Clear cursor row */
    memset(&con->display[cursor_row_offset], 0x20, SCREEN_COLS);

    /* Scroll background bitmap if active */
    if (con->bg_flag != BG_OFF) {
        byte bg_row = con->cursy;
        word bg_src = (word)(SCREEN_ROWS - 2) * BGMAP_BYTES_PER_ROW;
        word bg_cursor = (word)bg_row * BGMAP_BYTES_PER_ROW;
        for (word s = bg_src; s >= bg_cursor && s <= bg_src; s -= BGMAP_BYTES_PER_ROW) {
            memcpy(&con->bgmap[s + BGMAP_BYTES_PER_ROW], &con->bgmap[s],
                   BGMAP_BYTES_PER_ROW);
        }
        memset(&con->bgmap[bg_cursor], 0, BGMAP_BYTES_PER_ROW);
    }

    (void)dst;
}

void console_delete_line(console_t *con) {
    /* Shift rows from cursor row + 1 through row 24 up by one */
    word cursor_row_offset = con->cury;
    word last_row = (word)(SCREEN_ROWS - 1) * SCREEN_COLS;

    for (word d = cursor_row_offset; d < last_row; d += SCREEN_COLS) {
        memcpy(&con->display[d], &con->display[d + SCREEN_COLS], SCREEN_COLS);
    }
    /* Clear row 24 */
    memset(&con->display[last_row], 0x20, SCREEN_COLS);

    /* Scroll background bitmap if active */
    if (con->bg_flag != BG_OFF) {
        byte bg_row = con->cursy;
        word bg_cursor = (word)bg_row * BGMAP_BYTES_PER_ROW;
        word bg_last = (word)(SCREEN_ROWS - 1) * BGMAP_BYTES_PER_ROW;
        for (word d = bg_cursor; d < bg_last; d += BGMAP_BYTES_PER_ROW) {
            memcpy(&con->bgmap[d], &con->bgmap[d + BGMAP_BYTES_PER_ROW],
                   BGMAP_BYTES_PER_ROW);
        }
        memset(&con->bgmap[bg_last], 0, BGMAP_BYTES_PER_ROW);
    }
}

void console_enter_background(console_t *con) {
    con->bg_flag = BG_BACKGROUND;
    memset(con->bgmap, 0, BGMAP_SIZE);
}

void console_enter_foreground(console_t *con) {
    con->bg_flag = BG_FOREGROUND;
}

void console_clear_foreground(console_t *con) {
    /* Erase positions where the background bitmap bit is NOT set */
    for (word i = 0; i < SCREEN_SIZE; i++) {
        byte bm = con->bgmap[i >> 3];
        byte mask = (byte)(0x80 >> (i & 7));
        if (!(bm & mask))
            con->display[i] = 0x20;
    }
}

/* Process XY escape coordinate bytes */
static void process_xy(console_t *con, byte ch) {
    byte coord = (ch & 0x7F) - 32;

    if (con->xflg == XY_STATE_BYTE1) {
        /* First coordinate received */
        con->adr0 = coord;
        con->xflg = XY_STATE_BYTE2;
        return;
    }

    /* Second coordinate — compute final position */
    byte col, row;
    if (con->adrmod == 0) {
        /* XY mode: first=column, second=row */
        col = con->adr0;
        row = coord;
    } else {
        /* YX mode: first=row, second=column */
        row = con->adr0;
        col = coord;
    }

    /* Wrap excess values */
    while (col >= SCREEN_COLS) col -= SCREEN_COLS;
    while (row >= SCREEN_ROWS) row -= SCREEN_ROWS;

    set_cursor(con, col, row);
    con->xflg = XY_STATE_NORMAL;
}

/* Write a printable character to the display */
static void put_printable(console_t *con, byte ch) {
    word pos = con->cury + con->curx;
    con->display[pos] = ch;

    if (con->bg_flag == BG_BACKGROUND)
        set_bgmap_bit(con, con->curx, con->cursy);

    console_cursor_right(con);
}

void console_putchar(console_t *con, byte ch) {
    /* XY escape state machine takes priority */
    if (con->xflg != XY_STATE_NORMAL) {
        process_xy(con, ch);
        return;
    }

    /* Control characters 0x00..0x1F */
    if (ch < 0x20) {
        con->xflg = XY_STATE_NORMAL;  /* reset XY state on any control char */
        switch (ch) {
        case 0x01: console_insert_line(con); break;
        case 0x02: console_delete_line(con); break;
        case 0x05: /* fall through */
        case 0x08: console_cursor_left(con); break;
        case 0x06: con->xflg = XY_STATE_BYTE1; break;
        case 0x07: /* bell — HAL handles port write */ break;
        case 0x09: console_tab(con); break;
        case 0x0A: console_linefeed(con); break;
        case 0x0C: console_clear_screen(con); break;
        case 0x0D: console_carriage_return(con); break;
        case 0x13: console_enter_background(con); break;
        case 0x14: console_enter_foreground(con); break;
        case 0x15: console_clear_foreground(con); break;
        case 0x18: console_cursor_right(con); break;
        case 0x1A: console_cursor_up(con); break;
        case 0x1D: console_home(con); break;
        case 0x1E: console_erase_to_eol(con); break;
        case 0x1F: console_erase_to_eos(con); break;
        default: break;  /* silently ignore other control codes */
        }
        return;
    }

    /* Printable character (0x20..0xFF) */
    put_printable(con, ch);
}
