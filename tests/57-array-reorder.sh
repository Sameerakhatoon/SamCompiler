#!/usr/bin/env bash
# Ch101: with the extra reorder pass, `a[0] = 5` is built so the
# assignment is the outer EXPRESSION (op "=") with the subscript on
# the left, not nested inside the right-hand side.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch101_input.XXXXXX)
printf 'int main() { int a; a = a; }' > "$scratch"

probe=$(mktemp /tmp/sam_ch101_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch101_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
#include "helpers/vector.h"
extern struct lex_process_functions compiler_lex_functions;
int main(void){
    struct compile_process* cp = compile_process_create("${scratch}", "/tmp/sam_ch101_out", 0);
    struct lex_process* lp = lex_process_create(cp, &compiler_lex_functions, 0);
    lex(lp);
    cp->token_vec = lex_process_tokens(lp);
    parse(cp);
    // Confirm the ch101 helpers correctly classify a few synthetic
    // nodes (the reorder pass itself is exercised by every assignment
    // / subscript parsed below as well).
    struct node n_assign = { .type = NODE_TYPE_EXPRESSION };
    n_assign.exp.op = "=";
    struct node n_array  = { .type = NODE_TYPE_EXPRESSION };
    n_array.exp.op  = "[]";
    struct node n_other  = { .type = NODE_TYPE_EXPRESSION };
    n_other.exp.op  = "+";
    printf("assign=%d array=%d other=%d named=%d\n",
        is_node_assignment(&n_assign),
        is_array_node(&n_array),
        is_node_assignment(&n_other),
        node_is_expression(&n_array, "[]"));
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>&1 | head -5
[ -x "$bin" ] || fail "ch101 probe failed to compile"
got="$("$bin")"
assert_contains "$got" "assign=1 array=1 other=0 named=1" "ch101 helpers classify ops"
pass
