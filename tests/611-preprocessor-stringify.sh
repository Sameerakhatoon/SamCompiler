#!/usr/bin/env bash
# #x spells the argument's tokens. `strlen(STR(abc))` was 4
# (the string took the call's `)`), `STR(SQUARE(2))` was "SQUARE(", and
# `XSTR(TWO)` crashed the compiler, which the driver hid behind exit
# status 1. More cases against gcc: tests/diff/pp_stringify.c.
. "$(dirname "$0")/lib.sh"

tmp=$(mktemp -d /tmp/sam_stringify.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

cat > "$tmp/in_call.c" <<'EOF'
#include <string.h>
#define STR(x) #x
int main(void) {
    return (int)strlen(STR(abc));
}
EOF
cat > "$tmp/parens.c" <<'EOF'
#include <string.h>
#define STR(x) #x
#define SQUARE(x) ((x) * (x))
int main(void) {
    const char* s = STR(SQUARE(2));
    return (int)strlen(s);
}
EOF
cat > "$tmp/xstr.c" <<'EOF'
#include <string.h>
#define TWO 2
#define STR(x) #x
#define XSTR(x) STR(x)
int main(void) {
    const char* s = XSTR(TWO);
    return (int)strlen(s) + s[0];
}
EOF

run(){
    local name="$1" want="$2" rc=0
    samcc -target=x86_64 "$tmp/$name.c" -o "$tmp/$name" > "$tmp/$name.log" 2>&1 || fail "$name did not compile: $(head -3 "$tmp/$name.log")"
    "$tmp/$name" || rc=$?
    [ "$rc" = "$want" ] || fail "$name returned $rc, gcc $want"
}
run in_call 3
run parens 9
run xstr 51

samcc -target=x86_64 -E "$tmp/in_call.c" -o "$tmp/in_call.i" > "$tmp/log" 2>&1 || fail "-E: $(head -2 "$tmp/log")"
grep -q 'strlen ( "abc" )\|strlen("abc")\|strlen( "abc" )\|strlen ("abc")' "$tmp/in_call.i" || fail "-E wrote: $(grep strlen "$tmp/in_call.i")"
pass
