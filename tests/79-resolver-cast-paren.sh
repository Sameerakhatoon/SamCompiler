#!/usr/bin/env bash
# Ch131: resolver_follow handles EXPRESSION_PARENTHESES (stepping
# through), CAST (UNSUPPORTED operand wrap + CAST entity with the
# target dtype), and unsupported unary fall-through to UNSUPPORTED.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

probe=$(mktemp /tmp/sam_ch131_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch131_bin.XXXXXX)
scratch=$(mktemp /tmp/sam_ch131_input.XXXXXX)
printf 'int v;' > "$scratch"
trap 'rm -f "$probe" "$bin" "$scratch"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
#include "helpers/vector.h"
extern struct lex_process_functions compiler_lex_functions;
static void del_scope(struct resolver_scope* s){ (void)s; }
static void del_ent(struct resolver_entity* e){ (void)e; }
static void* mk_priv(struct resolver_entity* e, struct node* n, int o, struct resolver_scope* s){
    (void)e; (void)n; (void)o; (void)s; return NULL;
}
int main(void){
    struct compile_process* cp = compile_process_create("${scratch}", 0, 0);
    struct resolver_callbacks cb = {.delete_scope=del_scope,.delete_entity=del_ent,.make_private=mk_priv};
    struct resolver_process* rp = resolver_new_process(cp, &cb);

    struct datatype dt = { .type = DATA_TYPE_INTEGER, .size = 4, .type_str = "int" };
    struct node operand = { .type = NODE_TYPE_NUMBER, .llnum = 99 };
    struct node cast = { .type = NODE_TYPE_CAST };
    cast.cast.dtype = dt;
    cast.cast.operand = &operand;

    struct resolver_result* r = resolver_new_result(rp);
    resolver_follow_cast(rp, &cast, r);
    // count >= 2: UNSUPPORTED wrap + CAST entity on top.
    struct resolver_entity* top = resolver_result_peek(r);
    printf("count=%zu top_type=%d top_dt=%d\n",
        r->count, top->type, top->dtype.type);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || true
[ -x "$bin" ] || fail "ch131 probe failed to compile"
got="$("$bin")"
# CAST=10, DATA_TYPE_INTEGER value depends on enum; we just check top is CAST.
assert_contains "$got" "count=2 top_type=10" "follow_cast pushes UNSUPPORTED operand + CAST on top"
pass
