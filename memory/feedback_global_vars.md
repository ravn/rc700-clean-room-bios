---
name: Prefer global variables over function parameters
description: BIOS has no reentrant code - global variables produce smaller Z80 code than parameter passing
type: feedback
---

Use global variables instead of function parameters for BIOS logic. No BIOS code is reentrant.

**Why:** Global variable pattern saves ~8% code size on Z80 (1103 vs 1190 bytes in testing). Register parameter passing overhead adds up across many small functions.

**How to apply:** For Z80 target code, use module-level static globals for state and function inputs/outputs. The native test build can still use parameters for testability — the HAL abstraction makes this workable. But the Z80 code should favor globals.
