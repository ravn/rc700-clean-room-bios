#!/bin/bash
# Build the BIOS for Z80 using z88dk in Docker
set -e
cd "$(dirname "$0")/.."
mkdir -p build/z80

SOURCES="src/crt0.asm src/iobyte.c src/sector.c src/dpb.c src/config.c src/deblock.c src/console.c src/chartab.c src/ringbuf.c src/serial.c src/floppy.c src/hwinit.c src/interrupt.c src/jtvars.c src/hal_z80.c src/main.c"
FLAGS="-compiler=sdcc -mz80 -Cs--std-c23 -SO3 --opt-code-size -Isrc"

echo "=== BIOS binary (with crt0 at 0xDA00) ==="
docker run --rm --platform linux/amd64 \
  -v "$(pwd)":/src -w /src \
  z88dk/z88dk \
  sh -c "zcc +test $FLAGS -DBIOS_WITH_CRT0 --no-crt -zorg 0xC000 -m -create-app $SOURCES -o build/z80/bios"

echo "Built: $(wc -c < build/z80/bios_CODE.bin) bytes (CODE binary)"
grep -E '__code_compiler_tail|__rodata_compiler_tail|__bss_compiler_head' build/z80/bios.map | head -3

# Verify BIOS jump table is at a page boundary (0xXX00)
CODE_HEAD=$(grep '__CODE_head' build/z80/bios.map | awk '{print $3}' | sed 's/\$//')
CODE_HEAD_LOW=$((16#${CODE_HEAD: -2}))
if [ "$CODE_HEAD_LOW" -ne 0 ]; then
    echo "ERROR: BIOS jump table at 0x${CODE_HEAD} is NOT page-aligned!"
    exit 1
fi
echo "BIOS jump table at 0x${CODE_HEAD} — page-aligned OK"

echo ""
echo "=== Smoke test (ticks) ==="
docker run --rm --platform linux/amd64 \
  -v "$(pwd)":/src -w /src \
  z88dk/z88dk \
  sh -c "zcc +test $FLAGS -DSMOKE_TEST -create-app src/iobyte.c src/sector.c src/dpb.c src/config.c src/deblock.c src/console.c src/chartab.c src/ringbuf.c src/serial.c src/floppy.c src/hwinit.c src/interrupt.c src/jtvars.c src/hal_z80.c src/main.c -o build/z80/bios_smoke && z88dk-ticks build/z80/bios_smoke.bin"
echo "Ticks exit: $?"
