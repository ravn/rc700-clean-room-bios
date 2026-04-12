#!/bin/bash
# Run a Z80 binary in the z88dk ticks simulator
set -e
BINARY="${1:?Usage: z80-ticks.sh <binary>}"
cd "$(dirname "$0")/.."
docker run --rm --platform linux/amd64 \
  -v "$(pwd)":/src -w /src \
  z88dk/z88dk \
  ticks "$BINARY"
