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
20. [x] Two-phase loader (head 0 + head 1 data from track 0)
21. [x] hal_in bug fixed (return 0 clobbered port read result)
22. [x] Background bitmap stubbed out (-DCONSOLE_NO_BGMAP) — 355 bytes saved
23. [x] FDC recalibrate/seek with polling (wait_rqm + SENSE INTERRUPT)
24. [x] Display stable during FDC polling (hal_ei in wait loops)
25. [ ] FDC READ DATA command — **blocking warm boot** (#6)
26. [ ] Keyboard input from MAME
27. [ ] CP/M `A>` prompt working in MAME

## Known Issues

### hal_in sdcccall(1) return convention (#1 resolved)
The original `hal_in` had `return 0;` which generated `LD A,#0` — clobbering
the IN result. Fixed by using `IN A,(C)` + `LD L,A` with no return statement.

### SDCC code generation bug
SDCC generates incorrect code for inline expressions passed as function
parameters. Workaround: pre-compute into local variables.

### CONFI PAR3/PAR4 values differ from PROM
The CONFI default CRT parameters differ from the PROM's. Currently relying
on PROM's configuration.

### BIOS size
Code+rodata = 8,290 bytes with bgmap stubbed. Fits in track 0 (9,312 byte
capacity) with 1,022 bytes margin. Still too large for 0xDA00 (target ~5.2KB).

### hw_init_all disabled
Hardware initialization skipped — PROM handles it.

## Open Issues

- #1 ~~hal_in return bug~~ FIXED
- #2 ~~FDC interrupt-driven completion~~ Changed to polling approach
- #3 8275 CRT parameters differ between PROM and CONFI defaults
- #4 BIOS too large for 0xDA00 (8.3KB vs 5.2KB target)
- #5 Automate disk image build
- #6 FDC READ DATA hangs in wait_rqm_write (DIO stays set after command) — **next priority**

## Investigate Later
- Compiler inlining with zcc flags
- Switch statement Z80 code quality
- z88dk-gdb for Z80 debugging
- FDC DMA interaction with display DMA (bus contention)
