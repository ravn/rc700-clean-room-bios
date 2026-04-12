#include "imd.h"
#include <stdio.h>
#include <string.h>

int imd_load(imd_disk_t *disk, const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) return -1;

    memset(disk, 0, sizeof(*disk));

    /* Skip header (terminated by 0x1A) */
    int ch;
    while ((ch = fgetc(f)) != EOF) {
        if (ch == 0x1A) break;
    }
    if (ch == EOF) { fclose(f); return -1; }

    /* Parse track records */
    int track_count = 0;
    int max_cyl = 0;
    int max_head = 0;

    while (1) {
        /* Read track header: mode, cylinder, head, nsec, secsize_code */
        byte hdr[5];
        if (fread(hdr, 1, 5, f) != 5) break;

        byte mode = hdr[0];
        byte cyl = hdr[1];
        byte head = hdr[2] & 0x01;  /* bit 0 = head, upper bits = flags */
        byte nsec = hdr[3];
        byte secsize_code = hdr[4];
        int secsize = 128 << secsize_code;

        if (cyl >= IMD_MAX_TRACKS || head >= IMD_MAX_HEADS) {
            fclose(f);
            return -2;
        }
        if (nsec > IMD_MAX_SECTORS || secsize > IMD_MAX_SECSIZE) {
            fclose(f);
            return -3;
        }

        imd_track_t *trk = &disk->tracks[cyl][head];
        trk->mode = mode;
        trk->cylinder = cyl;
        trk->head = head;
        trk->num_sectors = nsec;
        trk->sector_size = secsize;

        /* Read sector numbering map */
        if (fread(trk->sector_map, 1, nsec, f) != nsec) {
            fclose(f);
            return -4;
        }

        /* Read sector data records */
        for (int s = 0; s < nsec; s++) {
            int rec_type = fgetc(f);
            if (rec_type == EOF) { fclose(f); return -5; }

            switch (rec_type) {
            case 0:
                /* Unavailable — fill with 0xE5 */
                memset(trk->data[s], 0xE5, (size_t)secsize);
                break;
            case 1:
                /* Normal data */
                if (fread(trk->data[s], 1, (size_t)secsize, f) != (size_t)secsize) {
                    fclose(f);
                    return -6;
                }
                break;
            case 2: {
                /* Compressed: single fill byte */
                int fill = fgetc(f);
                if (fill == EOF) { fclose(f); return -7; }
                memset(trk->data[s], fill, (size_t)secsize);
                break;
            }
            case 3:
                /* Normal data, deleted mark — treat as normal */
                if (fread(trk->data[s], 1, (size_t)secsize, f) != (size_t)secsize) {
                    fclose(f);
                    return -6;
                }
                break;
            case 4: {
                /* Compressed, deleted mark */
                int fill = fgetc(f);
                if (fill == EOF) { fclose(f); return -7; }
                memset(trk->data[s], fill, (size_t)secsize);
                break;
            }
            default:
                /* Unknown record type — skip data-sized chunk for types 5-8 */
                if (rec_type & 1)
                    fseek(f, secsize, SEEK_CUR);
                else
                    fgetc(f);
                memset(trk->data[s], 0xE5, (size_t)secsize);
                break;
            }
        }

        if (cyl > max_cyl) max_cyl = cyl;
        if (head > max_head) max_head = head;
        track_count++;
    }

    fclose(f);
    disk->num_tracks = track_count;
    disk->max_cylinder = max_cyl;
    disk->num_heads = max_head + 1;
    return 0;
}

byte *imd_get_sector(imd_disk_t *disk, int cyl, int head, int sector_1based) {
    if (cyl >= IMD_MAX_TRACKS || head >= IMD_MAX_HEADS) return NULL;
    imd_track_t *trk = &disk->tracks[cyl][head];
    if (trk->num_sectors == 0) return NULL;

    /* Find sector by its physical number in the sector map */
    for (int i = 0; i < trk->num_sectors; i++) {
        if (trk->sector_map[i] == sector_1based)
            return trk->data[i];
    }
    return NULL;
}

int imd_get_sector_size(imd_disk_t *disk, int cyl, int head) {
    if (cyl >= IMD_MAX_TRACKS || head >= IMD_MAX_HEADS) return 0;
    return disk->tracks[cyl][head].sector_size;
}

int imd_get_num_sectors(imd_disk_t *disk, int cyl, int head) {
    if (cyl >= IMD_MAX_TRACKS || head >= IMD_MAX_HEADS) return 0;
    return disk->tracks[cyl][head].num_sectors;
}
