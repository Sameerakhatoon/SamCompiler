#!/usr/bin/env bash
# -O1 -g. A variable that lives in a register is described
# with DW_OP_reg (rbx is DWARF 3, x19 is 19, s1 is 9), the saves of the
# callee-saved registers have call frame information, and gdb prints
# the register variables of the current frame and, through the saved
# registers, those of its caller.
. "$(dirname "$0")/lib.sh"

tmp=$(mktemp -d /tmp/sam_radebug.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

cat > "$tmp/rdbg.c" <<'EOF'
int leaf(int k){
    int twice = k * 2;
    for(int j = 0; j < 2; j++){
        twice += j;
    }
    return twice;
}

int outer(int n){
    int total = 0;
    for(int i = 0; i < n; i++){
        total += leaf(i);
    }
    return total;
}

int main(void){
    return outer(4) == 2 * (0 + 1 + 2 + 3) + 4 ? 0 : 1;
}
EOF

for target in x86_64 aarch64 riscv64; do
    case $target in
        x86_64)  reg="DW_OP_reg3"; readelf=readelf ;;
        aarch64) reg="DW_OP_reg19"; readelf=aarch64-linux-gnu-readelf ;;
        riscv64) reg="DW_OP_reg9"; readelf=riscv64-linux-gnu-readelf ;;
    esac
    have_tools objdump $readelf || continue
    ./main -target=$target -O1 -g -c "$tmp/rdbg.c" -o "$tmp/r_$target.o" >"$tmp/log" 2>&1 || fail "$target: -O1 -g -c: $(tail -2 "$tmp/log")"
    info=$(objdump --dwarf=info "$tmp/r_$target.o" 2>&1)
    assert_contains "$info" "$reg" "$target: a variable in its first register"
    assert_contains "$info" "DW_OP_fbreg" "$target: parameters still have their home slot until loaded"
    cfi=$($readelf --debug-dump=frames "$tmp/r_$target.o" 2>&1)
    case $target in
        x86_64) assert_contains "$cfi" "DW_CFA_offset: r3 (rbx)" "$target: rbx saved" ;;
        aarch64) assert_contains "$cfi" "DW_CFA_offset: r19 (x19)" "$target: x19 saved" ;;
        riscv64) assert_contains "$cfi" "DW_CFA_offset: r9 (s1)" "$target: s1 saved" ;;
    esac
done

# gdb stops in leaf's loop (line 4): its locals are in registers, and
# outer's i and total, one frame up, come back from leaf's saves.
gdb_check(){
    local target="$1" out="$2"
    assert_contains "$out" "Breakpoint 1, leaf (k=2) at" "$target: break leaf if k == 2"
    assert_contains "$out" "twice = 4" "$target: leaf's twice"
    assert_contains "$out" "j = 0" "$target: leaf's j"
    grep -qE "^#1 .* in outer \(n=4\) at .*rdbg\.c:12" <<<"$out" || fail "$target: bt: $out"
    assert_contains "$out" "\$1 = 2" "$target: outer's i through the saved register"
    assert_contains "$out" "\$2 = 4" "$target: outer's total through the saved register"
}
commands=(-ex "break rdbg.c:4 if k == 2" -ex "START" -ex "info locals" -ex "bt" -ex "up" -ex "print i" -ex "print total")
for target in x86_64 aarch64 riscv64; do
    ./main -target=$target -O1 -g "$tmp/rdbg.c" -o "$tmp/rdbg_$target" >"$tmp/log" 2>&1 || fail "$target: -O1 -g link: $(tail -2 "$tmp/log")"
    case $target in
        x86_64)
            "$tmp/rdbg_$target" || fail "$target: the -O1 -g program failed"
            have_tools gdb || continue
            out=$(timeout 120 gdb -nx -batch "${commands[@]/START/run}" "$tmp/rdbg_$target" 2>&1) ;;
        *)
            have_tools gdb-multiarch qemu-$target || continue
            qemu-$target "$tmp/rdbg_$target" || fail "$target: the -O1 -g program failed"
            port=$((20000 + RANDOM % 20000))
            qemu-$target -g $port "$tmp/rdbg_$target" >/dev/null 2>&1 &
            sleep 1
            out=$(timeout 120 gdb-multiarch -nx -batch -ex "file $tmp/rdbg_$target" -ex "target remote :$port" \
                  "${commands[@]/START/continue}" -ex "kill" 2>&1)
            wait ;;
    esac
    gdb_check $target "$out"
done

pass
