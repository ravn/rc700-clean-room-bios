#ifndef IMD_H
#define IMD_H

#include "types.h"

/*
 * IMD (ImageDisk) format reader for test harness.
 *
 * Reads .imd files into a structured in-memory representation that
 * the FDC simulator can use. Supports the RC702 disk formats:
 * - Track 0 head 0: FM, 26 sectors × 128 bytes
 * - Track 0 head 1: MFM, 26 sectors × 256 bytes
 * - Tracks 1-76: MFM, 15 sectors × 512 bytes per side
 */

#define IMD_MAX_TRACKS   77
#define IMD_MAX_HEADS     2
#define IMD_MAX_SECTORS  26
#define IMD_MAX_SECSIZE  512

/* Per-track info */
typedef struct {
    byte  mode;         /* 0=FM 500kbps, 3=MFM 500kbps, etc. */
    byte  cylinder;
    byte  head;
    byte  num_sectors;
    int   sector_size;  /* bytes per sector */
    byte  sector_map[IMD_MAX_SECTORS];  /* physical sector numbering */
    byte  data[IMD_MAX_SECTORS][IMD_MAX_SECSIZE];  /* sector data */
} imd_track_t;

/* Complete disk image */
typedef struct {
    int num_tracks;  /* total track records in file */
    imd_track_t tracks[IMD_MAX_TRACKS][IMD_MAX_HEADS];
    int max_cylinder;
    int num_heads;
} imd_disk_t;

/* Load an IMD file. Returns 0 on success, nonzero on error. */
int imd_load(imd_disk_t *disk, const char *filename);

/* Get sector data for a given C/H/R (1-based sector number).
 * Returns pointer to sector data, or NULL if not found. */
byte *imd_get_sector(imd_disk_t *disk, int cyl, int head, int sector_1based);

/* Get sector size for a given track */
int imd_get_sector_size(imd_disk_t *disk, int cyl, int head);

/* Get number of sectors on a track */
int imd_get_num_sectors(imd_disk_t *disk, int cyl, int head);

#endif
