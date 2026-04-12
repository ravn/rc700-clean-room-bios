---
name: Never search home directory
description: Critical - never search or glob the user's entire home directory
type: feedback
---

Never search, glob, or grep the user's entire home directory (~/ or /Users/ravn/).

**Why:** User explicitly flagged this as important. Searching ~ is slow, invasive, and irrelevant.

**How to apply:** Always scope searches to the project directory or specific known paths (like ~/git/mame or ~/git/rc700 when explicitly relevant). Never use ~ or /Users/ravn as a search root.
