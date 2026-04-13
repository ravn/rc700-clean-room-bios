# SDCC Z80 code generation bug: inline shift dropped in parameter passing

## To reproduce

```bash
docker run --rm --platform linux/amd64 -v "$(pwd)":/src -w /src z88dk/z88dk sh -c \
  "zcc +test -compiler=sdcc -mz80 -SO3 --opt-code-size -create-app bug.c -o bug && z88dk-ticks bug.bin"
```

Expected: exit code 0 (all tests pass)
Actual: exit code 2 (high byte test fails)

## Files

- `bug.c` — minimal reproducer with self-checking test
- `README.md` — this file
