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
6. [x] `console.c` + `test_console.c` — cursor, control chars, scrolling, XY escape, background bitmap
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
16. [x] sdcccall(1) calling convention — 1,105 bytes saved
17. [x] CCP+BDOS assembled for CCP=0xAA00, BDOS=0xB200
18. [x] CCP+BDOS written to track 1
19. [x] Warm boot code wired (reads track 1, installs vectors, jumps to CCP)
20. [ ] FDC interrupt-driven completion (#2) — **blocking warm boot**
21. [ ] Keyboard input from MAME
22. [ ] CP/M `A>` prompt working in MAME

## Known Issues

### SDCC code generation bug
SDCC (z88dk zsdcc) generates incorrect code for inline expressions
passed as function parameters:
```c
hal_out(0xF4, (byte)(disp_addr >> 8));  // HIGH BYTE LOST
```
The shift result is dropped during stack-based parameter passing.
Workaround: pre-compute into local variables before calling.
TODO: File upstream bug report.

### CONFI PAR3/PAR4 values differ from PROM
The CONFI default CRT parameters (PAR3=0x7A, PAR4=0x6D) differ from
what the PROM uses (PAR3=0x9A, PAR4=0x5D). The PROM values work;
the CONFI values break the display. Need to investigate which is
correct for the physical hardware. Currently skipping 8275 reset
and relying on PROM's configuration.

### BIOS too large for 0xDA00
BIOS code+rodata = 8,662 bytes (with sdcccall(1)). Original BIOS
fits in ~5KB at 0xDA00. Currently using 0xC000 as temporary base.
Need to optimize to fit at 0xDA00 for standard CP/M memory map.

### hw_init_all disabled
Hardware initialization (PIO/CTC/SIO/DMA/FDC/8275) is currently
skipped because the PROM already configured everything. Need to
enable for standalone boot (without PROM).

## Open Issues

- #1 SDCC code generation bug (inline shift in params) — workaround applied
- #2 Floppy driver needs interrupt-driven completion — **next priority**
- #3 8275 CRT parameters differ between PROM and CONFI defaults
- #4 BIOS too large for 0xDA00 (8.8KB vs 5.2KB target)
- #5 Automate disk image build

## Investigate Later
- Compiler inlining with zcc flags
- Switch statement Z80 code quality
- z88dk-gdb for Z80 debugging
