;
; RC702 CP/M 2.2 BIOS — crt0 (Z80 entry point)
;
; Section ordering: CODE (jump table) → code_compiler → rodata_compiler → bss_compiler
; BSS comes last so it doesn't consume disk space.
;

    ; Force section ordering: all code/rodata/library before BSS
    SECTION CODE
    SECTION code_compiler
    SECTION code_string
    SECTION code_l_sccz80
    SECTION code_l_sdcc
    SECTION code_math
    SECTION code_error
    SECTION rodata_compiler
    SECTION bss_compiler
    SECTION bss_error

    ; ---- BIOS Jump Table at 0xDA00 ----
    SECTION CODE
    ORG 0xC000      ; temporary - will be 0xDA00 when code is optimized

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
    ; Build vector table at 0xF600
    ld  hl, _isr_vector_data
    ld  de, 0xF600
    ld  bc, 36             ; 18 entries x 2 bytes
    ldir

    ; Set I register and enable IM2
    ld  a, 0xF6
    ld  i, a
    im  2
    ret

    ; ---- ISR Vector Table Data (18 entries x 2 bytes) ----
    ; C ISR handlers use __interrupt keyword (SDCC generates RETI + reg saves).
    ; Vector table layout must match device interrupt vector bases:
    ;   PIO:  vectors 0x02, 0x04 (PIO base set to 0x02 by PROM)
    ;   CTC:  vectors 0x08, 0x0A, 0x0C, 0x0E (CTC base set to 0x08 by PROM)
    ;   SIO:  vectors 0x10-0x1E (SIO-B WR2 base = 0x10)
    ;   PIO keyboard: vector 0x02
    ;
    ; Table index = vector_byte / 2
_isr_vector_data:
    DEFW _isr_dummy              ;  0: (unused)
    DEFW _isr_keyboard_handler   ;  1: PIO Ch.A (keyboard) vector=0x02
    DEFW _isr_dummy              ;  2: PIO Ch.B vector=0x04
    DEFW _isr_dummy              ;  3: (unused)
    DEFW _isr_dummy              ;  4: CTC Ch.0 (baud rate) vector=0x08
    DEFW _isr_dummy              ;  5: CTC Ch.1 (baud rate) vector=0x0A
    DEFW _isr_crt_fast           ;  6: CTC Ch.2 (display) vector=0x0C
    DEFW _isr_floppy_complete    ;  7: CTC Ch.3 (floppy) vector=0x0E
    DEFW _isr_sio_b_tx           ;  8: SIO Ch.B TX vector=0x10
    DEFW _isr_sio_b_ext          ;  9: SIO Ch.B EXT vector=0x12
    DEFW _isr_sio_b_rx           ; 10: SIO Ch.B RX vector=0x14
    DEFW _isr_sio_b_spec         ; 11: SIO Ch.B SPEC vector=0x16
    DEFW _isr_sio_a_tx           ; 12: SIO Ch.A TX vector=0x18
    DEFW _isr_sio_a_ext          ; 13: SIO Ch.A EXT vector=0x1A
    DEFW _isr_sio_a_rx           ; 14: SIO Ch.A RX vector=0x1C
    DEFW _isr_sio_a_spec         ; 15: SIO Ch.A SPEC vector=0x1E
    DEFW _isr_dummy              ; 16: PIO Ch.A alt (keyboard) vector=0x20
    DEFW _isr_dummy              ; 17: PIO Ch.B alt vector=0x22

    ; ---- Fast display refresh ISR (replaces C version) ----
    ; Must reprogram DMA Ch2 immediately after 8275 VRTC interrupt.
    ; Matches the PROM's DISINT routine — all inline, no C calls.
_isr_crt_fast:
    push af
    in   a, (001H)          ; read 8275 status to acknowledge interrupt

    push hl
    push de
    push bc

    ; Mask DMA channels 2 and 3
    ld   a, 006H
    out  (0FAH), a          ; mask ch.2
    ld   a, 007H
    out  (0FAH), a          ; mask ch.3
    out  (0FCH), a          ; clear byte pointer flip-flop (value ignored)

    ; Ch2 address = 0xF800 (display buffer)
    ld   a, 000H
    out  (0F4H), a          ; ch.2 addr low
    ld   a, 0F8H
    out  (0F4H), a          ; ch.2 addr high

    ; Ch2 word count = 1999
    ld   a, 0CFH
    out  (0F5H), a          ; ch.2 count low
    ld   a, 007H
    out  (0F5H), a          ; ch.2 count high

    ; Unmask ch.2
    ld   a, 002H
    out  (0FAH), a

    ; Re-arm CTC Ch.2
    ld   a, 0D7H
    out  (00EH), a          ; counter mode, int enabled
    ld   a, 001H
    out  (00EH), a          ; count = 1

    pop  bc
    pop  de
    pop  hl
    pop  af
    ei
    reti

    ; ---- Dummy ISR for unused vectors ----
_isr_dummy:
    ei
    reti

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

    ; ISR C handlers (use __interrupt for RETI + register saves)
    EXTERN _isr_display_refresh
    EXTERN _isr_floppy_complete
    EXTERN _isr_keyboard_handler
    EXTERN _isr_sio_b_tx
    EXTERN _isr_sio_b_ext
    EXTERN _isr_sio_b_rx
    EXTERN _isr_sio_b_spec
    EXTERN _isr_sio_a_tx
    EXTERN _isr_sio_a_ext
    EXTERN _isr_sio_a_rx
    EXTERN _isr_sio_a_spec
