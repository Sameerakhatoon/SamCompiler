#!/usr/bin/env bash
# Try SamCompiler on its own sources (x86_64, assembled with nasm) and
# print, per source file, "ok" or the first error. The libc headers are
# the short declaration-only ones in tools/selfhost/include, then
# pc_includes; glibc's own headers are not used. What still fails, and
# why, is.
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
    if ./main -Itools/selfhost/include -target=x86_64 "$f" "$out/$name.s" object >"$out/log" 2>&1; then
        ok=$((ok + 1))
        echo "ok    $f"
    else
        echo "FAIL  $f: $(grep -v '^everything\|^nasm\|^Compile failed' "$out/log" | head -1)"
    fi
done
echo "compiled: $ok of $total"
