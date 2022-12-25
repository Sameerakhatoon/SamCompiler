#!/usr/bin/env bash
# Ch216: getting the value of definitions from within source code.
# preprocessor_handle_identifier(_for_token_vector) now looks the
# identifier up in the definitions vector; missing -> push through
# unchanged; TYPEDEF -> push value tokens; otherwise (STANDARD)
# push value tokens via token_vec_push_src_resolve_definitions.
# Macro-function call (next-token is `(`) gets a TODO #warning
# and falls through to the standard path.
#
# Test: feed `#define ABC 50 \n int x = ABC ;` and confirm the
# resulting token_vec replaces the ABC identifier with NUMBER(50).
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

probe=$(mktemp /tmp/sam_ch216_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch216_bin.XXXXXX)
trap 'rm -f "$probe" "$bin"' EXIT

cat > "$probe" <<'EOF'
#include <stdio.h>
#include "compiler.h"
#include "helpers/vector.h"

int preprocessor_run(struct compile_process* compiler);

void push_sym(struct vector* v, char c){ struct token t = {0}; t.type = TOKEN_TYPE_SYMBOL; t.cval = c; vector_push(v, &t); }
void push_id (struct vector* v, const char* s){ struct token t = {0}; t.type = TOKEN_TYPE_IDENTIFIER; t.sval = s; vector_push(v, &t); }
void push_kw (struct vector* v, const char* s){ struct token t = {0}; t.type = TOKEN_TYPE_KEYWORD; t.sval = s; vector_push(v, &t); }
void push_op (struct vector* v, const char* s){ struct token t = {0}; t.type = TOKEN_TYPE_OPERATOR; t.sval = s; vector_push(v, &t); }
void push_num(struct vector* v, long long n){ struct token t = {0}; t.type = TOKEN_TYPE_NUMBER; t.llnum = n; vector_push(v, &t); }
void push_nl (struct vector* v){ struct token t = {0}; t.type = TOKEN_TYPE_NEWLINE; vector_push(v, &t); }

int main(void){
    struct compile_process* cp = compile_process_create("/dev/null", NULL, 0, NULL);
    push_sym(cp->token_vec_original, '#'); push_id(cp->token_vec_original, "define");
    push_id (cp->token_vec_original, "ABC"); push_num(cp->token_vec_original, 50);
    push_nl (cp->token_vec_original);
    push_kw (cp->token_vec_original, "int");
    push_id (cp->token_vec_original, "x");
    push_op (cp->token_vec_original, "=");
    push_id (cp->token_vec_original, "ABC");
    push_sym(cp->token_vec_original, ';');
    push_nl (cp->token_vec_original);

    preprocessor_run(cp);

    int n = vector_count(cp->token_vec);
    // Find the NUMBER token that replaced ABC.
    long long val = -1;
    for (int i = 0; i < n; i++){
        struct token* t = vector_at(cp->token_vec, i);
        if (t && t->type == TOKEN_TYPE_NUMBER){ val = t->llnum; break; }
    }
    int has_abc_ident = 0;
    for (int i = 0; i < n; i++){
        struct token* t = vector_at(cp->token_vec, i);
        if (t && t->type == TOKEN_TYPE_IDENTIFIER && t->sval && S_EQ(t->sval, "ABC")){
            has_abc_ident = 1;
            break;
        }
    }
    printf("n=%d val=%lld abc=%d\n", n, val, has_abc_ident);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || fail "ch216 probe failed to compile"
got="$("$bin")"
# Expect: int (kw) + x (ident) + = (op) + 50 (num replaced ABC) + ; (sym) = 5 tokens, val=50, no ABC ident.
assert_contains "$got" "n=5 val=50 abc=0" "#define ABC 50 + use of ABC expands to NUMBER(50)"
pass
