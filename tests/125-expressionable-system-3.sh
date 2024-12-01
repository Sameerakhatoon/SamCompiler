#!/usr/bin/env bash
# Ch194 (book unnumbered "Part 3"): wires up the precedence /
# associtivity helpers and the reorder pass so binary expressions
# can be rebalanced. New helpers:
#   expressionable_parser_get_precedence_for_operator
#   expressionable_parser_left_op_has_priority
#   expressionable_parser_node_shift_children_left
#   expressionable_parser_reorder_expression
# parse_for_operator now calls reorder on the freshly built node.
#
# Note: upstream has `assert(left_node_type = 0)` (typo:
# assignment, not comparison) which would fire on every binary
# expression. We deviate to `>= 0` so the suite stays green.
# See docs/194.
#
# This chapter's test walks the precedence lookup helpers
# directly; tree-shape reorder is exercised by the next deep
# precedence test.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

probe=$(mktemp /tmp/sam_ch194_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch194_bin.XXXXXX)
trap 'rm -f "$probe" "$bin"' EXIT

cat > "$probe" <<'EOF'
#include <stdio.h>
#include <string.h>
#include "compiler.h"

int expressionable_parser_get_precedence_for_operator(const char* op, struct expressionable_op_precedence_group** group_out);
bool expressionable_parser_left_op_has_priority(const char* op_left, const char* op_right);

int main(void){
    struct expressionable_op_precedence_group* g = NULL;

    int p_star  = expressionable_parser_get_precedence_for_operator("*",  &g);
    int g_star_assoc = g ? g->associtivity : -1;

    int p_plus  = expressionable_parser_get_precedence_for_operator("+",  &g);
    int p_assign= expressionable_parser_get_precedence_for_operator("=",  &g);
    int g_assign_assoc = g ? g->associtivity : -1;

    int p_bogus = expressionable_parser_get_precedence_for_operator("@?", &g);

    // *  binds tighter than + (lower index = higher priority).
    int star_tighter_than_plus = (p_star < p_plus);
    // Left op "+" has priority over right op "*" only if precedence_left
    // <= precedence_right; "+" is wider than "*" so left does NOT have
    // priority, so left_has_priority("+", "*") should be false.
    int left_plus_vs_star = expressionable_parser_left_op_has_priority("+", "*");
    // Same precedence, same op string: short-circuits to false.
    int same_op = expressionable_parser_left_op_has_priority("+", "+");
    // Left "*" vs right "+": left binds tighter, left has priority.
    int left_star_vs_plus = expressionable_parser_left_op_has_priority("*", "+");

    printf("star=%d plus=%d assign=%d bogus=%d sa=%d aa=%d t=%d ps=%d ss=%d sp=%d\n",
        p_star, p_plus, p_assign, p_bogus,
        g_star_assoc, g_assign_assoc,
        star_tighter_than_plus, left_plus_vs_star, same_op, left_star_vs_plus);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || true
[ -x "$bin" ] || fail "ch194 probe failed to compile"
got="$("$bin")"
# * is row 1, + is row 2, = is row 12, bogus -1.
# * associtivity LEFT_TO_RIGHT (0), = associtivity RIGHT_TO_LEFT (1).
assert_contains "$got" "star=1 plus=2 assign=12 bogus=-1 sa=0 aa=1 t=1 ps=0 ss=0 sp=1" \
    "precedence lookup + left_op_has_priority match the op_precedence table"
pass
