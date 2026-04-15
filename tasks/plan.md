# Plan: RC702 Clean Room BIOS Implementation

## Status: PAUSED — specification needs more detail on interrupt architecture

CP/M boots partially: signon banner displays, display ISR works at 50Hz,
FDC initialization begins. The system hangs waiting for the first FDC
interrupt (CTC Ch.3) to fire after RECALIBRATE. The CTC interrupt never
triggers, so fdc_wait_complete() spins forever.

## Verified correct:
- All CP/M ABI return registers (byte in A, word in HL via EX DE,HL)
- All CP/M ABI input wrappers (C→A, BC→HL)
- FDC ISR reads exactly 7 result bytes (READ) or 2 (SENSE INT) with RQM polling
- ISR preserves all registers (AF,HL,DE,BC,IX,IY), uses dedicated stack
- DPH structure layout matches CP/M 2.2 (16 bytes, correct field order)
- DPB values match working BIOS (SPT=120, BSH=4, etc.)
- SECTRAN wrapper has EX DE,HL for word return
- CCP+BDOS binary verified byte-perfect at entry via debugger dump
- SPECIFY uses CONFI defaults (0xDF=SRT 3ms, 0x28=HLT 40ms)
- Display ISR (CTC Ch.2) works throughout, never stops
- ISR wrappers end with EI+RETI (not RET or RETN)
- DMA Ch.1 programming wrapped with DI/EI (prevents flip-flop corruption)

## Blocking issue: CTC Ch.3 interrupt never fires
- CTC Ch.3 configured: counter mode, int enabled, rising edge, count=1
- Re-arming added to fdc_prepare() before each FDC command
- Display ISR works fine (CTC Ch.2) — proves IM2 vector table works
- FDC ISR wrapper exists in crt0.asm with correct EI+RETI
- Vector table entry 7 points to what appears to be the C ISR function
  directly instead of the assembly wrapper — needs investigation
- ISR complete flag stays at 0 forever
- Possible causes:
  - CTC Ch.3 CLK/TRG not connected to FDC INTRQ in MAME
  - SIO daisy chain interference (uninitialized SIO stealing RETI)
  - Vector table data not matching crt0.asm source (linker issue?)
  - CTC re-arming timing wrong (armed too early/late relative to FDC INTRQ)

## Key fixes applied this session:
1. DI/EI around DMA Ch.1 programming in dma_setup() — prevents display ISR 
   from corrupting flip-flop between two-byte register writes
2. CTC Ch.3 re-arm in fdc_prepare() before each FDC command

## Next steps (when resuming):
1. Ask working BIOS instance the 8 open questions in docs/SPECIFICATION_FEEDBACK.md
2. Investigate vector table mismatch: entry 7 should be _isr_fdc_wrapper (asm)
   but appears to point to the C function isr_floppy_complete directly
3. Check if SIO needs initialization to prevent daisy chain interference
4. Verify CTC Ch.3 CLK/TRG connection in MAME driver source (rc702.cpp)
5. Once FDC interrupt works, verify full boot to A> prompt and DIR command

## Documentation:
- docs/SPECIFICATION_FEEDBACK.md — comprehensive feedback for improving the spec
- docs/ANSWERS.md — answers from working BIOS instance
