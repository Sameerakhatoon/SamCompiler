#!/usr/bin/env bash
# -g on every target.
#   - the code is the same with and without -g (.text compared byte for
#     byte), and the asm without -g has no debug directives;
#   - the line table maps the statements to the C file (readelf), the
#     call frame information is there (.eh_frame from .cfi_* on aarch64
#     and riscv64, a .debug_frame on the NASM targets), and .debug_info
#     has the functions, parameters, locals and types (objdump --dwarf);
#   - with gdb (gdb for x86 and x86_64, gdb-multiarch and qemu -g for
#     aarch64 and riscv64): `break file.c:N` stops at line N, `bt` names
#     the C functions through a recursion and a libc callback (qsort),
#     and the parameters, locals, a static local and a global print.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

tmp=$(mktemp -d /tmp/sam_debug.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

cat > "$tmp/dbg.c" <<'EOF'
#include <stdlib.h>

struct point { int x; long y; struct point* next; };
int counter = 5;

static int cmp(const void* a, const void* b){
    return *(const int*)a - *(const int*)b;
}

int fact(int n){
    if(n <= 1){
        return 1;
    }
    return n * fact(n - 1);
}

int sum(int n, struct point* p){
    static int calls = 40;
    int s = 0;
    int i;
    calls++;
    for(i = 0; i < n; i++){
        s += i;
    }
    return s + p->x + calls;
}

int main(void){
    int v[4] = {4, 2, 3, 1};
    struct point pt;
    pt.x = 7;
    pt.y = 9;
    pt.next = 0;
    qsort(v, 4, sizeof(int), cmp);
    counter = sum(4, &pt) + fact(3);
    return counter == 6 + 7 + 41 + 6 && v[0] == 1 ? 0 : 1;
}
EOF

for target in x86 x86_64 aarch64 riscv64; do
    case $target in
        x86)     objcopy=objcopy; readelf=readelf; frames=".debug_frame" ;;
        x86_64)  objcopy=objcopy; readelf=readelf; frames=".debug_frame" ;;
        aarch64) objcopy=aarch64-linux-gnu-objcopy; readelf=aarch64-linux-gnu-readelf; frames=".eh_frame" ;;
        riscv64) objcopy=riscv64-linux-gnu-objcopy; readelf=riscv64-linux-gnu-readelf; frames=".eh_frame" ;;
    esac
    have_tools $objcopy $readelf objdump || continue
    samcc -target=$target -S "$tmp/dbg.c" -o "$tmp/plain_$target.s" >"$tmp/log" 2>&1 || fail "$target: -S: $(tail -2 "$tmp/log")"
    if grep -qE '\.loc |\.cfi_|\.\.@dbg|debug_info' "$tmp/plain_$target.s"; then
        fail "$target: debug directives without -g"
    fi
    samcc -target=$target -c "$tmp/dbg.c" -o "$tmp/plain_$target.o" >"$tmp/log" 2>&1 || fail "$target: -c: $(tail -2 "$tmp/log")"
    samcc -target=$target -g -c "$tmp/dbg.c" -o "$tmp/g_$target.o" >"$tmp/log" 2>&1 || fail "$target: -g -c: $(tail -2 "$tmp/log")"
    $objcopy -O binary --only-section=.text "$tmp/plain_$target.o" "$tmp/plain.bin"
    $objcopy -O binary --only-section=.text "$tmp/g_$target.o" "$tmp/g.bin"
    cmp -s "$tmp/plain.bin" "$tmp/g.bin" || fail "$target: -g changed the code"

    lines=$($readelf --debug-dump=decodedline "$tmp/g_$target.o" 2>&1)
    assert_contains "$lines" "dbg.c" "$target: line table file"
    for n in 7 14 22 23 25 35; do
        grep -qE "dbg\.c +$n +0x" <<<"$lines" || fail "$target: no row for line $n in the line table"
    done
    grep -qF "$frames" <<<"$($readelf -S "$tmp/g_$target.o")" || fail "$target: no $frames"
    cfi=$($readelf --debug-dump=frames "$tmp/g_$target.o" 2>&1)
    assert_contains "$cfi" "DW_CFA_def_cfa_register" "$target: frame information"
    info=$(objdump --dwarf=info "$tmp/g_$target.o" 2>&1)
    for text in "DW_TAG_subprogram" "DW_TAG_formal_parameter" "DW_TAG_lexical_block" "DW_TAG_structure_type" \
                "DW_AT_name        : sum" "DW_AT_name        : calls" "DW_AT_name        : counter" "DW_OP_fbreg"; do
        assert_contains "$info" "$text" "$target: .debug_info"
    done
done

# gdb. One batch per target, the breakpoints in program order: cmp
# under qsort (line 34), then sum's loop, then fact at the bottom of
# its recursion (line 35).
gdb_check(){
    local target="$1" out="$2"
    assert_contains "$out" "Breakpoint 1, cmp (a=" "$target: break cmp"
    grep -qE "^#[0-9]+ .* in main \(\) at .*dbg\.c:34" <<<"$out" || fail "$target: bt through qsort: $out"
    assert_contains "$out" "Breakpoint 2, sum (n=4, p=" "$target: break dbg.c:23"
    assert_contains "$out" "23	        s += i;" "$target: stopped at line 23"
    grep -qE "^#1 .* in main \(\) at .*dbg\.c:35" <<<"$out" || fail "$target: bt from sum: $out"
    assert_contains "$out" "s = 0" "$target: info locals"
    assert_contains "$out" "calls = 41" "$target: static local"
    assert_contains "$out" "\$1 = {x = 7, y = 9, next = 0x0}" "$target: print *p"
    assert_contains "$out" "\$2 = 5" "$target: print counter"
    assert_contains "$out" "Breakpoint 3, fact (n=1) at" "$target: break fact if n == 1"
    grep -qE "^#1 .* in fact \(n=2\) at .*dbg\.c:14" <<<"$out" || fail "$target: bt through fact: $out"
    grep -qE "^#3 .* in main \(\) at .*dbg\.c:35" <<<"$out" || fail "$target: bt through fact: $out"
}
commands=(-ex "break cmp" -ex "START" -ex "bt" -ex "delete"
          -ex "break dbg.c:23" -ex "continue" -ex "bt" -ex "info locals" -ex "print *p" -ex "print counter" -ex "delete 2"
          -ex "break fact if n == 1" -ex "continue" -ex "bt")
for target in x86 x86_64 aarch64 riscv64; do
    samcc -target=$target -g "$tmp/dbg.c" -o "$tmp/dbg_$target" >"$tmp/log" 2>&1 || fail "$target: -g link: $(tail -2 "$tmp/log")"
    case $target in
        x86|x86_64)
            "$tmp/dbg_$target" || fail "$target: the -g program failed"
            have_tools gdb || continue
            out=$(timeout 120 gdb -nx -batch "${commands[@]/START/run}" "$tmp/dbg_$target" 2>&1) ;;
        *)
            have_tools gdb-multiarch qemu-$target || continue
            qemu-$target "$tmp/dbg_$target" || fail "$target: the -g program failed"
            port=$((20000 + RANDOM % 20000))
            qemu-$target -g $port "$tmp/dbg_$target" >/dev/null 2>&1 &
            sleep 1
            out=$(timeout 120 gdb-multiarch -nx -batch -ex "file $tmp/dbg_$target" -ex "target remote :$port" \
                  "${commands[@]/START/continue}" -ex "kill" 2>&1)
            wait ;;
    esac
    gdb_check $target "$out"
done

pass
