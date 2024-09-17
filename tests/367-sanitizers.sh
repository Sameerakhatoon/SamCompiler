#!/usr/bin/env bash
# There was no sanitizer test. Build the compiler
# with AddressSanitizer and UndefinedBehaviorSanitizer into a temporary
# directory, then compile every sample, every golden input and every
# tests/diff program for all four targets with it. Any ASan or UBSan
# report, crash or hang fails the test. Leak checking is off: the
# compiler never frees.
. "$(dirname "$0")/lib.sh"

tmp=$(mktemp -d /tmp/sam_sanitize.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

srcs="main.c $(ls *.c preprocessor/*.c preprocessor/static-includes/*.c helpers/*.c | grep -v -e '^test\.c$' -e '^main\.c$' | tr '\n' ' ')"
gcc -I./ -g -O1 -fsanitize=address,undefined -fno-sanitize-recover=undefined \
    -fno-omit-frame-pointer $srcs -o "$tmp/main-san" 2>"$tmp/build.log" \
    || fail "sanitizer build failed: $(grep -m3 error "$tmp/build.log")"

export ASAN_OPTIONS=detect_leaks=0:abort_on_error=0
export UBSAN_OPTIONS=print_stacktrace=1

count=0
failures=""
for target in x86 x86_64 aarch64 riscv64; do
    for src in samples/*.c tests/golden/inputs/*.c tests/diff/*.c; do
        base=$(basename "$src" .c)
        log="$tmp/$target-$base.log"
        # object mode runs the target's assembler too; its result does
        # not matter here, only what the sanitizers say.
        timeout 300 "$tmp/main-san" -target="$target" "$src" "$tmp/out.s" object >"$log" 2>&1 && rc=0 || rc=$?
        if grep -q -e 'ERROR: AddressSanitizer' -e 'runtime error:' "$log"; then
            failures="$failures $target/$base"
            grep -m1 -e 'ERROR: AddressSanitizer' -e 'runtime error:' "$log" >&2
            grep -m4 '^    #[0-9]' "$log" >&2 || true
        elif [ "$rc" = 124 ] || { [ "$rc" -ge 128 ] && [ "$rc" != 255 ]; }; then
            failures="$failures $target/$base(status=$rc)"
        fi
        count=$((count + 1))
    done
done

[ "$count" -gt 400 ] || fail "only $count compiles"
[ -z "$failures" ] || fail "sanitizer reports:$failures"
pass
