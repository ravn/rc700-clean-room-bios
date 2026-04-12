---
name: BIOS must fit in track 0
description: Hard size constraint - entire BIOS + config blocks must fit in track 0 for roa375 autoloader
type: project
---

The complete BIOS including configuration blocks must fit in track 0 of the diskette, as expected by the ROA375 autoloader.

**Hard limit:** 8" diskette track 0 capacity.
**Long-term goal:** Also fit in minidiskette (5.25") track 0.

**Why:** The ROA375 autoloader loads from track 0. Everything must fit there — no overflow to other tracks.

**How to apply:** Monitor Z80 code+rodata size continuously (BSS is zero-initialized in RAM and does NOT consume disk space, so it doesn't count toward the track 0 limit). This applies to the cross-compiled Z80 output only — the native test build has no size restriction. Prefer compact code on the Z80 side. This constraint may drive architectural decisions (e.g., what to include in BIOS vs. load later).
