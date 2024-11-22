#!/usr/bin/env bash
# The compiler's sources include no Linux-only header (compiler.h took
# PATH_MAX from <linux/limits.h>) and call realpath only through
# compiler_real_path in helper.c.
. "$(dirname "$0")/lib.sh"

sources=$(ls ./*.c ./*.h helpers/*.c helpers/*.h preprocessor/*.c preprocessor/static-includes/*.c)
linux=$(grep -n '#include <linux/' $sources || true)
assert_eq "" "$linux" "Linux-only includes"
calls=$(grep -n '\brealpath(' $sources | grep -v '^\./helper\.c:' || true)
assert_eq "" "$calls" "realpath calls outside helper.c"
grep -q 'bool compiler_real_path' helper.c || fail "compiler_real_path missing"

# The helper resolves a relative path to the absolute one.
printf 'int main(){ return 0; }\n' > ./.portable_probe.c
trap 'rm -f ./.portable_probe.c ./.portable_probe.s ./.portable_probe.s.o' EXIT
# (`main a.c a.c` is two inputs to the gcc-style driver; -o names the
# output.)
out=$(./main ./.portable_probe.c -o ./.portable_probe.c 2>&1 && echo rc=0 || echo rc=$?)
assert_contains "$out" "is the input file" "same-file check through compiler_real_path"
pass
