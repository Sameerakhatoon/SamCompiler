#!/usr/bin/env bash
# C11 atomics. tests/diff/atomic_*.c and threads_*.c check the
# values against gcc on every target (threads included); here:
#   - the instructions: every atomic access is a real atomic instruction
#     (lock xadd, xchg, lock cmpxchg, mfence; ldar, stlr, ldaxr/stlxr,
#     dmb ish; amo*.aqrl, lr/sc, the psABI fences), not a load and a
#     store;
#   - _Atomic float and double (gcc needs
#     libatomic for them, so the result is checked by value);
#   - what is rejected instead of miscompiled.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

tmp=$(mktemp -d /tmp/sam_atomics.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

cat > "$tmp/shape.c" <<'EOF'
_Atomic int a;
_Atomic long l;
_Atomic char c;
_Atomic short s;
int f(void){
    a++;
    l += 5;
    c = 3;
    s ^= 1;
    int e = 0;
    __atomic_compare_exchange_n(&a, &e, 1, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    return __atomic_load_n(&a, __ATOMIC_SEQ_CST) + a;
}
EOF

expect_asm(){
    local target="$1"; shift
    ./main -target=$target -S -o "$tmp/shape_$target.s" "$tmp/shape.c" >"$tmp/log" 2>&1 \
        || fail "$target: shape.c did not compile: $(tail -2 "$tmp/log")"
    local ins
    for ins in "$@"; do
        grep -qF -- "$ins" "$tmp/shape_$target.s" || fail "$target: no '$ins' in the atomic code"
    done
}

expect_asm x86     "lock add dword" "lock cmpxchg" "xchg byte" "mfence"
expect_asm x86_64  "lock xadd dword" "lock xadd qword" "xchg byte" "lock cmpxchg word" "lock cmpxchg dword" "mfence"
expect_asm aarch64 "ldaxr w2" "ldaxr x2" "stlxr w4" "stlrb w0" "ldaxrh" "ldar w0" "cmp w2, w1" "dmb ish"
expect_asm riscv64 "amoadd.w.aqrl" "amoadd.d.aqrl" "lr.w.aqrl" "sc.w.aqrl" "fence rw,w" "fence r,rw" "fence rw,rw"

# A plain int keeps its plain code.
printf 'int a;\nint f(void){ a++; a = 2; return a; }\n' > "$tmp/plain.c"
./main -target=x86_64 -S -o "$tmp/plain.s" "$tmp/plain.c" >/dev/null 2>&1 || fail "plain.c did not compile"
if grep -qE 'lock|xchg [a-z]+ \[|mfence' "$tmp/plain.s"; then
    fail "x86_64: atomic instructions for a plain int"
fi

# _Atomic float and double: the compare-and-swap loop on their bits.
cat > "$tmp/fp.c" <<'EOF'
int main(void){
    _Atomic double d = 1.5;
    d += 2;
    d *= 3;
    _Atomic float f = 0.25f;
    f++;
    f -= 0.5f;
    double x = d;
    return (int)x * 10 + (int)(f * 4);
}
EOF
for target in x86 x86_64 aarch64 riscv64; do
    case $target in
        x86|x86_64)  run="" ;;
        aarch64) run="qemu-aarch64" ;;
        riscv64) run="qemu-riscv64" ;;
    esac
    have_tools ${run:-true} || continue
    ./main -target=$target "$tmp/fp.c" "$tmp/fp_$target" >"$tmp/log" 2>&1 || fail "$target: fp.c: $(tail -2 "$tmp/log")"
    set +e
    $run "$tmp/fp_$target"
    rc=$?
    set -e
    assert_eq 103 "$rc" "$target: _Atomic double and float"
done

reject(){
    local target="$1" what="$2" src="$3"
    printf '%s\n' "$src" > "$tmp/bad.c"
    if ./main -target=$target "$tmp/bad.c" "$tmp/bad" >"$tmp/log" 2>&1; then
        fail "$target: $what was accepted"
    fi
    grep -q "$4" "$tmp/log" || fail "$target: $what: unexpected message: $(grep -v '^\s' "$tmp/log" | tail -2)"
}

for target in x86_64 aarch64 riscv64; do
    reject $target "an _Atomic struct" 'struct S { int a; }; _Atomic struct S s; int main(void){ struct S t = s; return t.a; }' \
        "_Atomic struct, union or void"
    reject $target "fetch_add on a double" 'double d; int main(void){ __atomic_fetch_add(&d, 1, 5); return 0; }' \
        "needs an integer or pointer object"
done
reject x86 "an 8-byte atomic on i386" 'double d; int main(void){ double x = __atomic_load_n(&d, 5); return 0; }' \
    "no 8-byte integers on i386"
reject x86 "an _Atomic int += a double on i386" '_Atomic int i; int main(void){ i += 0.5; return 0; }' \
    "only supported for a float or double object"
for target in x86 x86_64 aarch64 riscv64; do
    reject $target "the wrong argument count" 'int a; int main(void){ return __atomic_load_n(&a); }' \
        "__atomic_load_n expects 2 arguments"
    reject $target "a builtin on a non-pointer" 'int a; int main(void){ return __atomic_load_n(a, 5); }' \
        "must be a pointer"
done

pass
