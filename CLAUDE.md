# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

The goal is to write a **working CP/M 2.2 BIOS for the physical RC702/RC703 with 8" floppy drives**, developed clean-room without access to the original source code or binaries. `RC702_BIOS_SPECIFICATION.md` is the detailed specification driving the implementation.

Two emulators are available for testing:
- **MAME** — source at `~/git/mame` (driver: `regnecentralen/rc702.cpp`). Supports multiple diskette image formats. Note: IMD format is only updated in memory, not written back to disk.
- **rc700** — dedicated emulator at `~/git/rc700`.

## Size Constraint

The Z80 BIOS binary including configuration blocks must fit in **track 0** of the diskette, as required by the ROA375 autoloader. Hard limit is 8" diskette track 0; long-term goal is fitting in 5.25" minidiskette track 0 as well. This constraint applies to the Z80 cross-compiled output only, not the native test build.

## Build Commands

cmake is bundled with CLion (not installed system-wide):
```bash
CMAKE=/Applications/CLion.app/Contents/bin/cmake/mac/aarch64/bin/cmake
$CMAKE --preset native          # Configure native build (host tests)
$CMAKE --build --preset native  # Build
ctest --test-dir build/native   # Run tests
scripts/z80-build.sh            # Cross-compile to Z80 via Docker (z88dk/zsdcc)
scripts/z80-ticks.sh <binary>   # Run Z80 binary in ticks simulator
```

CLion: open the project and select the "Native (Host Tests)" preset.

## Z80 Compiler (zcc)

Always invoke the compiler through `zcc`, never directly as zsdcc/sccz80.

Recommended backend: **sdcc** (`-compiler=sdcc`). Produces smaller code than sccz80.

Recommended flags: `-Cs--std-c23 -SO3 --opt-code-size`

Supported C features (sdcc with `--std-c23`):
- C99: stdint.h, designated initializers, for-init, inline, _Bool, __func__
- C11: _Static_assert, _Generic, anonymous structs/unions, _Alignof
- C23: typeof, nullptr, bool/true/false keywords, enum with underlying type, auto, empty initializers {}, _Static_assert without message, binary literals (0b...), digit separators

**Not supported:** constexpr, compound literals, [[attributes]], __attribute__((packed))

**Important:** Do NOT include `<stdbool.h>` when using `--std-c23` (bool is a keyword, stdbool.h conflicts). Do NOT use `-debug` flag (causes assembler errors with arrays).

SDCC Z80 extensions: `__sfr __at(port)`, `__at(addr)`, `__asm/__endasm`, `__naked`, `__critical{}`, `__interrupt`.

**Prefer global variables over function parameters** for Z80 code — no BIOS code is reentrant, and globals save ~8% code size.

## Architecture

- **src/** — BIOS logic in portable C. Compiles natively (for tests) and to Z80 (via z88dk).
- **test/** — Native test executables. Each `test_*.c` has its own `main()`, uses plain `assert()`, runs via CTest.
- **src/hal.h** — Hardware abstraction layer. Portable logic calls `hal_out()`/`hal_in()` etc., never touches hardware directly.
- **test/hal_mock.c** — Mock HAL for tests: arrays simulating port state.
- **scripts/** — Docker wrappers for z88dk Z80 builds and z88dk-ticks simulator.

## Key Files

- **RC702_BIOS_SPECIFICATION.md** — The specification (~1,700 lines). Covers the full Z80-based CP/M 2.2 BIOS: memory map, interrupt architecture, I/O device programming, disk drivers, console driver, boot sequence, and configuration system.
- **AGENT.md** — Workflow orchestration guidelines (plan mode, subagent strategy, verification, task management).

## Working with the Specification

The specification targets a Z80-A at 4 MHz with specific I/O chips (Z80-PIO, Z80-CTC, Z80-SIO/2, Am9517A DMA, uPD765 FDC, Intel 8275 CRT, optional WD1000 hard disk). When editing, maintain consistency with:

- The 64K memory map layout (TPA ends at 0xC400, BIOS at 0xDA00-0xEE80, display memory at 0xF800-0xFFD0)
- CP/M 2.2 conventions (18-entry BIOS jump table, IOBYTE device routing, 128-byte logical sectors)
- Z80 Interrupt Mode 2 vector table architecture
- I/O port addresses and device register definitions already established in the document

## Workflow Preferences (from AGENT.md)

- Enter plan mode for non-trivial tasks; re-plan immediately if something goes wrong
- Use subagents for research and parallel analysis
- Verify before marking done — prove correctness
- Track tasks in `tasks/todo.md`, lessons in `tasks/lessons.md`
- Simplicity first, minimal impact, find root causes
