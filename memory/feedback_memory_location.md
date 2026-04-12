---
name: Memory location preference
description: User wants memory stored in project directory (memory/), never in .claude/
type: feedback
---

Store memory in version-controlled locations. The project `memory/` directory or `.claude/` are both fine, as long as the files end up committed to git.

**Why:** User wants memory to be persistent and visible in the repo, not hidden in untracked local paths.

**How to apply:** For this project, memory is in `memory/` at the project root. Either that or `.claude/` is acceptable as long as it's tracked by git.
