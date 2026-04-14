# Plan: RC702 Clean Room BIOS Implementation

## Completed Phases

### Phase 1-4 (DONE)
Project scaffold, pure logic modules, stateful logic, drivers, Z80 target.
Full test harness with 16 native tests passing.

### Phase 5 — MAME integration (IN PROGRESS)
14. [x] BIOS boots in MAME, displays signon
15. [x] Display refresh ISR (fast asm in crt0.asm)
16. [x] sdcccall(1) calling convention
17. [x] CCP+BDOS assembled (CCP=0xAA00, BDOS=0xB200)
18. [x] CCP+BDOS written to track 1 with interleave
19. [x] Warm boot loads CCP+BDOS (verified byte-perfect via debugger)
20. [x] Two-phase loader (head 0 + head 1)
21. [x] hal_in/hal_out fixed (__sfr for FDC/DMA/CTC ports)
22. [x] Background bitmap stubbed (-DCONSOLE_NO_BGMAP)
23. [x] FDC interrupt-driven via CTC Ch.3 (ISR wrapper with RETI)
24. [x] CP/M→sdcccall(1) register wrappers in jump table
25. [x] sector_shift fix (3→2 for 512B sectors)
26. [x] **CP/M boots and prints to screen** ("Bdos Err On A: Bad Sector")
27. [ ] Fix directory read errors (#9) — **next priority**
28. [ ] CP/M `A>` prompt
29. [ ] Keyboard input

## Open Issues

- #3 8275 CRT parameters differ between PROM and CONFI defaults
- #4 BIOS too large for 0xDA00 (target ~5.2KB)
- #5 Automate disk image build
- #8 ISR stack leak (+2 bytes per FDC ISR, cause unknown)
- #9 **Directory read fails** — CCP reads directory on track 2+,
     BDOS reports "Bad Sector". DPB off=2 means directory starts
     on track 2. FDC reads track 1 (CCP) fine but track 2+ fails.
     May be sector translation, DPB parameters, or head selection.

## Bugs Found & Fixed This Session
- hal_in `return 0;` clobbered port read result
- OUT (C),A / IN A,(C) use B:C — need __sfr for IN A,(n) / OUT (n),A
- __critical __interrupt generates RETN (wrong), need asm wrapper with RETI
- ISR without RETI blocks CTC daisy chain
- sector_shift=3 should be 2 for 512B sectors
- CP/M register convention (C/BC) vs sdcccall(1) (A/HL) mismatch
- make-disk.py didn't write CCP+BDOS to track 1
- CCP+BDOS needed interleave (tran8) on track 1
- IOBYTE_JOINED hangs on SIO-B serial output
