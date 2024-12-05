#!/usr/bin/env bash
# Ch124: struct_offset walks a registered struct's variable nodes,
# summing sizes and aligning at each member, until it finds the
# requested name. Tested against `struct s { int a; char b; int c; }`:
#   a -> 0   b -> 4   c -> aligned-up-by-int -> 8
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch124_input.XXXXXX)
printf 'struct s { int a; char b; int c; };' > "$scratch"
probe=$(mktemp /tmp/sam_ch124_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch124_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
#include "helpers/vector.h"
extern struct lex_process_functions compiler_lex_functions;
int main(void){
    struct compile_process* cp = compile_process_create("${scratch}", 0, 0, NULL);
    struct lex_process* lp = lex_process_create(cp, &compiler_lex_functions, 0);
    lex(lp);
    cp->token_vec = lex_process_tokens(lp);
    parse(cp);

    struct node* out = NULL;
    int oa = struct_offset(cp, "s", "a", &out, 0, 0);
    int ob = struct_offset(cp, "s", "b", &out, 0, 0);
    int oc = struct_offset(cp, "s", "c", &out, 0, 0);
    printf("a=%d b=%d c=%d\n", oa, ob, oc);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || true
[ -x "$bin" ] || fail "ch124 probe failed to compile"
got="$("$bin")"
assert_contains "$got" "a=0 b=4 c=8" "struct s {int a; char b; int c;} offsets"
pass
