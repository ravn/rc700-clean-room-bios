---
name: BIOS project status
description: RC702 clean room BIOS paused — CTC Ch.3 floppy interrupt never fires, spec needs more detail on ISR architecture
type: project
---

Project paused as of 2026-04-15. The BIOS boots partially (signon banner, display ISR works) but hangs waiting for the first FDC interrupt. CTC Ch.3 interrupt never fires after RECALIBRATE command.

**Why:** The specification lacks sufficient detail on interrupt service routine architecture, CTC re-arming, DMA atomicity, and the exact ISR sequences. A feedback document (docs/SPECIFICATION_FEEDBACK.md) was written for the other Claude instance that has access to the working BIOS source.

**How to apply:** When resuming, start by getting answers to the 8 open questions in SPECIFICATION_FEEDBACK.md Section 7. The most likely root causes are: (1) vector table entry 7 pointing to wrong function, (2) SIO daisy chain interference, (3) CTC Ch.3 CLK/TRG not connected to FDC INTRQ in MAME.
