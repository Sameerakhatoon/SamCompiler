#!/usr/bin/env bash
# SamCompiler compiles itself against glibc's own headers.
# Stage 1, the gcc-built ./main, compiles every source of the compiler
# for x86_64 with -glibc: the headers are /usr/include's, with nothing
# but -I. for the compiler's own; pc_includes only gives stdarg.h,
# stddef.h and stdbool.h, which glibc leaves to the compiler. Linked
# with glibc, that is stage 2, which prints through glibc's vprintf
# family with SamCompiler's psABI va_list. Stage 2 must give the same
# exit status, messages and asm as stage 1 for the samples and the
# gcc-differential programs on x86_64 and the samples on the other
# three targets. Stage 2 then compiles the sources again, and that
# asm, stage 3's, must be stage 2's.
. "$(dirname "$0")/lib.sh"

need_tools nasm gcc
[ -f /usr/include/x86_64-linux-gnu/bits/types.h ] || fail "glibc's x86_64 headers are not in /usr/include"
tmp=$(mktemp -d /tmp/sam_selfhost_glibc.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

sources=$(ls ./*.c helpers/*.c preprocessor/*.c preprocessor/static-includes/*.c | grep -v '^\./test\.c$')
mkdir -p "$tmp/s2" "$tmp/s3" "$tmp/obj"
for f in $sources; do
    name=$(echo "$f" | tr '/.' '__')
    ./main -glibc -I. -target=x86_64 -S "$f" -o "$tmp/s2/$name.s" >"$tmp/log" 2>&1 \
        || fail "stage 1 cannot compile $f: $(grep -v '^warning' "$tmp/log" | head -2)"
    nasm -f elf64 "$tmp/s2/$name.s" -o "$tmp/obj/$name.o" 2>"$tmp/log" \
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

# Stage 2 compiles the compiler's sources, against glibc too, to the
# asm stage 1 made of them: stage 3 would be stage 2.
for f in $sources; do
    name=$(echo "$f" | tr '/.' '__')
    "$tmp/stage2" -glibc -I. -target=x86_64 -S "$f" -o "$tmp/s3/$name.s" >"$tmp/log" 2>&1 \
        || fail "stage 2 cannot compile $f: $(grep -v '^warning' "$tmp/log" | head -2)"
    cmp -s "$tmp/s2/$name.s" "$tmp/s3/$name.s" || fail "stage 2 compiles $f to other asm than stage 1"
    count=$((count + 1))
done
set -e
[ "$count" -gt 250 ] || fail "only $count comparisons ran"
pass
