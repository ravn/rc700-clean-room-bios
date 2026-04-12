#!/bin/bash
# Build the BIOS for Z80 using z88dk in Docker
set -e
cd "$(dirname "$0")/.."
docker run --rm --platform linux/amd64 \
  -v "$(pwd)":/src -w /src \
  z88dk/z88dk \
  sh -c "cmake --preset z80 && cmake --build --preset z80"
