---
name: RC702 CP/M memory and disk layout
description: Standard 56K CP/M layout from jbox.dk, CCP+BDOS addresses, track assignments
type: reference
---

Source: https://www.jbox.dk/rc702/cpm.shtm

Standard 56K layout:
- TPA: 0x0100-0xC3FF (49KB)
- CCP: 0xC400-0xC7FF (~2KB, actual CCP is 0x800 bytes)
- BDOS: 0xCC00-0xD47F (~2KB)
- BIOS: 0xD480-0xFFFF

Note: gap between CCP end (0xC7FF) and BDOS start (0xCC00) = 0x0400.
The CCP is actually 0x0800 bytes, BDOS is ~0x0E00 bytes.
CCP+BDOS combined = 0x1600 bytes = 5,632 bytes.

Disk layout:
- Track 0: INIT + BIOS (loaded by PROM, 6,144 bytes)
- Track 1: CCP + BDOS (loaded by warm boot, 5,632 bytes)

During warm boot, BIOS reads track 1 into CCP base address.

**How to apply:** Our BIOS currently at 0xC000 overlaps CCP/BDOS.
For the current temporary layout, we need CCP+BDOS compiled for
a different base address matching our BIOS position.
