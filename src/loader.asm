;
; RC702 BIOS init loader — loaded by ROA375 PROM to 0x0280.
;
; The PROM loads track 0 head 0 FM (26 x 128B) to address 0x0000.
; This loader at sector 6 (address 0x0280):
;   1. Copies BIOS part 1 from head 0 (0x028F onwards) to 0xDA00
;   2. Reads track 0 head 1 MFM (26 x 256B = 6656 bytes) via FDC+DMA
;   3. Jumps to 0xDA00
;
; Size budget: sectors 6-26 = 2688 bytes max.
; This loader uses ~200 bytes; rest is BIOS part 1.
;

FDC_MSR     equ 0x04
FDC_DATA    equ 0x05
DMA_MASK    equ 0xFA
DMA_MODE    equ 0xFB
DMA_CLEAR   equ 0xFC
DMA_ADDR1   equ 0xF2
DMA_COUNT1  equ 0xF3
BIOS_BASE   equ 0xDA00

    ORG 0x0280

    di

    ; ---- Copy head 0 BIOS data to BIOS_BASE ----
    ld  hl, bios_data_start
    ld  de, BIOS_BASE
    ld  bc, bios_data_end - bios_data_start
    ldir

    ; ---- Read head 1 (26 sectors x 256B) via FDC ----
    ; Destination starts at BIOS_BASE + (bios_data_end - bios_data_start)
    ; which is where the head 0 data ends.
    ld  hl, BIOS_BASE
    ld  bc, bios_data_end - bios_data_start
    add hl, bc                  ; HL = dest for head 1 data

    ld  c, 1                    ; C = current sector (1..26)
    ld  b, 26                   ; B = sectors remaining

.sector_loop:
    push bc                     ; save counter + sector
    push hl                     ; save dest address

    ; -- DMA setup --
    ld  a, 0x05
    out (DMA_MASK), a           ; mask ch 1
    ld  a, 0x45
    out (DMA_MODE), a           ; IO-to-mem, ch 1
    xor a
    out (DMA_CLEAR), a          ; clear flip-flop
    ld  a, l
    out (DMA_ADDR1), a          ; addr low
    ld  a, h
    out (DMA_ADDR1), a          ; addr high
    ld  a, 255                  ; count = 255 (transfers 256 bytes)
    out (DMA_COUNT1), a
    xor a
    out (DMA_COUNT1), a         ; count high = 0
    ld  a, 0x01
    out (DMA_MASK), a           ; unmask ch 1

    ; -- FDC READ DATA command (9 bytes) --
    ; MFM read, head 1, drive 0, cylinder 0, sector C, N=1, EOT=26
    pop  hl                     ; restore dest (not needed now, but balanced)
    push hl
    pop  hl
    ; Get sector number from saved BC
    pop  bc                     ; B=remaining, C=sector
    push bc                     ; save again

    ld  a, 0x46                 ; READ DATA + MFM
    call fdc_out
    ld  a, 0x04                 ; head=1, drive=0  (bit2=head)
    call fdc_out
    xor a                       ; C = cylinder 0
    call fdc_out
    ld  a, 1                    ; H = head 1
    call fdc_out
    ld  a, c                    ; R = sector number
    call fdc_out
    ld  a, 1                    ; N = 1 (256 bytes/sector)
    call fdc_out
    ld  a, 26                   ; EOT = 26
    call fdc_out
    ld  a, 14                   ; GPL = 14
    call fdc_out
    ld  a, 0xFF                 ; DTL
    call fdc_out

    ; -- Wait for and read 7 result bytes --
    call fdc_in                 ; ST0
    call fdc_in                 ; ST1
    call fdc_in                 ; ST2
    call fdc_in                 ; C
    call fdc_in                 ; H
    call fdc_in                 ; R
    call fdc_in                 ; N

    ; -- Advance to next sector --
    pop  bc                     ; B=remaining, C=sector
    pop  hl                     ; dest address
    ld   de, 256
    add  hl, de                 ; advance dest by 256
    inc  c                      ; next sector
    djnz .sector_loop

    ; ---- Jump to BIOS ----
    jp  BIOS_BASE

; ---- FDC I/O subroutines ----

fdc_out:
    ; Send byte in A to FDC data port. Wait for RQM=1, DIO=0.
    push af
.fdc_out_wait:
    in   a, (FDC_MSR)
    and  0xC0
    cp   0x80
    jr   nz, .fdc_out_wait
    pop  af
    out  (FDC_DATA), a
    ret

fdc_in:
    ; Read byte from FDC data port. Wait for RQM=1, DIO=1.
.fdc_in_wait:
    in   a, (FDC_MSR)
    and  0xC0
    cp   0xC0
    jr   nz, .fdc_in_wait
    in   a, (FDC_DATA)
    ret

; ---- BIOS data from head 0 starts here ----
; make-disk.py fills this with the first portion of the BIOS binary.
bios_data_start:
    ; (filled by make-disk.py)
bios_data_end:
    ; (end marker — make-disk.py calculates the actual size)
