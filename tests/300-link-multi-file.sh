#!/usr/bin/env bash
# B5-B8 and PIE: tests/link/main.c and lib.c
# linked as two objects on all four targets, built by SamCompiler and
# by the target's gcc in every combination: both by SamCompiler, main.c
# by SamCompiler with lib.c by gcc, and the other way round. Globals
# and extern variables cross the objects both ways, the two static
# helpers must not clash, and the 64-bit links are PIEs (dynamic on
# x86_64 and under qemu with the cross sysroot); i386 links static.
# The objects carry .note.GNU-stack, so the linker does not warn about
# an executable stack.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

tmp=$(mktemp -d /tmp/sam_link.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

for target in x86 x86_64 aarch64 riscv64; do
    case $target in
        x86)     cc="i686-linux-gnu-gcc"; link="-static"; run="" ;;
        x86_64)  cc="gcc";                link="-pie";    run="" ;;
        aarch64) cc="aarch64-linux-gnu-gcc"; link="-pie"; run="qemu-aarch64 -L /usr/aarch64-linux-gnu" ;;
        riscv64) cc="riscv64-linux-gnu-gcc"; link="-pie"; run="qemu-riscv64 -L /usr/riscv64-linux-gnu" ;;
    esac
    command -v "$cc" >/dev/null || fail "$target: $cc is missing"
    [ -z "$run" ] || command -v "${run%% *}" >/dev/null || fail "$target: ${run%% *} is missing"

    for f in main lib; do
        # object mode: the asm goes to the named file, the object next to it.
        ./main -target=$target tests/link/$f.c "$tmp/$f.$target.s" object >"$tmp/log" 2>&1 \
            || fail "$target: $f.c did not compile: $(tail -2 "$tmp/log")"
        $cc -w -c tests/link/$f.c -o "$tmp/$f.$target.gcc.o" || fail "$target: gcc rejected $f.c"
        readelf -S "$tmp/$f.$target.s.o" | grep -q '\.note\.GNU-stack' \
            || fail "$target: $f.c's object has no .note.GNU-stack"
        bind=$(readelf -s "$tmp/$f.$target.s.o" | awk '$NF == "helper" { print $5 }')
        assert_eq LOCAL "$bind" "$target: $f.c's static helper"
    done
    bind=$(readelf -s "$tmp/lib.$target.s.o" | awk '$NF == "gs" { print $5 }')
    assert_eq GLOBAL "$bind" "$target: lib.c's gs"

    for pair in "s.o s.o" "s.o gcc.o" "gcc.o s.o"; do
        set -- $pair
        objs="$tmp/main.$target.$1 $tmp/lib.$target.$2"
        $cc $link $objs -o "$tmp/prog" 2>"$tmp/log" || fail "$target ($pair): link failed: $(tail -3 "$tmp/log")"
        if grep -qi 'executable stack' "$tmp/log"; then
            fail "$target ($pair): $(cat "$tmp/log")"
        fi
        rc=0
        out=$($run "$tmp/prog") || rc=$?
        assert_eq 0 "$rc" "$target ($pair): failed check bits of main.c"
        assert_eq "> link 21 7" "$out" "$target ($pair): output"
    done
done

# The default link (not object mode) makes a PIE on x86_64 (dynamic)
# and aarch64 (static-pie); riscv64 stays -static (no rcrt1.o in the
# cross glibc) and i386 is not position independent.
for target in x86_64 aarch64; do
    ./main -target=$target tests/diff/register_names.c "$tmp/default_$target" >"$tmp/log" 2>&1 \
        || fail "$target: the default link failed: $(tail -2 "$tmp/log")"
    file "$tmp/default_$target" | grep -q 'pie executable' \
        || fail "$target: the default link is not a PIE: $(file "$tmp/default_$target")"
done

pass