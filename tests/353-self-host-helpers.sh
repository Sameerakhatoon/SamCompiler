#!/usr/bin/env bash
# Self-hosting, as far as it goes. With the
# small libc declarations in tools/selfhost/include, SamCompiler
# compiles its own vector and arena helpers for x86_64; linked with a
# gcc-built driver they behave like the gcc-built ones. The GNU
# spellings found in gcc-oriented code (__attribute__, __extension__,
# __restrict, __inline), function pointer typedefs returning a struct
# pointer and va_copy, which the compiler's own sources use, compile too.
. "$(dirname "$0")/lib.sh"

command -v nasm >/dev/null || fail "nasm is needed"
tmp=$(mktemp -d /tmp/sam_selfhost.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

for f in vector arena buffer; do
    samcc -Itools/selfhost/include -target=x86_64 "helpers/$f.c" "$tmp/$f.s" object >"$tmp/$f.log" 2>&1 \
        || fail "helpers/$f.c: $(grep -v '^nasm\|^everything' "$tmp/$f.log" | head -3)"
done

cat > "$tmp/driver.c" <<'EOF'
#include <stdio.h>
#include "helpers/vector.h"
void arena_free_all(void);
int main(void){
    struct vector* v = vector_create(sizeof(int));
    for(int i = 0; i < 1000; i++){
        vector_push(v, &i);
    }
    long sum = 0;
    int* p;
    vector_set_peek_pointer(v, 0);
    while((p = vector_peek(v))){
        sum += *p;
    }
    int x = 7;
    vector_push_at(v, 3, &x);
    vector_pop_at(v, 0);
    struct vector* c = vector_clone(v);
    vector_save(c);
    vector_pop(c);
    vector_restore(c);
    printf("count=%d sum=%ld at2=%d back=%d clone=%d\n", vector_count(v), sum,
        *(int*)vector_at(v, 2), *(int*)vector_back(v), vector_count(c));
    vector_free(c);
    arena_free_all();
    return 0;
}
EOF
gcc -no-pie -I. "$tmp/driver.c" "$tmp/vector.s.o" "$tmp/arena.s.o" -o "$tmp/self" 2>"$tmp/link.log" \
    || fail "link: $(head -3 "$tmp/link.log")"
gcc -no-pie -I. "$tmp/driver.c" helpers/vector.c helpers/arena.c -o "$tmp/ref"
want=$("$tmp/ref")
assert_eq "count=1000 sum=499500 at2=7 back=999 clone=1000" "$want" "gcc-built helpers"
assert_eq "$want" "$("$tmp/self")" "self-compiled helpers"

cat > "$tmp/gnu.c" <<'EOF'
#include <stdarg.h>
struct r { int x; };
typedef struct r*(*pick_fn)(struct r* a, struct r* b);
static __inline struct r* second(struct r* a, struct r* b){ return b; }
void fail_hard(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
__attribute__((noreturn)) void stop(void);
__extension__ typedef long wide;
static int sum(int n, ...){
    va_list a;
    va_list b;
    va_start(a, n);
    va_copy(b, a);
    int s = 0;
    for(int i = 0; i < n; i++){
        s += va_arg(b, int);
    }
    va_end(a);
    return s;
}
int main(void){
    struct r one;
    struct r two;
    two.x = 30;
    int* __restrict unused = 0;
    pick_fn pick = second;
    wide w = 2;
    return pick(&one, &two)->x + sum(3, 1, 2, 3) + (int)w;
}
EOF
for t in x86_64 x86; do
    samcc -target=$t "$tmp/gnu.c" "$tmp/gnu.$t" >"$tmp/gnu.log" 2>&1 \
        || fail "gnu.c on $t: $(grep -v '^nasm\|^everything' "$tmp/gnu.log" | head -3)"
done
rc=0
"$tmp/gnu.x86_64" || rc=$?
assert_eq 38 "$rc" "gnu.c exit code"
pass
