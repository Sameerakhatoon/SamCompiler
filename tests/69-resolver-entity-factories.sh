#!/usr/bin/env bash
# Ch121: smoke-test each resolver entity factory shipped in ch121.
# We only check the shape (type / flags / dtype) the factory leaves on
# the returned entity.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

scratch=$(mktemp /tmp/sam_ch121_input.XXXXXX)
printf 'int v = 5;' > "$scratch"
probe=$(mktemp /tmp/sam_ch121_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch121_bin.XXXXXX)
trap 'rm -f "$probe" "$bin" "$scratch"' EXIT

cat > "$probe" <<EOF
#include <stdio.h>
#include "compiler.h"
#include "helpers/vector.h"
static void del_scope(struct resolver_scope* s){ (void)s; }
static void del_ent(struct resolver_entity* e){ (void)e; }
int main(void){
    struct compile_process* cp = compile_process_create("${scratch}", 0, 0, NULL);
    struct resolver_callbacks cb = { .delete_scope = del_scope, .delete_entity = del_ent };
    struct resolver_process* rp = resolver_new_process(cp, &cb);

    struct datatype dt = { .type = DATA_TYPE_INTEGER, .size = 4, .type_str = "int" };
    struct node dummy = { .type = NODE_TYPE_NUMBER, .llnum = 1 };

    struct resolver_entity* unk = resolver_create_new_unknown_entity(rp, NULL, &dt, &dummy,
                                    resolver_scope_current(rp), 16);
    struct resolver_entity* ind = resolver_create_new_unary_indirection_entity(rp, NULL, &dummy, 2);
    struct resolver_entity* addr = resolver_create_new_unary_get_address_entity(rp, NULL, &dt,
                                    &dummy, resolver_scope_current(rp), 0);
    struct resolver_entity* cast = resolver_create_new_cast_entity(rp, resolver_scope_current(rp), &dt);

    printf("unk_type=%d unk_off=%d\n",   unk->type,  unk->offset);
    printf("ind_type=%d ind_depth=%d\n", ind->type,  ind->indirection.depth);
    printf("addr_type=%d addr_ptr=%d addr_pd=%d\n",
        addr->type,
        (addr->dtype.flags & DATATYPE_FLAG_IS_POINTER) != 0,
        addr->dtype.pointer_depth);
    printf("cast_type=%d\n", cast->type);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || true
[ -x "$bin" ] || fail "ch121 probe failed to compile"
got="$("$bin")"
# ch237 added RESOLVER_ENTITY_TYPE_NATIVE_FUNCTION between
# FUNCTION (1) and STRUCTURE so every later enum slot bumps by 1.
assert_contains "$got" "unk_type=7 unk_off=16"     "GENERAL=7 unknown entity (post-ch237)"
assert_contains "$got" "ind_type=9 ind_depth=2"    "UNARY_INDIRECTION=9 carries depth (post-ch237)"
assert_contains "$got" "addr_type=8 addr_ptr=1 addr_pd=1" "UNARY_GET_ADDRESS=8 bumps pointer_depth (post-ch237)"
assert_contains "$got" "cast_type=11"               "CAST=11 (post-ch237)"
pass
