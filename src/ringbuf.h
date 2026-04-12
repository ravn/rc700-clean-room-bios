#ifndef RINGBUF_H
#define RINGBUF_H

#include "types.h"

/*
 * Generic ring buffer for keyboard (16-entry) and SIO (256-entry) use.
 * Per RC702_BIOS_SPECIFICATION.md Sections 8.2 and 9.3-9.4.
 *
 * Head is advanced by the ISR (producer).
 * Tail is advanced by the consumer (CONIN/READER).
 * Buffer is empty when head == tail.
 * Buffer is full when ((head + 1) & mask) == tail.
 */

/* Keyboard ring buffer: 16 entries */
#define KB_BUFSZ   16
#define KB_MASK    (KB_BUFSZ - 1)

/* SIO ring buffer: 256 entries */
#define SIO_BUFSZ  256
#define SIO_MASK   (SIO_BUFSZ - 1)

/* RTS flow control high-water mark (SIO-A only) */
#define SIO_A_HIGHWATER  248  /* deassert RTS when 248 bytes buffered */

typedef struct {
    byte *buf;        /* pointer to storage array */
    byte  head;       /* write index (ISR advances) */
    byte  tail;       /* read index (consumer advances) */
    byte  mask;       /* index mask (size - 1) */
} ringbuf_t;

/* Initialize a ring buffer with external storage */
void ringbuf_init(ringbuf_t *rb, byte *storage, byte mask);

/* Reset head and tail to zero */
void ringbuf_reset(ringbuf_t *rb);

/* Check if data is available (head != tail) */
int ringbuf_has_data(const ringbuf_t *rb);

/* Number of bytes currently in the buffer */
byte ringbuf_count(const ringbuf_t *rb);

/* Check if buffer is full */
int ringbuf_full(const ringbuf_t *rb);

/* Put a byte into the buffer (ISR side). Returns 0 on success, 1 if full. */
int ringbuf_put(ringbuf_t *rb, byte value);

/* Get a byte from the buffer (consumer side). Returns the byte.
 * Caller must check ringbuf_has_data() first. */
byte ringbuf_get(ringbuf_t *rb);

/* Peek at the next byte without consuming it.
 * Caller must check ringbuf_has_data() first. */
byte ringbuf_peek(const ringbuf_t *rb);

#endif
