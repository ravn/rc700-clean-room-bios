;
; RC702 BIOS loader stub — loaded by ROA375 PROM to low memory.
;
; The PROM reads track 0 sectors 6-26 (head 0, FM 128B) and
; sectors 1-26 (head 1, MFM 256B) into contiguous memory starting
; at the entry address from boot sector bytes 0-1.
;
; This stub is the first code loaded. It:
;   1. Disables interrupts
;   2. Copies the BIOS binary from its load position to 0xDA00
;   3. Jumps to 0xDA00 (BIOS cold boot entry)
;
; The BIOS binary follows immediately after this stub in the
; loaded data. Stub size must be accounted for in the copy.
;

    ORG 0x0280

    di

    ; Copy BIOS from (load_addr + stub_size) to 0xDA00
    ; Source: just past this stub in loaded memory
    ld  hl, bios_data       ; source = load address + stub size
    ld  de, 0xDA00          ; destination = BIOS_BASE
    ld  bc, BIOS_SIZE       ; byte count (patched by make-disk.py)
    ldir

    ; Jump to BIOS cold boot
    jp  0xDA00

bios_data:
    ; BIOS binary data follows here (placed by make-disk.py)

; BIOS_SIZE is a placeholder — make-disk.py patches the actual size
BIOS_SIZE equ 9344          ; max track 0 capacity minus stub
