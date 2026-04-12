---
name: Use byte/word type aliases
description: User prefers byte and word over uint8_t and uint16_t throughout the codebase
type: feedback
---

Use `byte` (uint8_t) and `word` (uint16_t) from `types.h` instead of the stdint names.

**Why:** User preference for readability — byte/word are more natural for Z80 BIOS code.

**How to apply:** All new code should use byte/word. Include "types.h" instead of <stdint.h> in src/ headers. uint32_t stays as-is (no alias defined).
