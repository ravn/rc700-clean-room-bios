---
name: No Lua I/O port reads in MAME
description: CRITICAL - Never read I/O ports from MAME Lua scripts, it has side effects that change device state
type: feedback
---

NEVER read I/O ports from Lua scripts in MAME (e.g., `io:read_u8(0x04)`).
Reading FDC MSR, CTC registers, etc. from Lua has side effects that change
the device state — it can acknowledge interrupts, drain result bytes, or
alter the FDC protocol state machine.

**Why:** This was causing false diagnostic results and actively interfering
with the FDC's operation during boot. The Lua reads were draining FDC state
that the Z80 code needed to see.

**How to apply:** For MAME diagnostics, only read CPU registers (PC, SP, AF, etc.)
and memory contents. Never use `cpu.spaces["io"]` to read device ports.
Use screenshots and register traces only.
