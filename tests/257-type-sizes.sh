#!/usr/bin/env bash
# The second keyword of `long int`, `short int`
# and `long long` adds no storage, and a double is 8 bytes. The parser
# added the two sizes (long long 16, long int 12, short int 6).
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

tmp=$(mktemp -d /tmp/sam_type_sizes.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

cat > "$tmp/s.c" <<'EOF'
int main(void){
    int r = 0;
    if(sizeof(short int) == 2) r = r + 1;
    if(sizeof(long int) == sizeof(long)) r = r + 2;
    if(sizeof(long long) == 8) r = r + 4;
    if(sizeof(double) == 8) r = r + 8;
    return r;
}
EOF
samcc -target=x86_64 "$tmp/s.c" "$tmp/s" >/dev/null 2>&1 || fail "the program did not compile"
rc=0
"$tmp/s" || rc=$?
assert_eq 15 "$rc" "x86_64 exit status"

pass
