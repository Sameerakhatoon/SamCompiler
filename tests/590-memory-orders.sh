#!/usr/bin/env bash
# the memory order argument of an atomic builtin picks the
# instructions, per target and order: what each emits and what it must
# not. tests/diff/threads_atomic_struct.c and the other threads_*.c
# programs run the orders under threads against gcc.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

tmp=$(mktemp -d /tmp/sam_orders.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

# shape TARGET BODY [+want | -unwanted]...: f's code for the body, on
# int a, short h and int o (a memory order that is not a constant).
shape(){
    local target="$1" body="$2"; shift 2
    printf '#include <stdatomic.h>\nint a; short h; int o;\nint f(void){ %s return 0; }\n' "$body" > "$tmp/f.c"
    ./main -target=$target -S -o "$tmp/f.s" "$tmp/f.c" >"$tmp/log" 2>&1 \
        || fail "$target: '$body' did not compile: $(tail -2 "$tmp/log")"
    local check
    for check in "$@"; do
        case "$check" in
            +*) grep -qF -- "${check#+}" "$tmp/f.s" || fail "$target: '$body': no '${check#+}'" ;;
            -*) if grep -qF -- "${check#-}" "$tmp/f.s"; then fail "$target: '$body': '${check#-}' is there"; fi ;;
        esac
    done
}

# x86_64 and i386: TSO, so only seq_cst stores and fences cost anything.
for target in x86_64 x86; do
    shape $target '__atomic_store_n(&a, 1, __ATOMIC_RELAXED);' "+mov dword [" "-xchg"
    shape $target 'atomic_store_explicit(&a, 1, memory_order_release);' "+mov dword [" "-xchg"
    shape $target '__atomic_store_n(&a, 1, __ATOMIC_SEQ_CST);' "+xchg dword ["
    shape $target '__atomic_store_n(&a, 1, o);' "+xchg dword ["
    shape $target '__atomic_store_n(&a, 1, __ATOMIC_ACQUIRE);' "+xchg dword ["
    shape $target '__sync_lock_release(&a);' "+mov dword [" "-xchg"
    shape $target 'atomic_thread_fence(memory_order_acq_rel);' "-mfence"
    shape $target 'atomic_signal_fence(memory_order_seq_cst);' "-mfence"
    shape $target 'atomic_thread_fence(memory_order_seq_cst);' "+mfence"
    shape $target '__atomic_fetch_add(&a, 1, __ATOMIC_RELAXED);' "+lock xadd"
done

# aarch64: ldar/stlr only when the order asks, the exclusive pair's
# acquire and release halves apart, dmb ishld for an acquire fence.
shape aarch64 '__atomic_load_n(&a, __ATOMIC_RELAXED);' "+ldrsw x0, [x0]" "-ldar"
shape aarch64 'atomic_load_explicit(&a, memory_order_consume);' "+ldar w0"
shape aarch64 '__atomic_load_n(&a, __ATOMIC_RELEASE);' "+ldar w0"
shape aarch64 '__atomic_store_n(&a, 1, __ATOMIC_RELAXED);' "+str w0, [x1]" "-stlr"
shape aarch64 '__atomic_store_n(&a, 1, __ATOMIC_RELEASE);' "+stlr w0"
shape aarch64 '__atomic_fetch_add(&a, 1, __ATOMIC_RELAXED);' "+ldxr w2" "+stxr w4" "-ldaxr" "-stlxr"
shape aarch64 '__atomic_fetch_add(&a, 1, __ATOMIC_ACQUIRE);' "+ldaxr w2" "+stxr w4" "-stlxr"
shape aarch64 '__atomic_fetch_or(&a, 1, __ATOMIC_RELEASE);' "+ldxr w2" "+stlxr w4" "-ldaxr"
shape aarch64 '__atomic_exchange_n(&h, 1, __ATOMIC_ACQ_REL);' "+ldaxrh w2" "+stlxrh w4"
shape aarch64 '__atomic_fetch_add(&a, 1, o);' "+ldaxr w2" "+stlxr w4"
shape aarch64 'int e = 0; __atomic_compare_exchange_n(&a, &e, 1, 0, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED);' "+ldaxr w2" "+stxr w3" "-stlxr"
shape aarch64 'int e = 0; __atomic_compare_exchange_n(&a, &e, 1, 0, __ATOMIC_RELEASE, __ATOMIC_RELAXED);' "+ldxr w2" "+stlxr w3" "-ldaxr"
shape aarch64 'int e = 0; __atomic_compare_exchange_n(&a, &e, 1, 0, __ATOMIC_RELEASE, __ATOMIC_ACQUIRE);' "+ldaxr w2" "+stlxr w3"
shape aarch64 'atomic_thread_fence(memory_order_relaxed);' "-dmb"
shape aarch64 'atomic_thread_fence(memory_order_acquire);' "+dmb ishld"
shape aarch64 'atomic_thread_fence(memory_order_release);' "+dmb ish" "-ishld"
shape aarch64 'atomic_thread_fence(memory_order_seq_cst);' "+dmb ish" "-ishld"
shape aarch64 'atomic_signal_fence(memory_order_seq_cst);' "-dmb"

# riscv64: the psABI table: fences around loads and stores, the aq and
# rl bits of amo*, lr and sc.
shape riscv64 '__atomic_load_n(&a, __ATOMIC_RELAXED);' "-fence"
shape riscv64 '__atomic_load_n(&a, __ATOMIC_ACQUIRE);' "+fence r,rw" "-fence rw,rw"
shape riscv64 '__atomic_load_n(&a, __ATOMIC_SEQ_CST);' "+fence rw,rw" "+fence r,rw"
shape riscv64 '__atomic_store_n(&a, 1, __ATOMIC_RELAXED);' "-fence"
shape riscv64 '__atomic_store_n(&a, 1, __ATOMIC_RELEASE);' "+fence rw,w"
shape riscv64 '__atomic_fetch_add(&a, 1, __ATOMIC_RELAXED);' "+amoadd.w a0, a0, (a1)"
shape riscv64 '__atomic_fetch_add(&a, 1, __ATOMIC_ACQUIRE);' "+amoadd.w.aq a0"
shape riscv64 '__atomic_fetch_and(&a, 1, __ATOMIC_RELEASE);' "+amoand.w.rl a0"
shape riscv64 '__atomic_exchange_n(&a, 1, __ATOMIC_ACQ_REL);' "+amoswap.w.aqrl a0"
shape riscv64 '__atomic_exchange_n(&h, 1, __ATOMIC_ACQUIRE);' "+lr.w.aq t5" "+sc.w a6" "-.rl"
shape riscv64 '__atomic_exchange_n(&h, 1, __ATOMIC_RELEASE);' "+lr.w t5" "+sc.w.rl a6" "-.aq"
shape riscv64 'int e = 0; __atomic_compare_exchange_n(&a, &e, 1, 0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);' "+lr.w.aq t5" "+sc.w.rl a2"
shape riscv64 'int e = 0; __atomic_compare_exchange_n(&a, &e, 1, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED);' "+lr.w t5" "+sc.w a2" "-.aq" "-.rl"
shape riscv64 '__atomic_fetch_add(&a, 1, o);' "+amoadd.w.aqrl"
shape riscv64 'atomic_thread_fence(memory_order_relaxed);' "-fence"
shape riscv64 'atomic_thread_fence(memory_order_acquire);' "+fence r,rw"
shape riscv64 'atomic_thread_fence(memory_order_release);' "+fence rw,w"
shape riscv64 'atomic_thread_fence(memory_order_acq_rel);' "+fence.tso"
shape riscv64 'atomic_thread_fence(memory_order_seq_cst);' "+fence rw,rw"

# A memory order with side effects is still evaluated.
cat > "$tmp/side.c" <<'EOF'
#include <stdatomic.h>
int a, calls;
int order(void){ calls++; return __ATOMIC_RELAXED; }
int main(void){
    __atomic_store_n(&a, 5, order());
    atomic_thread_fence(order());
    int v = __atomic_load_n(&a, order());
    return calls * 10 + v;
}
EOF
for target in x86 x86_64 aarch64 riscv64; do
    case $target in
        x86|x86_64) run="" ;;
        aarch64) run="qemu-aarch64" ;;
        riscv64) run="qemu-riscv64" ;;
    esac
    have_tools ${run:-true} || continue
    ./main -target=$target "$tmp/side.c" "$tmp/side_$target" >"$tmp/log" 2>&1 || fail "$target: side.c: $(tail -2 "$tmp/log")"
    set +e
    $run "$tmp/side_$target"
    rc=$?
    set -e
    assert_eq 35 "$rc" "$target: orders with side effects"
done

pass
