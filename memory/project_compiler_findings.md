---
name: zcc compiler feature findings
description: Tested z88dk zcc with sdcc and sccz80 backends - C feature support, code size, and recommended flags
type: project
---

Tested z88dk v1-95163d1f-20260410 (Docker image z88dk/z88dk, --platform linux/amd64).

## Recommended compiler: sdcc (via `zcc +test -compiler=sdcc`)

sdcc produces smaller code than sccz80 and supports more C features.

### Recommended flags
- `-Cs--std-c23` — enables C23 features (typeof, nullptr, bool keyword, enum underlying type, auto, empty initializers)
- `-SO3` — peephole optimizer level 3 (small size reduction)
- `--opt-code-size` — optimize for code size
- Do NOT use `-debug` — causes assembler errors with array names
- Do NOT include `<stdbool.h>` when using `--std-c23` — conflicts because bool becomes a keyword

### Code size comparison (representative BIOS code)
- sdcc default: 1196 bytes
- sdcc -SO3 --opt-code-size: 1190 bytes  
- sccz80 default: 1292 bytes
- Global variable pattern (sdcc): 1103 bytes — **significant savings over parameter passing**
- Global variable pattern (sccz80): 1113 bytes

### Global variables preferred over parameters
Since no BIOS code is reentrant, using global variables instead of function parameters saves significant Z80 code size. Both compilers benefit.

## C Feature Support Summary

### sdcc (default mode, no --std-c23)
- **Works:** stdint.h, stdbool.h, designated initializers, for-init, inline, __func__, _Static_assert (with message), _Generic, anonymous structs/unions, _Alignof, binary literals (0b...)
- **Broken:** compound literals, __attribute__((packed))

### sdcc (with -Cs--std-c23)
- **Additional works:** typeof, nullptr, bool/true/false as keywords, enum with underlying type, empty initializers {}, auto type inference, _Static_assert without message, digit separators (with warning)
- **Still broken:** constexpr (error 281: not implemented), [[maybe_unused]] (parse error), compound literals
- **Caveat:** must NOT include stdbool.h (conflicts with bool keyword)

### sccz80
- **Works:** stdint.h, designated initializers, for-init, inline asm (#asm/#endasm), function pointers, volatile, const, enums, bitfields, 32-bit integers
- **Broken:** _Static_assert, _Generic, _Alignof, __sfr/__at, anonymous structs/unions, C23 features

### SDCC-specific Z80 extensions (work with sdcc)
- `__sfr __at(port)` — direct port I/O variables
- `__at(addr)` — absolute memory address variables  
- `__asm ... __endasm` — inline assembly
- `__naked` — no function prologue/epilogue
- `__critical { }` ��� atomic sections (auto di/ei)
- `__interrupt` — ISR declaration

## Verification
All features verified at runtime using z88dk-ticks simulator — return values checked, not just compilation.

**How to apply:** Use sdcc with `-Cs--std-c23 -SO3 --opt-code-size`. Prefer global variables. Do not use constexpr, compound literals, or [[attributes]]. Use `#define` instead of constexpr.
