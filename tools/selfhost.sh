#!/usr/bin/env bash
# Try SamCompiler on its own sources (x86_64, assembled with nasm) and
# print, per source file, "ok" or the first error. The libc headers are
# the short declaration-only ones in tools/selfhost/include, then
# pc_includes; glibc's own headers are not used. All of them compile
#; tests/370-self-host.sh links and runs the result
#. With glibc's own headers instead,
# `./main -glibc -I. -target=x86_64`, see tests/580-self-host-glibc.sh.
#
# Usage: bash tools/selfhost.sh [file.c ...]

set -u
cd "$(dirname "$0")/.."
[ -x ./main ] || ./build.sh >/dev/null
out=$(mktemp -d /tmp/sam_selfhost.XXXXXX)
trap 'rm -rf "$out"' EXIT

if [ $# -gt 0 ]; then
    files="$*"
else
    files=$(ls ./*.c helpers/*.c preprocessor/*.c preprocessor/static-includes/*.c | grep -v '^\./test\.c$')
fi

ok=0
total=0
for f in $files; do
    total=$((total + 1))
    name=$(echo "$f" | tr '/.' '__')
    if ./main -Itools/selfhost/include -I. -target=x86_64 -S "$f" -o "$out/$name.s" >"$out/log" 2>&1; then
        ok=$((ok + 1))
        echo "ok    $f"
    else
        echo "FAIL  $f: $(grep -v '^everything\|^nasm\|^Compile failed' "$out/log" | head -1)"
    fi
done
echo "compiled: $ok of $total"
