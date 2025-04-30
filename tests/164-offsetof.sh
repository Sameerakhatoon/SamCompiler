#!/usr/bin/env bash
# Ch234: implementing offsetof. Adds pc_includes/stddef.h which
# defines `offsetof(TYPE, MEMBER)` as `&((TYPE*)0x00)->MEMBER`,
# leveraging the ch233 resolver fix for pointer casts.
#
# Empties the stddef static-include stub (real content arrives
# via #include <stddef.h> -> pc_includes/stddef.h).
#
# Test: compile a source that #includes stddef.h and uses
# offsetof; confirm main reaches codegen and the output
# references the offset of the second int member (4).
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

src=$(mktemp /tmp/sam_ch234_src.XXXXXX.c)
asm=$(mktemp /tmp/sam_ch234_asm.XXXXXX.asm)
trap 'rm -f "$src" "$asm"' EXIT

cat > "$src" <<'EOF'
#include <stddef.h>
struct dog
{
    int x;
    int y;
};

int main()
{
    return offsetof(struct dog, y);
}
EOF

# The asm is no longer echoed to stdout:
# read it from the output file.
out=$(samcc "$src" "$asm" object 2>&1 || true; cat "$asm")
case "$out" in
    *"everything compiled fine"*|*"Issue assemblign"*) ;;
    *) fail "main did not reach codegen on offsetof source: $out" ;;
esac
case "$out" in
    *"4"*) ;;
    *) fail "expected output to reference offset 4 for member y; got: $out" ;;
esac
pass
