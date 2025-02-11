#!/usr/bin/env bash
# on i386, _Atomic float and double read-modify-writes (a lock
# cmpxchg or lock cmpxchg8b loop), the generic __atomic builtins on a
# double (fild/fistp, cmpxchg8b), and floating conversions from several
# threads at once (the x87 scratch is on each thread's stack now). gcc
# needs libatomic for the floating updates, so the program is not in
# tests/diff: its output is checked against gcc -latomic's here, on every
# target.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

tmp=$(mktemp -d /tmp/sam_x86_atomic.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

cat > "$tmp/atomic_double.c" <<'EOF'
/* Atomic read-modify-writes of float and double objects and the generic
 * __atomic builtins on a double. On i386 these are compare-and-swap
 * loops, cmpxchg8b for the 8-byte ones
 *; four threads make a
 * lost update show. */
#include <pthread.h>
#include <stdio.h>

#define THREADS 4
#define ROUNDS 20000

static _Atomic double total;
static _Atomic float ftotal;
static double plain;
static _Atomic double steps;

static void* work(void* arg){
    long id = (long)arg;
    for(int i = 0; i < ROUNDS; i++){
        total += 0.5;
        ftotal += 1;
        steps++;
        double seen;
        __atomic_load(&plain, &seen, __ATOMIC_SEQ_CST);
        double want;
        do {
            want = seen + 1.0;
        } while(!__atomic_compare_exchange(&plain, &seen, &want, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST));
    }
    return (void*)id;
}

int main(void){
    long check = 0;
    _Atomic double d = 1.25;
    d += 2;
    d *= 3;
    d -= 0.25;
    d /= 2;
    double before = d++;
    double after = --d;
    check += (long)(d * 8) + (long)(before * 8) + (long)(after * 8);
    _Atomic float f = 1.5f;
    f *= 2.5;
    f += 1;
    float old = f--;
    check += (long)(f * 4) + (long)(old * 4);
    double v = 6.75, out = 0;
    __atomic_store(&plain, &v, __ATOMIC_SEQ_CST);
    __atomic_load(&plain, &out, __ATOMIC_SEQ_CST);
    check += (long)(out * 4);
    double nv = -3.5, prev = 0;
    __atomic_exchange(&plain, &nv, &prev, __ATOMIC_SEQ_CST);
    check += (long)(prev * 4) + (long)(plain * 4);
    double expect = 99, desired = 1;
    int ok = __atomic_compare_exchange(&plain, &expect, &desired, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    check += ok * 1000 + (long)(expect * 4);
    ok = __atomic_compare_exchange(&plain, &expect, &desired, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    check += ok * 100 + (long)(plain * 4);
    check += __atomic_always_lock_free(sizeof(double), 0);
    plain = 0;
    pthread_t t[THREADS];
    for(long i = 0; i < THREADS; i++){
        pthread_create(&t[i], 0, work, (void*)i);
    }
    for(int i = 0; i < THREADS; i++){
        void* r;
        pthread_join(t[i], &r);
        check += (long)r;
    }
    printf("%.2f %.2f %.2f %.2f %ld\n", (double)total, (double)ftotal, (double)steps, plain, check);
    check += total == THREADS * ROUNDS * 0.5;
    check += ftotal == THREADS * ROUNDS;
    check += steps == THREADS * ROUNDS;
    check += plain == THREADS * ROUNDS;
    printf("check %ld\n", check);
    return (int)(check & 0x7f);
}
EOF

want="40000.00 80000.00 80000.00 80000.00 285
check 289"

for target in x86 x86_64 aarch64 riscv64; do
    case $target in
        x86|x86_64) run="" ;;
        aarch64)    run="qemu-aarch64" ;;
        riscv64)    run="qemu-riscv64" ;;
    esac
    have_tools ${run:-true} || continue
    ./main -target=$target "$tmp/atomic_double.c" "$tmp/ad_$target" >"$tmp/log" 2>&1 \
        || fail "$target: did not compile: $(tail -2 "$tmp/log")"
    set +e
    got=$(timeout 60 $run "$tmp/ad_$target")
    rc=$?
    set -e
    assert_eq 33 "$rc" "$target: exit status"
    assert_eq "$want" "$got" "$target: output"
done

# The 8-byte loops are cmpxchg8b; no .data scratch is left for the x87.
./main -target=x86 -S -o "$tmp/ad.s" "$tmp/atomic_double.c" >"$tmp/log" 2>&1 || fail "x86: -S failed"
grep -q "lock cmpxchg8b" "$tmp/ad.s" || fail "x86: no lock cmpxchg8b"
grep -q "fistp qword" "$tmp/ad.s" || fail "x86: no fistp qword for the 8-byte load and store"
if grep -q "x87_tmp\|x87_cw" "$tmp/ad.s"; then
    fail "x86: the x87 scratch is still in .data"
fi

# An _Atomic global is aligned to its size, so its plain loads and
# stores are single accesses; other globals stay packed.
cat > "$tmp/align.c" <<'EOF'
char c1;
_Atomic double d;
char c2;
_Atomic int i;
char c3;
_Atomic short s;
int main(void){
    return (int)((unsigned long)&d % 8) + (int)((unsigned long)&i % 4) * 10 + (int)((unsigned long)&s % 2) * 100;
}
EOF
need_x86_link
./main -target=x86 "$tmp/align.c" "$tmp/align" >"$tmp/log" 2>&1 || fail "x86: align.c: $(tail -2 "$tmp/log")"
set +e
"$tmp/align"
rc=$?
set -e
assert_eq 0 "$rc" "x86: _Atomic globals aligned"

pass