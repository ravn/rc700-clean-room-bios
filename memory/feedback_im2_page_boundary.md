---
name: IM2 vector table page alignment
description: The Z80 IM2 interrupt vector table must be at a 256-byte page boundary, I register holds the high byte
type: feedback
---

The Z80 IM2 interrupt vector table must be at a 256-byte page boundary. The I register holds the upper 8 bits of the vector table address. The interrupting device provides the lower 8 bits.

**Why:** This is a Z80 hardware requirement. The spec places the table at 0xF600 (I=0xF6). If the table is not page-aligned, the vector addresses will be wrong.

**How to apply:** Verify that the vector table address used in `_z80_setup_im2` (I register value) matches the actual location where the 18-entry vector table is placed in memory. The CTC vector base (0x00) and SIO vector base (0x10) and PIO vectors (0x20, 0x22) must correspond to byte offsets within this page.
