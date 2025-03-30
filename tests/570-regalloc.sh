#!/usr/bin/env bash
# -O1 keeps scalar locals and
# parameters in callee-saved registers on x86_64, aarch64 and riscv64.
#   - a loop's counter and sum live in registers: the loop reads and
#     writes no stack slot, and the registers are saved and restored;
#   - a local whose address is taken or a double does not get one (the
#     only ones memory() saves are for n and i), and a function with
#     inline asm saves none;
#   - -O0 is unchanged (550 compares it with the golden files) and the
#     gcc differential at -O1 runs tests/diff/regalloc.c on all targets
#     (550 runs the whole suite at -O1).
. "$(dirname "$0")/lib.sh"

tmp=$(mktemp -d /tmp/sam_regalloc.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

cat > "$tmp/loop.c" <<'EOF'
int count(int n, const int* v){
    int s = 0;
    for(int i = 0; i < n; i++){
        s += v[i];
    }
    return s;
}

static void take(int* p){ *p = 1; }

int memory(int n){
    int x = 0;
    double d = 0.5;
    take(&x);
    for(int i = 0; i < n; i++){
        d += x;
    }
    return (int)d;
}

int with_asm(int n){
    int s = 0;
    for(int i = 0; i < n; i++){
        s += i;
    }
    __asm__ volatile("nop");
    return s;
}
EOF

# The lines of function `name` in an asm file.
body(){
    awk -v f="$2" '$0 ~ "^\\$?"f":$" {on=1; next} on && /(;|\/\/|#) [a-z_]+ function$/ {on=0} on' "$1"
}

for target in x86_64 aarch64 riscv64; do
    ./main -target=$target -O1 -S "$tmp/loop.c" -o "$tmp/loop_$target.s" >"$tmp/log" 2>&1 || fail "$target: -O1 -S: $(head -2 "$tmp/log")"
    count=$(body "$tmp/loop_$target.s" count)
    memory=$(body "$tmp/loop_$target.s" memory)
    withasm=$(body "$tmp/loop_$target.s" with_asm)
    case $target in
        x86_64)
            first=rbx
            # Slot accesses: [rbp-N] other than the saves and restores.
            slots=$(grep -E '\[rbp-[0-9]+\]' <<<"$count" | grep -vE '^mov (\[rbp-[0-9]+\], (rbx|r1[2-5])|(rbx|r1[2-5]), \[rbp-[0-9]+\])$' || true)
            saves=$(grep -cE '^mov \[rbp-[0-9]+\], (rbx|r1[2-5])$' <<<"$count" || true)
            msaves=$(grep -cE '^mov \[rbp-[0-9]+\], (rbx|r1[2-5])$' <<<"$memory" || true)
            asaves=$(grep -cE '^mov \[rbp-[0-9]+\], (rbx|r1[2-5])$' <<<"$withasm" || true) ;;
        aarch64)
            first=x19
            slots=$(grep -E '\[x29, #-[0-9]+\]' <<<"$count" || true)
            saves=$(grep -cE '^str x(19|2[0-8]), \[x11' <<<"$count" || true)
            msaves=$(grep -cE '^str x(19|2[0-8]), \[x11' <<<"$memory" || true)
            asaves=$(grep -cE '^str x(19|2[0-8]), \[x11' <<<"$withasm" || true) ;;
        riscv64)
            first=s1
            slots=$(grep -E ' -[0-9]+\(s0\)' <<<"$count" || true)
            saves=$(grep -cE '^sd s([1-9]|1[01]), [0-9]+\(t2\)' <<<"$count" || true)
            msaves=$(grep -cE '^sd s([1-9]|1[01]), [0-9]+\(t2\)' <<<"$memory" || true)
            asaves=$(grep -cE '^sd s([1-9]|1[01]), [0-9]+\(t2\)' <<<"$withasm" || true) ;;
    esac
    [ -z "$slots" ] || fail "$target: count() still uses stack slots at -O1: $slots"
    grep -qE "\b$first\b" <<<"$count" || fail "$target: count() does not use $first"
    [ "$saves" -ge 2 ] || fail "$target: count() saves $saves registers, want at least 2 (i and s)"
    [ "$msaves" -le 2 ] || fail "$target: memory() saves $msaves registers (only n and i may have one)"
    [ "$asaves" = 0 ] || fail "$target: with_asm() saves $asaves registers"

    # The same without -O1: no callee-saved register at all.
    ./main -target=$target -S "$tmp/loop.c" -o "$tmp/o0_$target.s" >"$tmp/log" 2>&1 || fail "$target: -O0 -S"
    if grep -qE "\b$first\b" "$tmp/o0_$target.s"; then
        fail "$target: -O0 uses $first"
    fi
done

# setjmp: a function that calls it keeps everything in memory, so a
# local changed after setjmp has its new value after the longjmp (C
# leaves that value indeterminate, gcc -O0 and -O1 here keep it). Not
# in tests/diff: the x86 backend rejects the program at -O0 already
# ("call arguments do not match their stack size").
cat > "$tmp/sj.c" <<'EOF'
#include <setjmp.h>
static jmp_buf env;
static void jump(int v){ longjmp(env, v); }
int main(void){
    int n = 0;
    for(int i = 0; i < 4; i++){
        n += i;
    }
    int r = setjmp(env);
    n += 10;
    if(r < 3){
        jump(r + 1);
    }
    return n + r;
}
EOF
for target in x86_64 aarch64 riscv64; do
    case $target in x86_64) runner="" ;; *) runner="qemu-$target" ;; esac
    [ -z "$runner" ] || have_tools $runner || continue
    ./main -target=$target -O1 "$tmp/sj.c" -o "$tmp/sj_$target" >"$tmp/log" 2>&1 || fail "$target: setjmp program: $(head -2 "$tmp/log")"
    rc=0; $runner "$tmp/sj_$target" || rc=$?
    [ $rc = $((6 + 40 + 3)) ] || fail "$target: setjmp program returned $rc, want 49"
    ./main -target=$target -O1 -S "$tmp/sj.c" -o "$tmp/sj_$target.s" >"$tmp/log" 2>&1 || fail "$target: setjmp -S"
    if grep -qE '\b(rbx|r1[2-5]|x19|x2[0-8]|s[1-9]|s1[01])\b' "$tmp/sj_$target.s"; then
        fail "$target: a register variable in a function that calls setjmp"
    fi
done

mkdir "$tmp/diff" && cp tests/diff/regalloc.c "$tmp/diff/"
SAMCC_FLAGS=-O1 bash tools/gcc_differential.sh "$tmp/diff" > "$tmp/difflog" 2>&1 || {
    grep -E '\*|mismatches' "$tmp/difflog" | head -20 >&2
    fail "gcc differential of regalloc.c at -O1"
}
pass
