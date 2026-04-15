# Plan: RC702 Clean Room BIOS Implementation

## Status: CTC Ch.3 interrupt delivery is the last blocker

CP/M boots, CCP+BDOS loads correctly, BDOS starts directory reads.
The FDC interrupt (CTC Ch.3) stops firing after ~12-105 operations,
hanging the directory read. With debug display overhead (function
call-based memory writes), CTC works reliably for hundreds of ops.

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

## Open issue: CTC Ch.3 stops firing
- Works with debug display overhead (~50 function calls per FDC op)
- Fails without overhead after ~12-105 operations
- Not timing delays (DJNZ, hal_ei loops don't help)  
- Not SPECIFY values (CONFI defaults don't help)
- Not FDC result reading (ISR reads correct bytes with RQM+CB checks)
- MAME's CTC counter auto-reloads correctly (verified in source)
- May be MAME Z80 CTC daisy chain emulation artifact
- The working BIOS avoids this due to different code structure/timing

## Investigate:
- Ask working BIOS instance if there's any SIO initialization needed
  to prevent SIO from stealing RETI acknowledgments in daisy chain
- Check if MAME's CTC interrupt scheduling has known timing issues
- Consider implementing the BIOS as closer match to working BIOS
  structure (more code between FDC operations)
