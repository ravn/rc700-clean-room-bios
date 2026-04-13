---
name: MAME skip gameinfo
description: Must use -skip_gameinfo when running MAME from scripts to bypass the warning/info screen
type: feedback
---

Always pass `-skip_gameinfo` when launching MAME from scripts/automation.
Without it, MAME shows an info screen that requires pressing Space to continue,
which blocks automated runs.

**Why:** The gameinfo screen pauses emulation until user presses Space. Autoboot scripts
and Lua tracing don't work as expected without skipping it.

**How to apply:** Add `-skip_gameinfo` to every MAME command line in scripts and automation.
