#include "ringbuf.h"

void ringbuf_init(ringbuf_t *rb, byte *storage, byte mask) {
    rb->buf = storage;
    rb->head = 0;
    rb->tail = 0;
    rb->mask = mask;
}

void ringbuf_reset(ringbuf_t *rb) {
    rb->head = 0;
    rb->tail = 0;
}

int ringbuf_has_data(const ringbuf_t *rb) {
    return rb->head != rb->tail;
}

byte ringbuf_count(const ringbuf_t *rb) {
    return (byte)((rb->head - rb->tail) & rb->mask);
}

int ringbuf_full(const ringbuf_t *rb) {
    return ((rb->head + 1) & rb->mask) == rb->tail;
}

int ringbuf_put(ringbuf_t *rb, byte value) {
    byte next = (rb->head + 1) & rb->mask;
    if (next == rb->tail)
        return 1;  /* full — discard */
    rb->buf[rb->head] = value;
    rb->head = next;
    return 0;
}

byte ringbuf_get(ringbuf_t *rb) {
    byte value = rb->buf[rb->tail];
    rb->tail = (rb->tail + 1) & rb->mask;
    return value;
}

byte ringbuf_peek(const ringbuf_t *rb) {
    return rb->buf[rb->tail];
}
