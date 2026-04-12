---
name: Emulator locations and quirks
description: Two RC702 emulators available for testing - MAME and rc700. MAME IMD format caveat.
type: reference
---

Two emulators available for testing:

1. **MAME** — source at `~/git/mame`, RC702 driver at `regnecentralen/rc702.cpp`. Supports several diskette image formats. **Caveat:** IMD format is only updated in memory (changes are not written back to the IMD file).

2. **rc700** — source at `~/git/rc700`. Dedicated RC700-family emulator.

**How to apply:** When testing BIOS images, be aware of MAME's IMD write limitation. For testing disk writes, use a format MAME persists, or use the rc700 emulator.
