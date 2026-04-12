# Plan: CLion Project Setup with Native Test Harness + Z80 Cross-Compilation

## Context

The RC700 cleanroom BIOS project currently has only the specification document and no build infrastructure. The user wants to implement the BIOS in C, with CLion as the IDE. The BIOS logic should be testable natively on the host (macOS, gcc/clang) and cross-compilable to Z80 via z88dk/zsdcc in Docker. The z88dk `ticks` simulator provides an additional Z80-level smoke test layer.

## Directory Structure

```
rc700-cleanroom-bios/
  CMakeLists.txt            # Top-level: native vs z80 detection
  CMakePresets.json         # CLion presets: native + z80
  .gitignore
  src/
    CMakeLists.txt          # Builds bios_logic library (native) or bios binary (z80)
    hal.h                   # Hardware abstraction layer interface
    iobyte.h / iobyte.c    # IOBYTE bit-field parsing and dispatch
    sector.h / sector.c    # Sector translation tables
    dpb.h / dpb.c          # Disk Parameter Blocks, FSPA, format dispatch
    deblock.h / deblock.c  # Sector deblocking algorithm
    console.h / console.c  # Cursor, control chars, scroll, XY escape
    chartab.h / chartab.c  # Character conversion tables
    config.h / config.c    # CONFI block parsing, baud rate tables
  test/
    CMakeLists.txt          # Native-only test executables
    hal_mock.h / hal_mock.c # Mock HAL for tests
    test_iobyte.c           # One file per module, each has main()
    test_sector.c
    test_dpb.c
    test_deblock.c
    test_console.c
    test_chartab.c
    test_config.c
  scripts/
    z80-build.sh            # Docker wrapper for z88dk cmake build
    z80-ticks.sh            # Docker wrapper for ticks simulator
```

## Architecture: HAL Abstraction

All hardware I/O goes through `hal.h`:
- `hal_out(port, value)` / `hal_in(port)` — port I/O
- `hal_di()` / `hal_ei()` — interrupt control
- `hal_display_buffer()` / `hal_workarea()` — memory-mapped regions

Two implementations:
- `hal_mock.c` (test/) — arrays + logging, test helpers to inspect state
- `hal_z80.c` (src/, later) — real Z80 I/O via inline asm

Portable logic modules (iobyte, sector, dpb, deblock, console, chartab, config) never touch hardware directly.

## CMake Setup

**Top-level CMakeLists.txt:** Detects Z80 via `CMAKE_SYSTEM_PROCESSOR`. Adds `src/` always, `test/` only for native.

**src/CMakeLists.txt:** Builds `bios_logic` static library (native) or `bios` executable (z80).

**test/CMakeLists.txt:** Each `test_*.c` is a standalone executable linked against `bios_logic` + `hal_mock`, registered with CTest.

**CMakePresets.json:** Two configure presets:
- `native` — Unix Makefiles, Debug, build/native/
- `z80` — Unix Makefiles, uses z88dk toolchain file (runs inside Docker)

CLion opens the native preset directly. Z80 builds run via `scripts/z80-build.sh`.

## Test Approach

Plain `assert()` based tests, no external framework. Each test file has `main()`, returns 0 on success. CTest runs all. CLion's test panel shows results.

## Implementation Phases

### Phase 1 — Data tables and pure logic (DONE)
1. [x] Project scaffold (CMake, presets, .gitignore)
2. [x] HAL interface + mock
3. [x] `iobyte.c` + `test_iobyte.c`

### Phase 2 — More pure logic modules
4. [ ] `sector.c` + `test_sector.c` — sector translation tables
5. [ ] `dpb.c` + `test_dpb.c` — DPB/FSPA structures, format dispatch
6. [ ] `config.c` + `test_config.c` — CONFI block parsing, baud rate tables

### Phase 3 — Stateful logic
7. [ ] `console.c` + `test_console.c` — cursor, control chars, scrolling
8. [ ] `deblock.c` + `test_deblock.c` — sector deblocking algorithm
9. [ ] `chartab.c` + `test_chartab.c` — character conversion tables

### Phase 4 — HAL and Z80 target
10. [ ] `hal_z80.c` — real Z80 I/O
11. [ ] `main.c` — BIOS jump table, entry points
12. [ ] Z80 Docker build producing a binary
13. [ ] Ticks smoke test

### Phase 5 — Integration
14. [ ] Test in MAME rc702 driver
15. [ ] Test on physical hardware

## Verification

1. `cmake --preset native && cmake --build --preset native` succeeds
2. `ctest --preset native` runs all tests and passes
3. CLion can open the project and run/debug tests
4. `scripts/z80-build.sh` builds (if Docker available, otherwise deferred)
