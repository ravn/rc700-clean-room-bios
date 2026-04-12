#include <assert.h>
#include <stdio.h>
#include "ringbuf.h"

static byte kb_storage[KB_BUFSZ];
static byte sio_storage[SIO_BUFSZ];

static void test_init(void) {
    ringbuf_t rb;
    ringbuf_init(&rb, kb_storage, KB_MASK);
    assert(!ringbuf_has_data(&rb));
    assert(ringbuf_count(&rb) == 0);
    assert(!ringbuf_full(&rb));
}

static void test_put_get(void) {
    ringbuf_t rb;
    ringbuf_init(&rb, kb_storage, KB_MASK);

    assert(ringbuf_put(&rb, 0x41) == 0);
    assert(ringbuf_has_data(&rb));
    assert(ringbuf_count(&rb) == 1);

    byte val = ringbuf_get(&rb);
    assert(val == 0x41);
    assert(!ringbuf_has_data(&rb));
    assert(ringbuf_count(&rb) == 0);
}

static void test_peek(void) {
    ringbuf_t rb;
    ringbuf_init(&rb, kb_storage, KB_MASK);

    ringbuf_put(&rb, 0x42);
    assert(ringbuf_peek(&rb) == 0x42);
    assert(ringbuf_has_data(&rb));  /* peek doesn't consume */
    assert(ringbuf_get(&rb) == 0x42);
}

static void test_fifo_order(void) {
    ringbuf_t rb;
    ringbuf_init(&rb, kb_storage, KB_MASK);

    for (byte i = 0; i < 10; i++)
        ringbuf_put(&rb, (byte)('A' + i));

    assert(ringbuf_count(&rb) == 10);

    for (byte i = 0; i < 10; i++)
        assert(ringbuf_get(&rb) == (byte)('A' + i));

    assert(!ringbuf_has_data(&rb));
}

static void test_kb_full(void) {
    ringbuf_t rb;
    ringbuf_init(&rb, kb_storage, KB_MASK);

    /* Fill to capacity (size-1 entries usable) */
    for (int i = 0; i < KB_BUFSZ - 1; i++)
        assert(ringbuf_put(&rb, (byte)i) == 0);

    assert(ringbuf_full(&rb));
    assert(ringbuf_count(&rb) == KB_BUFSZ - 1);

    /* Next put should fail (discard) */
    assert(ringbuf_put(&rb, 0xFF) == 1);

    /* Verify data intact */
    for (int i = 0; i < KB_BUFSZ - 1; i++)
        assert(ringbuf_get(&rb) == (byte)i);
}

static void test_sio_256(void) {
    ringbuf_t rb;
    ringbuf_init(&rb, sio_storage, SIO_MASK);

    /* Fill to capacity */
    for (int i = 0; i < SIO_BUFSZ - 1; i++)
        assert(ringbuf_put(&rb, (byte)i) == 0);

    assert(ringbuf_full(&rb));
    assert(ringbuf_count(&rb) == SIO_BUFSZ - 1);

    /* Verify data */
    for (int i = 0; i < SIO_BUFSZ - 1; i++)
        assert(ringbuf_get(&rb) == (byte)i);

    assert(!ringbuf_has_data(&rb));
}

static void test_wrap_around(void) {
    ringbuf_t rb;
    ringbuf_init(&rb, kb_storage, KB_MASK);

    /* Put and get several times to advance head/tail past the wrap point */
    for (int round = 0; round < 5; round++) {
        for (int i = 0; i < 10; i++)
            ringbuf_put(&rb, (byte)(round * 10 + i));
        for (int i = 0; i < 10; i++)
            assert(ringbuf_get(&rb) == (byte)(round * 10 + i));
    }
    assert(!ringbuf_has_data(&rb));
}

static void test_reset(void) {
    ringbuf_t rb;
    ringbuf_init(&rb, kb_storage, KB_MASK);

    ringbuf_put(&rb, 0x01);
    ringbuf_put(&rb, 0x02);
    assert(ringbuf_count(&rb) == 2);

    ringbuf_reset(&rb);
    assert(!ringbuf_has_data(&rb));
    assert(ringbuf_count(&rb) == 0);
}

static void test_highwater(void) {
    ringbuf_t rb;
    ringbuf_init(&rb, sio_storage, SIO_MASK);

    /* Fill to just below high-water mark */
    for (int i = 0; i < SIO_A_HIGHWATER - 1; i++)
        ringbuf_put(&rb, (byte)i);
    assert(ringbuf_count(&rb) == SIO_A_HIGHWATER - 1);
    assert(ringbuf_count(&rb) < SIO_A_HIGHWATER);

    /* One more reaches high-water */
    ringbuf_put(&rb, 0xFF);
    assert(ringbuf_count(&rb) >= SIO_A_HIGHWATER);
}

int main(void) {
    test_init();
    test_put_get();
    test_peek();
    test_fifo_order();
    test_kb_full();
    test_sio_256();
    test_wrap_around();
    test_reset();
    test_highwater();
    printf("All ringbuf tests passed.\n");
    return 0;
}
