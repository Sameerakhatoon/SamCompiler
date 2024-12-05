#!/usr/bin/env bash
# Ch136: default resolver wires up the standard callbacks. After
# resolver_default_new_scope_entity, the private data carries the
# right address strings, and resolver_follow uses the default
# set_result_base to fill result->base.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch136_input.XXXXXX)
printf 'int v;' > "$scratch"
probe=$(mktemp /tmp/sam_ch136_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch136_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
#include "helpers/vector.h"
extern struct lex_process_functions compiler_lex_functions;
int main(void){
    struct compile_process* cp = compile_process_create("${scratch}", 0, 0, NULL);
    struct lex_process* lp = lex_process_create(cp, &compiler_lex_functions, 0);
    lex(lp); cp->token_vec = lex_process_tokens(lp); parse(cp);

    struct resolver_process* rp = resolver_default_new_process(cp);
    struct node** pp = vector_at(cp->node_tree_vec, 0);
    struct node* var = *pp;
    resolver_default_new_scope_entity(rp, var, 0, 0);

    struct node id = { .type = NODE_TYPE_IDENTIFIER, .sval = "v" };
    struct resolver_result* r = resolver_follow(rp, &id);
    printf("ok=%d addr=%s base=%s off=%d\n",
        resolver_result_ok(r),
        r->base.address,
        r->base.base_address,
        r->base.offset);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || true
[ -x "$bin" ] || fail "ch136 probe failed to compile"
got="$("$bin")"
# Global var (no IS_LOCAL_STACK), offset 0: address == name, base == name.
assert_contains "$got" "ok=1 addr=v base=v off=0" "default resolver fills result.base with name-anchored address"
pass
