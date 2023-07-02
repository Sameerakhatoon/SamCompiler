#!/usr/bin/env bash
# operators of one left-to-right group chain to the left, in C
# code and in #if: `a - b - c` is `(a - b) - c`. Checked on the
# parse tree through the asm and on a running x86 program:
#   - `100 - 30 - 20` must subtract 30 first: the left operand of the
#     outer `-` is itself the result of `a - b`
#   - tests/golden/inputs/41_left_assoc.c chains -, /, %, << and >>
#     and an #if subtraction; it returns 111 (122 with the old
#     right grouping)
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

tmp=$(mktemp -d /tmp/sam_cm8_assoc.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

cat > "$tmp/sub.c" <<'EOF2'
int main()
{
    int a = 100;
    int b = 30;
    int c = 20;
    return a - b - c;
}
EOF2
"$REPO_ROOT/main" "$tmp/sub.c" "$tmp/sub.asm" object >/dev/null 2>&1 || true
got=$(cat "$tmp/sub.asm")
# (a - b) is computed first, then c is pushed and subtracted.
assert_contains "$got" $'push dword [ebp-4]\npush dword [ebp-8]\npop ecx\npop eax\nsub eax, ecx\npush eax\npush dword [ebp-12]\npop ecx\npop eax\nsub eax, ecx' "a - b - c groups as (a - b) - c"

command -v nasm >/dev/null || { echo "180-left-assoc ... skip (no nasm)"; exit 0; }
"$REPO_ROOT/main" tests/golden/inputs/41_left_assoc.c "$tmp/la" >/dev/null 2>&1 || true
[ -x "$tmp/la" ] || fail "41_left_assoc did not build"
"$tmp/la" && ec=0 || ec=$?
assert_eq "111" "$ec" "chained -, /, %, <<, >> and #if subtraction"
pass
