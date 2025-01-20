#!/usr/bin/env bash
# SamCompiler compiles itself. Stage 1 is the
# gcc-built ./main. It compiles every source of the compiler for x86_64
# with the declaration-only libc headers in tools/selfhost/include, plus
# tools/selfhost/samvprintf.c (the vprintf family for its own va_list);
# linked, that is stage 2. Stage 2 must then produce the same output as
# stage 1: the asm for the samples and the gcc-differential programs on
# x86_64 (and the samples on the other three targets), and the asm for
# the compiler's own sources, so stage 3 would be stage 2 again.
. "$(dirname "$0")/lib.sh"

need_tools nasm gcc
tmp=$(mktemp -d /tmp/sam_selfhost.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

sources=$(ls ./*.c helpers/*.c preprocessor/*.c preprocessor/static-includes/*.c | grep -v '^\./test\.c$')
mkdir -p "$tmp/s1" "$tmp/obj"
for f in $sources tools/selfhost/samvprintf.c; do
    name=$(echo "$f" | tr '/.' '__')
    ./main -Itools/selfhost/include -I. -target=x86_64 -S "$f" -o "$tmp/s1/$name.s" >"$tmp/log" 2>&1 \
        || fail "stage 1 cannot compile $f: $(grep -v '^everything' "$tmp/log" | head -2)"
    nasm -f elf64 "$tmp/s1/$name.s" -o "$tmp/obj/$name.o" 2>"$tmp/log" \
        || fail "nasm rejects $f: $(head -2 "$tmp/log")"
done
# The compiler finds pc_includes next to its binary.
ln -s "$REPO_ROOT/pc_includes" "$tmp/pc_includes"
gcc -no-pie "$tmp"/obj/*.o -o "$tmp/stage2" 2>"$tmp/log" || fail "link: $(head -3 "$tmp/log")"

# Same exit status, same asm, same messages.
compare(){
    local target="$1" src="$2"
    rm -f "$tmp/a1.s" "$tmp/a2.s"
    ./main -target="$target" -S "$src" -o "$tmp/a1.s" >"$tmp/l1" 2>&1
    local r1=$?
    "$tmp/stage2" -target="$target" -S "$src" -o "$tmp/a2.s" >"$tmp/l2" 2>&1
    local r2=$?
    [ "$r1" = "$r2" ] || fail "$src ($target): exit $r1 from stage 1, $r2 from stage 2: $(head -c 200 "$tmp/l2")"
    cmp -s "$tmp/l1" "$tmp/l2" || fail "$src ($target): the messages differ"
    [ "$r1" != 0 ] || cmp -s "$tmp/a1.s" "$tmp/a2.s" || fail "$src ($target): the asm differs"
}

set +e
count=0
for src in samples/*.c tests/diff/*.c; do
    compare x86_64 "$src"
    count=$((count + 1))
done
for target in x86 aarch64 riscv64; do
    for src in samples/*.c; do
        compare "$target" "$src"
        count=$((count + 1))
    done
done

# Stage 2 compiles the compiler's sources to the same asm.
for f in $sources tools/selfhost/samvprintf.c; do
    name=$(echo "$f" | tr '/.' '__')
    "$tmp/stage2" -Itools/selfhost/include -I. -target=x86_64 -S "$f" -o "$tmp/s2.s" >"$tmp/log" 2>&1 \
        || fail "stage 2 cannot compile $f: $(head -2 "$tmp/log")"
    cmp -s "$tmp/s1/$name.s" "$tmp/s2.s" || fail "stage 2 compiles $f to other asm than stage 1"
    count=$((count + 1))
done
set -e
[ "$count" -gt 250 ] || fail "only $count comparisons ran"
pass
