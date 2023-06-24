#!/usr/bin/env bash
# Compile every sample under samples/*.c with the freshly built
# ./main for one or all targets, run the binary (natively, or under
# qemu-user for the cross targets) and compare the exit code against
# the EXPECTED EXIT line at the top of each .c. A sample whose answer
# depends on pointer size may also carry `// EXPECTED EXIT lp64: N`,
# which the three 64-bit targets use instead.
#
# Usage:
#   bash samples/run_all.sh                    # x86, the default
#   bash samples/run_all.sh ARCH=aarch64       # one target
#   bash samples/run_all.sh ARCH=all           # all four
#   ARCH=riscv64 bash samples/run_all.sh       # same, from the environment
#
# Toolchains per target: docs/guides/targets/<target>.md.

set -u

# Find the repo root from the script's own location so this works
# no matter which directory the user invoked it from.
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/.." && pwd)"
cd "$repo_root"

arch="${ARCH:-x86}"
for arg in "$@"; do
    case "$arg" in
        ARCH=*) arch="${arg#ARCH=}" ;;
        *) echo "unknown argument $arg (expected ARCH=x86|x86_64|aarch64|riscv64|all)" >&2; exit 2 ;;
    esac
done

case "$arch" in
    all) targets="x86 x86_64 aarch64 riscv64" ;;
    x86|x86_64|aarch64|riscv64) targets="$arch" ;;
    *) echo "unknown ARCH=$arch (expected x86, x86_64, aarch64, riscv64 or all)" >&2; exit 2 ;;
esac

# Rebuild the compiler if main is missing.
if [ ! -x ./main ]; then
    ./build.sh >/dev/null 2>&1 || {
        echo "build.sh failed" >&2
        exit 1
    }
fi

# How to run a binary for each target on this x86-64 host.
runner_for(){
    case "$1" in
        aarch64) echo "qemu-aarch64" ;;
        riscv64) echo "qemu-riscv64" ;;
        *)       echo "" ;;
    esac
}

tmp=$(mktemp -d /tmp/sam_samples.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

total_fail=0
summary=""

for target in $targets; do
    runner=$(runner_for "$target")
    pass=0
    fail=0
    [ "$arch" = all ] && echo "== $target"
    for src in samples/*.c; do
        base=$(basename "$src" .c)
        bin="$tmp/${target}_${base}.bin"
        expected=""
        if [ "$target" != x86 ]; then
            expected=$(grep -oP 'EXPECTED EXIT lp64: \K[0-9]+' "$src" | head -1)
        fi
        [ -n "$expected" ] || expected=$(grep -oP 'EXPECTED EXIT: \K[0-9]+' "$src" | head -1)
        res=$(./main -target="$target" "$src" "$bin" 2>&1)
        if [ -x "$bin" ]; then
            $runner "$bin"
            ec=$?
            if [ "$ec" = "$expected" ]; then
                printf "%-32s exit=%-3s expected=%-3s OK\n" "$base" "$ec" "$expected"
                pass=$((pass + 1))
            else
                printf "%-32s exit=%-3s expected=%-3s MISMATCH\n" "$base" "$ec" "$expected"
                fail=$((fail + 1))
            fi
        else
            printf "%-32s build/link FAILED (toolchain installed? see docs/guides/targets/%s.md)\n" "$base" "$target"
            echo "$res" | tail -3 | sed 's/^/  /'
            fail=$((fail + 1))
        fi
    done
    echo
    echo "passed: $pass   failed: $fail"
    summary="$summary$(printf '%-8s passed: %-3s failed: %s' "$target" "$pass" "$fail")\n"
    total_fail=$((total_fail + fail))
    [ "$arch" = all ] && echo
done

if [ "$arch" = all ]; then
    echo "== summary"
    printf "$summary"
fi

[ "$total_fail" -eq 0 ]
