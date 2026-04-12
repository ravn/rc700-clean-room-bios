#ifndef DEBLOCK_H
#define DEBLOCK_H

#include "types.h"

/*
 * CP/M 2.2 sector deblocking algorithm.
 * Per RC702_BIOS_SPECIFICATION.md Section 10.1 and DR Alteration Guide Appendix G.
 *
 * Maps 128-byte CP/M logical sectors to larger physical sectors (256/512 bytes).
 * Maintains a single host-sector buffer with write-back caching.
 */

#define HSTBUFSZ  512   /* host sector buffer size (largest physical sector) */

/* Write types passed to deblock_write() */
#define WRTYP_DEFERRED   0   /* normal write, defer flush */
#define WRTYP_DIRECTORY  1   /* directory write, flush immediately */
#define WRTYP_UNALLOC    2   /* first sector of unallocated block, skip pre-read */

/* Physical disk I/O callbacks — provided by the floppy/hard disk driver */
typedef int (*disk_read_fn)(byte disk, word track, byte sector,
                            byte *buf);
typedef int (*disk_write_fn)(byte disk, word track, byte sector,
                             const byte *buf);

/* Deblocking state */
typedef struct {
    byte  hstbuf[HSTBUFSZ];  /* host sector buffer */
    byte  hstact;            /* host buffer active flag */
    byte  hstwrt;            /* host buffer written (dirty) flag */
    byte  hstdsk;            /* disk number in host buffer */
    word hsttrk;            /* track number in host buffer */
    byte  hstsec;            /* sector number in host buffer */

    byte  unacnt;            /* unallocated record count */
    byte  unadsk;            /* unallocated disk */
    word unatrk;            /* unallocated track */
    byte  unasec;            /* unallocated sector */

    /* Current request parameters (set by caller before read/write) */
    byte  sekdsk;            /* requested disk */
    word sektrk;            /* requested track */
    byte  seksec;            /* requested CP/M sector (0-based) */
    byte *dmaadr;            /* DMA address (128-byte buffer) */

    /* Format parameters (set by SELDSK) */
    byte  secmsk;            /* sector mask: records per phys sector - 1 */
    byte  secshf;            /* sector shift: log2(records per phys sector) + 1 */

    byte  erflag;            /* error flag (0=ok, 1=error) */

    /* I/O callbacks */
    disk_read_fn  read_fn;
    disk_write_fn write_fn;
} deblock_t;

/* Initialize deblocking state */
void deblock_init(deblock_t *db, disk_read_fn rfn, disk_write_fn wfn);

/* Perform a CP/M READ: copy 128 bytes from host buffer to dmaadr */
int deblock_read(deblock_t *db);

/* Perform a CP/M WRITE: copy 128 bytes from dmaadr to host buffer */
int deblock_write(deblock_t *db, byte wrtyp);

/* Flush dirty host buffer to disk (called at warm boot, format change, etc.) */
int deblock_flush(deblock_t *db);

#endif
