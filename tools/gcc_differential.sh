#!/usr/bin/env bash
# Differential test against gcc. Every program in a directory is built twice
# per target, once by that target's gcc and once by SamCompiler, and both
# binaries are run. The exit statuses must match.
#
#   tools/gcc_differential.sh [DIR] [TARGET...]
#
# DIR defaults to tests/diff. TARGET defaults to all four. A target whose
# gcc (or qemu) is missing fails the run, so a missing toolchain can never
# pass silently; set ALLOW_MISSING_TOOLCHAIN=1 to skip it instead.
# SAMCC_FLAGS adds options to every SamCompiler run (SAMCC_FLAGS=-O1
# tests the optimiser); gcc's side is unchanged.
#
# Result codes in the table: a number is the exit status, C means
# SamCompiler rejected the program, A means assembling or linking failed,
# X means SamCompiler crashed, T a timeout, Sn the program died with
# signal n. A '*' marks a mismatch with gcc.
set -uo pipefail

cd "$(dirname "$0")/.."
dir="${1:-tests/diff}"
shift || true
targets="${*:-x86 x86_64 aarch64 riscv64}"
dir="$(cd "$dir" && pwd)"

tmp=$(mktemp -d /tmp/sam_diff.XXXXXX)
trap 'rm -rf "$tmp"' EXIT
# gcc's exit status per (source, gcc version, target). Delete the
# directory to rebuild every reference.
cache_dir="${SAM_DIFF_CACHE:-${XDG_CACHE_HOME:-$HOME/.cache}/samcompiler-gcc-differential}"
mkdir -p "$cache_dir"

gcc_for(){
    case "$1" in
        x86)     echo "i686-linux-gnu-gcc -static" ;;
        x86_64)  echo "gcc -no-pie" ;;
        aarch64) echo "aarch64-linux-gnu-gcc -static" ;;
        riscv64) echo "riscv64-linux-gnu-gcc -static" ;;
    esac
}

runner_for(){
    case "$1" in
        aarch64) echo "qemu-aarch64" ;;
        riscv64) echo "qemu-riscv64" ;;
        *)       echo "" ;;
    esac
}

# Exit status of a binary, or T for a timeout, or Sn for signal n.
run_status(){
    local runner="$1" bin="$2" rc
    timeout 10 $runner "$bin" >/dev/null 2>&1
    rc=$?
    if [ $rc = 124 ]; then echo T
    elif [ $rc -ge 128 ]; then echo "S$((rc - 128))"
    else echo $rc; fi
}

active=""
for t in $targets; do
    cc=$(gcc_for "$t")
    runner=$(runner_for "$t")
    missing=""
    command -v "${cc%% *}" >/dev/null || missing="${cc%% *}"
    [ -n "$runner" ] && ! command -v "$runner" >/dev/null && missing="$missing $runner"
    if [ -n "$missing" ]; then
        if [ "${ALLOW_MISSING_TOOLCHAIN:-0}" = 1 ]; then
            echo "skip $t: missing$missing"
            continue
        fi
        echo "missing toolchain for $t:$missing (set ALLOW_MISSING_TOOLCHAIN=1 to skip)" >&2
        exit 1
    fi
    active="$active $t"
done

printf '%-28s' program
for t in $active; do printf ' %9s' "$t"; done
echo

mismatches=0
for src in "$dir"/*.c; do
    name=$(basename "$src" .c)
    printf '%-28s' "$name"
    for t in $active; do
        ref="$tmp/$name.$t.gcc"
        out="$tmp/$name.$t"
        runner=$(runner_for "$t")
        # gcc's answer only changes with the source or the gcc, so it is
        # cached; static linking and qemu dominate the run time otherwise.
        key=$( { cat "$src"; $(gcc_for "$t") --version 2>/dev/null | head -1; echo "$t"; } | sha256sum | cut -c1-32)
        cached="$cache_dir/$key"
        if [ -f "$cached" ]; then
            want=$(cat "$cached")
        else
            if ! $(gcc_for "$t") -w -o "$ref" "$src" 2>"$tmp/gcc.log"; then
                echo
                echo "gcc rejected $name for $t:" >&2
                cat "$tmp/gcc.log" >&2
                exit 1
            fi
            want=$(run_status "$runner" "$ref")
            echo "$want" > "$cached"
        fi
        # bash reports a crashed child ("Segmentation fault") on its own
        # stderr; the group keeps that redirected away from the table.
        { timeout 20 ./main -target="$t" ${SAMCC_FLAGS:-} "$src" "$out" >"$out.log" 2>&1; } 2>/dev/null
        rc=$?
        if [ $rc = 0 ]; then
            got=$(run_status "$runner" "$out")
        elif [ $rc = 124 ]; then got=T
        elif grep -q "Issue assemblign" "$out.log"; then got=A
        elif [ $rc -ge 128 ] && [ $rc != 255 ]; then got=X
        else got=C; fi
        mark=" "
        if [ "$got" != "$want" ]; then
            mark="*"
            mismatches=$((mismatches + 1))
        fi
        printf ' %4s/%-3s%s' "$got" "$want" "$mark"
    done
    echo
done

echo
echo "mismatches: $mismatches"
[ $mismatches = 0 ]
