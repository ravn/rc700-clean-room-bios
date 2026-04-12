---
name: Plans must live in project repo
description: Always save plans to tasks/ in the project directory, never in ~/.claude/plans/
type: feedback
---

Always save plans to `tasks/plan.md` (or similar) in the project directory, not in `~/.claude/plans/`.

**Why:** User wants all project artifacts tracked in git and visible in the repo.

**How to apply:** After plan mode creates a file in ~/.claude/plans/, copy it to tasks/ in the project. The plan file in ~/.claude/plans/ is ephemeral; the one in tasks/ is the source of truth.
