---
name: Use Docker for missing binaries
description: When a tool/binary is not installed on the host, use Docker to run it instead of installing
type: feedback
---

Use Docker when binaries are not available on the host system.
Do not attempt to install packages with brew/apt unless explicitly asked.

**Why:** The user prefers Docker containers over installing system packages.

**How to apply:** When a tool like poppler-utils, imagemagick, etc. is needed
but not installed, find a Docker image that has it and run the command via
`docker run --rm -v ...`.
