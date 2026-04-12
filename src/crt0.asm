;
; RC702 CP/M 2.2 BIOS — crt0 (Z80 entry point)
;
; Section ordering: CODE (jump table) → code_compiler → rodata_compiler → bss_compiler
; BSS comes last so it doesn't consume disk space.
;

    ; Force section ordering by declaring them in order
    SECTION CODE
    SECTION code_compiler
    SECTION rodata_compiler
    SECTION bss_compiler

    ; ---- BIOS Jump Table at 0xDA00 ----
    SECTION CODE
    ORG 0xDA00

    jp  _bios_boot          ; +0x00 BOOT
    jp  _bios_wboot         ; +0x03 WBOOT
    jp  _bios_const         ; +0x06 CONST
    jp  _bios_conin         ; +0x09 CONIN
    jp  _bios_conout        ; +0x0C CONOUT
    jp  _bios_list          ; +0x0F LIST
    jp  _bios_punch         ; +0x12 PUNCH
    jp  _bios_reader        ; +0x15 READER
    jp  _bios_home          ; +0x18 HOME
    jp  _bios_seldsk        ; +0x1B SELDSK
    jp  _bios_settrk        ; +0x1E SETTRK
    jp  _bios_setsec        ; +0x21 SETSEC
    jp  _bios_setdma        ; +0x24 SETDMA
    jp  _bios_read          ; +0x27 READ
    jp  _bios_write         ; +0x2A WRITE
    jp  _bios_listst        ; +0x2D LISTST
    jp  _bios_sectran       ; +0x30 SECTRAN

    ; ---- JTVARS block (22 bytes at +0x33) ----
    PUBLIC _jtvars
_jtvars:
    DEFS 22

    ; ---- Padding to +0x4A ----
    DEFB 0

    ; ---- Extended entry points at +0x4A ----
    jp  _bios_wfitr         ; +0x4A WFITR
    jp  _bios_reads         ; +0x4D READS
    jp  _bios_linsel        ; +0x50 LINSEL
    jp  _bios_exit          ; +0x53 EXIT
    jp  _bios_clock         ; +0x56 CLOCK
    jp  _bios_hrdfmt        ; +0x59 HRDFMT

    ; ---- IM2 setup ----
    PUBLIC _z80_setup_im2
_z80_setup_im2:
    ld  a, 0xF6
    ld  i, a
    im  2
    ret

    ; ---- External references ----
    EXTERN _bios_boot
    EXTERN _bios_wboot
    EXTERN _bios_const
    EXTERN _bios_conin
    EXTERN _bios_conout
    EXTERN _bios_list
    EXTERN _bios_punch
    EXTERN _bios_reader
    EXTERN _bios_home
    EXTERN _bios_seldsk
    EXTERN _bios_settrk
    EXTERN _bios_setsec
    EXTERN _bios_setdma
    EXTERN _bios_read
    EXTERN _bios_write
    EXTERN _bios_listst
    EXTERN _bios_sectran
    EXTERN _bios_wfitr
    EXTERN _bios_reads
    EXTERN _bios_linsel
    EXTERN _bios_exit
    EXTERN _bios_clock
    EXTERN _bios_hrdfmt
