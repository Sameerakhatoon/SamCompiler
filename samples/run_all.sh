#!/usr/bin/env bash
# Compile every sample under samples/*.c with the freshly built
# ./main, link via the toolchain main itself invokes (nasm + gcc
# -m32), run the resulting binary, and compare the exit code
# against the EXPECTED EXIT line at the top of each .c.
#
# Usage:
#   bash samples/run_all.sh           # run from repo root
#   bash run_all.sh                   # run from inside samples/
#
# Requires gcc-multilib so gcc -m32 can find Scrt1.o / libgcc.
# Without it main reports `Issue assemblign...` and the run
# table will show `build/link FAILED`.

set -u

# Find the repo root from the script's own location so this works
# no matter which directory the user invoked it from.
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/.." && pwd)"
cd "$repo_root"

# Rebuild the compiler if main is missing.
if [ ! -x ./main ]; then
    ./build.sh >/dev/null 2>&1 || {
        echo "build.sh failed" >&2
        exit 1
    }
fi

pass=0
fail=0

for src in samples/*.c; do
    base=$(basename "$src" .c)
    bin="/tmp/sam_${base}.bin"
    expected=$(grep -oP 'EXPECTED EXIT: \K[0-9]+' "$src" | head -1)
    res=$(./main "$src" "$bin" 2>&1)
    if [ -x "$bin" ]; then
        "$bin"
        ec=$?
        if [ "$ec" = "$expected" ]; then
            printf "%-32s exit=%-3s expected=%-3s OK\n" "$base" "$ec" "$expected"
            pass=$((pass + 1))
        else
            printf "%-32s exit=%-3s expected=%-3s MISMATCH\n" "$base" "$ec" "$expected"
            fail=$((fail + 1))
        fi
    else
        printf "%-32s build/link FAILED (gcc-multilib installed?)\n" "$base"
        echo "$res" | tail -3 | sed 's/^/  /'
        fail=$((fail + 1))
    fi
done

# Clean up the tmp binaries we just produced.
rm -f /tmp/sam_*.bin /tmp/sam_*.bin.o

echo
echo "passed: $pass   failed: $fail"
[ "$fail" -eq 0 ]
