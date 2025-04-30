#!/usr/bin/env bash
# lock the 32-bit x86 output. Every sample and every input in
# tests/golden/inputs is compiled with the default target and diffed
# byte for byte against tests/golden/x86/<name>.asm. Any change to the
# x86 asm, even whitespace, fails this test. Regenerate the files with
# tests/golden/regen.sh only when the change is intended.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

tmp=$(mktemp -d /tmp/sam_golden_x86.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

count=0
for src in samples/*.c tests/golden/inputs/*.c; do
    base=$(basename "$src" .c)
    golden="tests/golden/x86/$base.asm"
    [ -f "$golden" ] || fail "missing golden file $golden"
    samcc "$src" "$tmp/$base.asm" object >"$tmp/$base.log" 2>&1 || true
    grep -q "everything compiled fine" "$tmp/$base.log" || fail "$src did not compile: $(tail -2 "$tmp/$base.log")"
    if ! cmp -s "$golden" "$tmp/$base.asm"; then
        # diff exits 1 on a difference; keep set -e from ending the
        # test before fail prints its message.
        { diff -u "$golden" "$tmp/$base.asm" || true; } | head -20 >&2
        fail "x86 output for $src differs from $golden"
    fi
    count=$((count + 1))
done

[ "$count" -eq "$(ls tests/golden/x86/*.asm | wc -l)" ] || fail "golden dir has files with no matching input"
pass
