#!/usr/bin/env bash
# on i386 a call with a floating argument or result (the x87
# call path) named the function bare, `call printf`, so the PIC pass did
# not send it through the GOT and a PIE link with -z text refused the
# text relocation. tests/401 links only every 16th tests/diff program and
# met it when the list grew (tests/diff/ternary_struct.c).
. "$(dirname "$0")/lib.sh"
need_tools nasm i686-linux-gnu-gcc

tmp=$(mktemp -d /tmp/sam_x86_pic_x87.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

cat > "$tmp/prog.c" <<'EOF'
int printf(const char* format, ...);
double half(double x);
double half(double x){ return x / 2; }
int main(void){
    double d = 2.5;
    printf("%g %g\n", d, half(d));
    return (int)(half(d) * 4) - 5;
}
EOF
samcc -c "$tmp/prog.c" -o "$tmp/prog.o" 2>"$tmp/log" || fail "compile: $(head -3 "$tmp/log")"
samcc -S "$tmp/prog.c" -o "$tmp/prog.s" 2>"$tmp/log" || fail "-S: $(head -3 "$tmp/log")"
if grep -qE '^call (printf|half)$' "$tmp/prog.s"; then
    fail "a call names its function without the \$ prefix: $(grep -E '^call ' "$tmp/prog.s" | head -3)"
fi
i686-linux-gnu-gcc -pie -Wl,-z,text "$tmp/prog.o" -o "$tmp/prog" 2>"$tmp/log" \
    || fail "the PIE link failed: $(head -3 "$tmp/log")"
out=$("$tmp/prog") || fail "the program failed"
assert_eq "2.5 1.25" "$out" "output"
pass
