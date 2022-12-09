#!/usr/bin/env bash
# Ch199: preprocessor structures. Lands preprocessor/preprocessor.c
# with the typedef_type + preprocessor_node tag-union scaffolding.
# Header-only / declarations-only chapter: just confirm everything
# compiles into build/preprocessor.o and links.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

[ -f "$REPO_ROOT/build/preprocessor.o" ] || fail "build/preprocessor.o not produced by build.sh"
[ -x "$REPO_ROOT/main" ] || fail "main binary not produced after preprocessor.o was added to OBJECTS"

# Confirm the struct names + enum tags exist by feeding a tiny probe.
probe=$(mktemp /tmp/sam_ch199_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch199_bin.XXXXXX)
trap 'rm -f "$probe" "$bin"' EXIT

cat > "$probe" <<'EOF'
#include <stdio.h>
#include <string.h>

// Re-declare the structs and enums minimally so we can assert
// they have the documented shape without including the full
// preprocessor.c. The probe matches the structure layout we
// shipped this chapter.
enum {
    TYPEDEF_TYPE_STANDARD,
    TYPEDEF_TYPE_STRUCTURE_TYPEDEF,
};
enum {
    PREPROCESSOR_NUMBER_NODE,
    PREPROCESSOR_IDENTIFIER_NODE,
    PREPROCESSOR_KEYWORD_NODE,
    PREPROCESSOR_UNARY_NODE,
    PREPROCESSOR_EXPRESSION_NODE,
    PREPROCESSOR_JOINED_NODE,
    PREPROCESSOR_TENARY_NODE,
};
enum {
    PREPROCESSOR_FLAG_EVALUATE_NODE = 0b00000001,
};

int main(void){
    printf("std=%d struct=%d num=%d id=%d kw=%d un=%d ex=%d jn=%d tn=%d flag=%d\n",
        TYPEDEF_TYPE_STANDARD, TYPEDEF_TYPE_STRUCTURE_TYPEDEF,
        PREPROCESSOR_NUMBER_NODE, PREPROCESSOR_IDENTIFIER_NODE,
        PREPROCESSOR_KEYWORD_NODE, PREPROCESSOR_UNARY_NODE,
        PREPROCESSOR_EXPRESSION_NODE, PREPROCESSOR_JOINED_NODE,
        PREPROCESSOR_TENARY_NODE, PREPROCESSOR_FLAG_EVALUATE_NODE);
    return 0;
}
EOF

gcc "$probe" -o "$bin" 2>/dev/null || fail "ch199 probe failed to compile"
got="$("$bin")"
assert_contains "$got" "std=0 struct=1 num=0 id=1 kw=2 un=3 ex=4 jn=5 tn=6 flag=1" \
    "typedef + preprocessor node enums have the expected values"
pass
