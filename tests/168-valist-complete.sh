#!/usr/bin/env bash
# Ch239: implementing VALIST part 2. Closes the stdarg loop -
# pc_includes/stdarg.h now defines the va_arg macro that expands
# to __builtin_va_arg(list, sizeof(type)). Two new native
# functions land: __builtin_va_arg (the bump-and-read primitive)
# and va_end (currently a void no-op).
#
# Test: compile a real varargs function that loops over its args
# via va_start / va_arg / va_end and confirm the emitted asm
# wires all three native callbacks - native__builtin_va_arg
# start/end tags and the va_start tag are all present.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

src=$(mktemp /tmp/sam_ch239_src.XXXXXX.c)
asm=$(mktemp /tmp/sam_ch239_asm.XXXXXX.asm)
trap 'rm -f "$src" "$asm"' EXIT

cat > "$src" <<'EOF'
#include <stdarg.h>

int sum(int num, ...) {
    int result = 0;
    va_list list;
    va_start(list, num);
    int i = 0;
    for (i = 0; i < num; i += 1) {
        result += va_arg(list, int);
    }
    va_end(list);
    return result;
}

int main() {
    return sum(3, 20, 30, 40);
}
EOF

out=$("$REPO_ROOT/main" "$src" "$asm" 2>&1 || true)
case "$out" in
    *"; va_start on variable num"*) ;;
    *) fail "expected va_start tag; got: $out" ;;
esac
case "$out" in
    *"; native__builtin_va_arg start"*) ;;
    *) fail "expected native__builtin_va_arg start tag; got: $out" ;;
esac
case "$out" in
    *"; native__builtin_va_arg end"*) ;;
    *) fail "expected native__builtin_va_arg end tag; got: $out" ;;
esac
pass
