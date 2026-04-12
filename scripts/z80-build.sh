#!/bin/bash
# Build the BIOS for Z80 using z88dk in Docker
set -e
cd "$(dirname "$0")/.."
mkdir -p build/z80

SOURCES="src/iobyte.c src/sector.c src/dpb.c src/config.c src/deblock.c src/console.c src/chartab.c src/main.c"

docker run --rm --platform linux/amd64 \
  -v "$(pwd)":/src -w /src \
  z88dk/z88dk \
  sh -c "zcc +test -compiler=sdcc -mz80 -Cs--std-c23 -SO3 --opt-code-size -Isrc -m -create-app $SOURCES -o build/z80/bios_smoke"

echo "Built: $(wc -c < build/z80/bios_smoke.bin) bytes"
