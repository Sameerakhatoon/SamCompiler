#!/usr/bin/env bash
# a preprocessing number that is no constant (`0f`, `08`) is
# only an error where it is used, in the program or in an #if; in a
# skipped group or an unused macro it is fine (tests/diff/ppnumber.c).
# -E writes a floating constant as one (it wrote the bits of the double).
. "$(dirname "$0")/lib.sh"

tmp=$(mktemp -d /tmp/sam_ppnumber.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

check_error(){
    local name="$1" expected="$2" source="$3"
    printf '%s\n' "$source" > "$tmp/$name.c"
    if ./main -target=x86_64 -S "$tmp/$name.c" -o "$tmp/$name.s" > "$tmp/log" 2>&1; then
        fail "$name: compiled, expected an error"
    fi
    grep -q "$expected" "$tmp/log" || fail "$name: expected '$expected', got: $(head -2 "$tmp/log")"
}

check_error suffix 'Invalid suffix "f" on the integer constant 0f' 'int x = 0f;'
check_error octal "Invalid digit '8' in the integer constant 08" 'int main(void){ return 08; }'
check_error in_if 'Invalid suffix "f" on the integer constant 0f' '#if 0f
#endif
int x;'
check_error floating 'Invalid floating constant 1.5q' 'double d = 1.5q;'

printf '#define UNUSED 0f\n#if 0\nint a = 0f;\n#endif\nint x;\n' > "$tmp/fine.c"
./main -target=x86_64 -S "$tmp/fine.c" -o "$tmp/fine.s" > "$tmp/log" 2>&1 || fail "a skipped 0f: $(head -2 "$tmp/log")"
printf '#define F(x) x##f\nfloat a = F(2.5);\ndouble b = 1e3;\n' > "$tmp/float.c"
./main -target=x86_64 -E "$tmp/float.c" -o "$tmp/float.i" > "$tmp/log" 2>&1 || fail "-E: $(head -2 "$tmp/log")"
grep -q 'float a = 0x1.4p+1f;' "$tmp/float.i" || fail "-E wrote: $(cat "$tmp/float.i")"
grep -q 'double b = 0x1.f4p+9;' "$tmp/float.i" || fail "-E wrote: $(cat "$tmp/float.i")"
pass
