#include "deblock.h"
#include <string.h>

void deblock_init(deblock_t *db, disk_read_fn rfn, disk_write_fn wfn) {
    memset(db, 0, sizeof(*db));
    db->read_fn = rfn;
    db->write_fn = wfn;
}

int deblock_flush(deblock_t *db) {
    if (db->hstwrt) {
        db->hstwrt = 0;
        if (db->write_fn(db->hstdsk, db->hsttrk, db->hstsec,
                         db->hstbuf) != 0) {
            db->erflag = 1;
            return 1;
        }
    }
    return 0;
}

/*
 * Ensure the host buffer contains the sector for the current request.
 * If a different sector is buffered, flush it first, then read the new one.
 */
static int ensure_host_sector(deblock_t *db, int need_preread) {
    /* Compute host sector from CP/M sector */
    uint8_t host_sec = db->seksec >> db->secshf;
    uint8_t rec_offset = db->seksec & db->secmsk;

    /* Check if already in buffer */
    if (db->hstact &&
        db->hstdsk == db->sekdsk &&
        db->hsttrk == db->sektrk &&
        db->hstsec == host_sec) {
        /* Already buffered — just return the offset */
        (void)rec_offset;
        return 0;
    }

    /* Different sector needed — flush any dirty buffer */
    if (deblock_flush(db) != 0)
        return 1;

    /* Read new sector if pre-read is needed */
    db->hstdsk = db->sekdsk;
    db->hsttrk = db->sektrk;
    db->hstsec = host_sec;
    db->hstact = 1;
    db->hstwrt = 0;

    if (need_preread) {
        if (db->read_fn(db->hstdsk, db->hsttrk, db->hstsec,
                        db->hstbuf) != 0) {
            db->hstact = 0;
            db->erflag = 1;
            return 1;
        }
    }

    return 0;
}

int deblock_read(deblock_t *db) {
    db->erflag = 0;

    if (ensure_host_sector(db, 1) != 0)
        return 1;

    /* Copy 128-byte record from host buffer to DMA address */
    uint8_t rec_offset = db->seksec & db->secmsk;
    uint16_t buf_offset = (uint16_t)rec_offset * 128;
    memcpy(db->dmaadr, &db->hstbuf[buf_offset], 128);

    return 0;
}

int deblock_write(deblock_t *db, uint8_t wrtyp) {
    db->erflag = 0;

    if (wrtyp == WRTYP_UNALLOC) {
        /* First write to unallocated block — track sequential writes */
        db->unacnt = (db->secmsk + 1);  /* records per physical sector */
        db->unadsk = db->sekdsk;
        db->unatrk = db->sektrk;
        db->unasec = db->seksec;
    }

    int need_preread = 1;

    if (db->unacnt > 0) {
        /* Check if this write is part of the unallocated sequence */
        if (db->sekdsk == db->unadsk &&
            db->sektrk == db->unatrk &&
            db->seksec == db->unasec) {
            db->unacnt--;
            db->unasec++;
            /* No pre-read needed for unallocated sequential writes */
            need_preread = 0;
        } else {
            /* Sequence broken */
            db->unacnt = 0;
        }
    }

    if (ensure_host_sector(db, need_preread) != 0)
        return 1;

    /* Copy 128-byte record from DMA address to host buffer */
    uint8_t rec_offset = db->seksec & db->secmsk;
    uint16_t buf_offset = (uint16_t)rec_offset * 128;
    memcpy(&db->hstbuf[buf_offset], db->dmaadr, 128);
    db->hstwrt = 1;

    /* Directory writes flush immediately */
    if (wrtyp == WRTYP_DIRECTORY) {
        if (deblock_flush(db) != 0)
            return 1;
    }

    return 0;
}
