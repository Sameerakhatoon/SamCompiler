#!/usr/bin/env bash
# The compiler built with -fsanitize=address,leak compiles every sample
# for every target, and a program with an error, without a single
# AddressSanitizer or LeakSanitizer report. Nothing used to be freed and
# the input file was never closed, so LeakSanitizer reported every
# compile. The assembler
# is replaced by a stub: only the compiler runs under the sanitizers.
. "$(dirname "$0")/lib.sh"

tmp=$(mktemp -d /tmp/sam_asan.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

cp -r ./*.c ./*.h Makefile build.sh helpers preprocessor pc_includes samples "$tmp"/
mkdir -p "$tmp/build/helpers" "$tmp/build/static-includes" "$tmp/stub"
make -C "$tmp" -j2 CFLAGS="-g -O1 -fno-omit-frame-pointer -fsanitize=address,leak -w" >"$tmp/build.log" 2>&1 \
    || fail "sanitizer build failed: $(tail -5 "$tmp/build.log")"

for tool in nasm aarch64-linux-gnu-as riscv64-linux-gnu-as; do
    printf '#!/bin/sh\nexit 0\n' > "$tmp/stub/$tool"
    chmod +x "$tmp/stub/$tool"
done

export ASAN_OPTIONS=detect_leaks=1
cd "$tmp"
count=0
for t in x86 x86_64 aarch64 riscv64; do
    for src in samples/*.c; do
        name=$(basename "$src" .c)
        rc=0
        PATH="$tmp/stub:$PATH" ./main -target=$t "$src" "$tmp/$name.$t.s" object >/dev/null 2>"$tmp/err.txt" || rc=$?
        if grep -q "Sanitizer" "$tmp/err.txt"; then
            fail "$name on $t: $(grep -m1 -A6 'ERROR' "$tmp/err.txt")"
        fi
        assert_eq 0 "$rc" "$name on $t exit code"
        count=$((count + 1))
    done
done
assert_eq 56 "$count" "compiles checked"

# A compile that stops with an error exits from deep inside the compiler.
printf 'int main(){ return undeclared_thing; }\n' > "$tmp/bad.c"
rc=0
./main "$tmp/bad.c" "$tmp/bad.s" object >/dev/null 2>"$tmp/err.txt" || rc=$?
[ "$rc" -ne 0 ] || fail "bad.c compiled"
if grep -q "Sanitizer" "$tmp/err.txt"; then
    fail "error path: $(grep -m1 -A6 'ERROR' "$tmp/err.txt")"
fi
pass
