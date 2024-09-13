# Shared body of the golden tests for the 64-bit targets (362, 363,
# 364). Source it after lib.sh and call golden_check TARGET. Every
# sample and every input in tests/golden/inputs is compiled with
# -target=TARGET and diffed byte for byte against
# tests/golden/TARGET/<name>.asm. Regenerate with
# `bash tests/golden/regen.sh TARGET` only when a change is intended.
# Test 176 does the same for x86.

golden_check() {
    local target="$1" tmp count=0 src base golden
    tmp=$(mktemp -d "/tmp/sam_golden_$target.XXXXXX")
    # shellcheck disable=SC2064
    trap "rm -rf '$tmp'" EXIT
    for src in samples/*.c tests/golden/inputs/*.c; do
        base=$(basename "$src" .c)
        golden="tests/golden/$target/$base.asm"
        [ -f "$golden" ] || fail "missing golden file $golden"
        # object mode also runs the target's assembler; only the .asm
        # matters here, so a failed assemble is not checked.
        "$REPO_ROOT/main" -target="$target" "$src" "$tmp/$base.asm" object >"$tmp/$base.log" 2>&1 || true
        grep -q "everything compiled fine" "$tmp/$base.log" || fail "$src did not compile for $target: $(tail -2 "$tmp/$base.log")"
        if ! cmp -s "$golden" "$tmp/$base.asm"; then
            { diff -u "$golden" "$tmp/$base.asm" || true; } | head -20 >&2
            fail "$target output for $src differs from $golden"
        fi
        count=$((count + 1))
    done
    [ "$count" -eq "$(ls "tests/golden/$target"/*.asm | wc -l)" ] || fail "tests/golden/$target has files with no matching input"
}
