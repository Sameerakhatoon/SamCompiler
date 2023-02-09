#!/usr/bin/env bash
# Ch237: implementing native functions - part 2. The original
# test used the `test` stub; ch238 replaces that with the real
# va_start native. We update this test in place to verify the
# same dispatch path through va_start instead - both NATIVE
# FUNCTION dispatch and the registered callback firing are still
# under test.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

src=$(mktemp /tmp/sam_ch237_src.XXXXXX.c)
asm=$(mktemp /tmp/sam_ch237_asm.XXXXXX.asm)
trap 'rm -f "$src" "$asm"' EXIT

cat > "$src" <<'EOF'
#include <stdarg.h>

int sum(int num, ...) {
    int result = 0;
    va_list list;
    va_start(list, num);
    return result;
}

int main() {
    return sum(3, 20, 30, 40);
}
EOF

out=$("$REPO_ROOT/main" "$src" "$asm" 2>&1 || true)
case "$out" in
    *"; NATIVE FUNCTION va_start"*) ;;
    *) fail "expected '; NATIVE FUNCTION va_start' tag in asm output; got: $out" ;;
esac
case "$out" in
    *"; va_start on variable num"*) ;;
    *) fail "expected va_start callback's '; va_start on variable num' line; got: $out" ;;
esac
pass
