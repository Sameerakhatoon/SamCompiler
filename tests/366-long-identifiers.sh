#!/usr/bin/env bash
# There was no long-identifier test beyond the
# name-length check in test 251 (a 255-character global on x86_64). Here every
# kind of name is 255 characters long, the lexer's limit: a global, a
# static global, a local, a parameter, a function, a struct tag and
# member, a typedef, an enum constant, a goto label, a macro and its
# parameter. The program is built and run on all four targets, and a
# 256-character name is still a clean error.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1
need_tools nasm aarch64-linux-gnu-gcc qemu-aarch64 riscv64-linux-gnu-gcc qemu-riscv64
need_x86_link

tmp=$(mktemp -d /tmp/sam_longid.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

# A 255-character name starting with the given letter.
name(){ printf '%s' "$1"; printf 'x%.0s' $(seq 254); }
G=$(name g); S=$(name s); L=$(name l); P=$(name p); F=$(name f)
T=$(name t); M=$(name m); Y=$(name y); E=$(name e); B=$(name b)
D=$(name d); A=$(name a)
[ ${#G} = 255 ] || fail "name helper gave ${#G} characters"

cat > "$tmp/prog.c" <<EOF
#define $D($A) (($A) + 1)
struct $T { int $M; };
typedef struct $T $Y;
enum { $E = 7 };
int $G = 3;
static int $S = 4;
int $F(int $P){
    int $L = $P + $G + $S;
    if($L > 0) goto $B;
    return 0;
$B:
    return $L;
}
int main(void){
    $Y v;
    v.$M = $E;
    return $D($F(v.$M));
}
EOF

for target in x86 x86_64 aarch64 riscv64; do
    "$REPO_ROOT/main" -target=$target "$tmp/prog.c" "$tmp/prog-$target" >"$tmp/build-$target.log" 2>&1 \
        || fail "$target build failed: $(grep -v '^\s*$' "$tmp/build-$target.log" | tail -2 | cut -c1-200)"
    case $target in
        aarch64) run=qemu-aarch64 ;;
        riscv64) run=qemu-riscv64 ;;
        *) run="" ;;
    esac
    $run "$tmp/prog-$target" && rc=0 || rc=$?
    assert_eq 15 "$rc" "$target exit status"
done

long=$(name z)x
printf 'int main(void){ int %s = 1; return %s; }\n' "$long" "$long" > "$tmp/bad.c"
err=$("$REPO_ROOT/main" -target=x86_64 "$tmp/bad.c" "$tmp/bad" 2>&1) && rc=0 || rc=$?
[ "$rc" != 0 ] && { [ "$rc" -lt 128 ] || [ "$rc" = 255 ]; } || fail "a 256-character local gave status $rc"
assert_contains "$err" "the limit is 255" "256-character local"
pass
