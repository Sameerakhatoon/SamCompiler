#!/usr/bin/env bash
# Ch215: implementing macro functions. Lands the macro-function
# execution helpers: function_arguments_create + count,
# number_push_to_function_arguments, exp_is_macro_function_call,
# evaluate_function_call_argument(s), is_macro_function,
# macro_function_push_argument, token_vec_push_src_resolve_
# definition(s), macro_function_push_something(_definition),
# macro_function_execute, evaluate_function_call. Also wires
# preprocessor_exp_is_macro_function_call into evaluate_exp (with
# a TODO #warning - upstream stops short of actually invoking
# evaluate_function_call until a later chapter).
#
# Since the wire is incomplete this round, we just verify the new
# helpers link and behave sensibly: preprocessor_function_arguments_
# count(NULL) -> 0, preprocessor_is_macro_function over a STANDARD
# vs MACRO_FUNCTION definition.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

probe=$(mktemp /tmp/sam_ch215_probe.XXXXXX.c)
bin=$(mktemp /tmp/sam_ch215_bin.XXXXXX)
trap 'rm -f "$probe" "$bin"' EXIT

cat > "$probe" <<'EOF'
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "compiler.h"
#include "helpers/vector.h"

struct preprocessor_function_arguments* preprocessor_function_arguments_create(void);
int  preprocessor_function_arguments_count(struct preprocessor_function_arguments* arguments);
bool preprocessor_is_macro_function(struct preprocessor_definition* definition);

int main(void){
    struct preprocessor_function_arguments* args = preprocessor_function_arguments_create();
    int empty_n = preprocessor_function_arguments_count(args);
    int null_n  = preprocessor_function_arguments_count(NULL);

    struct preprocessor_definition standard = {0};
    standard.type = PREPROCESSOR_DEFINITION_STANDARD;
    struct preprocessor_definition mf = {0};
    mf.type = PREPROCESSOR_DEFINITION_MACRO_FUNCTION;

    int is_std = preprocessor_is_macro_function(&standard);
    int is_mf  = preprocessor_is_macro_function(&mf);

    printf("empty=%d null=%d is_std=%d is_mf=%d\n", empty_n, null_n, is_std, is_mf);
    return 0;
}
EOF

gcc -I"$REPO_ROOT" "$probe" $LINK_OBJS -o "$bin" 2>/dev/null || fail "ch215 probe failed to compile"
got="$("$bin")"
assert_contains "$got" "empty=0 null=0 is_std=0 is_mf=1" \
    "preprocessor_function_arguments_count + is_macro_function helpers behave as documented"
pass
