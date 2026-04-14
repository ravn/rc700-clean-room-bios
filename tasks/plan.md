# Plan: RC702 Clean Room BIOS Implementation

## Completed Phases

### Phase 1 — Project scaffold (DONE)
1. [x] CMake + CLion preset, HAL abstraction, test harness
2. [x] `iobyte.c` + `test_iobyte.c`

### Phase 2 — Pure logic modules (DONE)
3. [x] `sector.c` + `test_sector.c` — sector translation tables
4. [x] `dpb.c` + `test_dpb.c` — DPB/FSPA/FDF structures
5. [x] `config.c` + `test_config.c` — CONFI block parsing, baud rate tables

### Phase 3 — Stateful logic (DONE)
6. [x] `console.c` + `test_console.c` — cursor, control chars, scrolling, XY escape
7. [x] `deblock.c` + `test_deblock.c` — sector deblocking algorithm
8. [x] `chartab.c` + `test_chartab.c` — character conversion tables

### Phase 4 — Drivers and Z80 target (DONE)
9. [x] Ring buffers, serial driver, floppy driver
10. [x] Hardware init, interrupt handlers, JTVARS
11. [x] Z80 build with crt0.asm jump table
12. [x] FDC simulator + IMD reader for native testing
13. [x] Warm boot test verified byte-for-byte against real disk image

### Phase 5 — MAME integration (IN PROGRESS)
14. [x] BIOS boots in MAME and displays signon message
15. [x] Display refresh ISR working (fast assembly in crt0.asm)
16. [x] sdcccall(1) calling convention
17. [x] CCP+BDOS assembled for CCP=0xAA00, BDOS=0xB200
18. [x] CCP+BDOS written to track 1
19. [x] Warm boot code wired
20. [x] Two-phase loader (head 0 + head 1)
21. [x] hal_in/hal_out fixed (return clobber, B register, __sfr for FDC/DMA)
22. [x] Background bitmap stubbed (-DCONSOLE_NO_BGMAP)
23. [x] FDC interrupt-driven via CTC Ch.3 — ISR fires, recalibrate/seek/read complete
24. [x] Display stable during FDC operations (hal_ei in wait loops)
25. [x] Warm boot reaches CCP at 0xAA00 (confirmed by execution trace)
26. [ ] Fix DMA data corruption (#7) — **blocking A> prompt**
27. [ ] Fix ISR stack leak (#8)
28. [ ] Keyboard input from MAME
29. [ ] CP/M `A>` prompt working in MAME

## Known Issues / Bugs Found

### Port I/O forms (RESOLVED)
Z80 `IN A,(C)` / `OUT (C),A` use B:C as 16-bit address. With SDCC-generated
code, B is unpredictable. All FDC and DMA port I/O now uses `__sfr __at()`
which generates `IN A,(n)` / `OUT (n),A` (fixed 8-bit port address).
`hal_in`/`hal_out` have `LD B,#0` for other ports.

### SDCC __interrupt attribute (RESOLVED)
- `__interrupt` generates `EI` at entry + `RETI` at exit (nesting, but correct RETI)
- `__critical __interrupt` generates `RETN` at exit (WRONG for IM2 daisy chain)
- Solution: assembly wrappers in crt0.asm with manual EI+RETI

### ISR RETI requirement (RESOLVED)
All ISRs must end with RETI (ED 4D), not RET (C9) or RETN (ED 45).
Without RETI, higher-priority CTC channels block lower-priority ones
in the Z80 daisy chain. CTC Ch.2 (display) blocked Ch.3 (floppy) when
the floppy ISR used RET.

## Open Issues

- #3 8275 CRT parameters differ between PROM and CONFI defaults
- #4 BIOS too large for 0xDA00 (target ~5.2KB, currently ~8.3KB)
- #5 Automate disk image build
- #7 **DMA data corruption** — FDC reads complete but bytes in memory are wrong.
     CCP at 0xAA00 should start with C3 50 AD (JP 0xAD50) but contains
     C3 5C C7 (JP 0xC75C). DMA channel 1 setup may have wrong address,
     count, or mode. The `dma_setup` function uses `__sfr` for port writes.
     Native test_warmboot verifies byte-for-byte correctly, so the issue
     is specific to the Z80/MAME DMA controller interaction.
- #8 **ISR stack leak** — SP grows by 2 bytes per FDC ISR invocation.
     Entry/exit SP within the ISR wrapper is balanced, but mainline SP
     increments. Root cause unknown — may be related to the display ISR
     nesting via EI at entry, or an SDCC function prologue issue.

## Investigate Later
- Move BIOS to 0xDA00 for standard CP/M memory map
- Compiler inlining with zcc flags
- Switch statement Z80 code quality
- z88dk-gdb for Z80 debugging
- Re-enable SIO/PIO ISRs with proper asm wrappers
- Background bitmap (CONSOLE_NO_BGMAP)
