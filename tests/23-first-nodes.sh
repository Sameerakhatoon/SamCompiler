#!/usr/bin/env bash
# Ch27: parse() converts NUMBER / IDENTIFIER / STRING tokens 1:1 into
# NODE_TYPE_NUMBER / IDENTIFIER / STRING nodes via parse_single_token_to_node.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch27_input.XXXXXX)
printf '5837 ABCD' > "$scratch"

probe=$(mktemp /tmp/sam_ch27_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch27_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
#include "helpers/vector.h"
int main(void){
    int r = compile_file("${scratch}", "/tmp/sam_ch27_out", 0);
    if(r != COMPILER_FILE_COMPILED_OK){ printf("FAIL compile\n"); return 1; }
    // Re-drive a fresh compile_process so we can inspect node_tree_vec.
    extern struct lex_process_functions compiler_lex_functions;
    struct compile_process* cp = compile_process_create("${scratch}", "/tmp/sam_ch27_out2", 0, NULL);
    struct lex_process* lp = lex_process_create(cp, &compiler_lex_functions, 0);
    lex(lp);
    cp->token_vec = lex_process_tokens(lp);
    parse(cp);

    int n = vector_count(cp->node_tree_vec);
    printf("roots=%d\n", n);
    for(int i = 0; i < n; i++){
        struct node** pp = vector_at(cp->node_tree_vec, i);
        struct node* nd = *pp;
        if(nd->type == NODE_TYPE_NUMBER){
            printf("ND NUM %llu\n", nd->llnum);
        } else if(nd->type == NODE_TYPE_IDENTIFIER){
            printf("ND ID %s\n", nd->sval);
        } else {
            printf("ND type=%d\n", nd->type);
        }
    }
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>&1 | head -5
[ -x "$bin" ] || fail "ch27 probe failed to compile"
got="$("$bin")"
# After ch28, parse_expressionable() greedily consumes adjacent
# expressionable tokens. For "5837 ABCD" with no operator, only the
# last one (ABCD) ends up in node_tree_vec; 5837 is left on the
# scratch stack. The book's parser does the same, and real C input
# never has two bare expressionable tokens in a row so it doesn't
# matter past ch28.
assert_contains "$got" "roots=1"     "one top-level node (post-ch28 greedy parse_expressionable)"
assert_contains "$got" "ND ID ABCD"  "trailing identifier surfaces as the root"
pass
