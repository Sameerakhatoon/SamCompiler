#!/usr/bin/env bash
# i386 position independent code. Every program of
# tests/diff and samples, compiled for x86 with and without -fPIC, has
# no absolute address (R_386_32) in .text: a local symbol is
# GOT-relative off esi, an undefined one read from the GOT, and nothing
# calls through the PLT, so ebx stays a scratch register. A sample of
# them links as a dynamic PIE and into a shared object with `-z text`,
# which refuses any text relocation (the x86_64 -fPIC path too). The
# default link is a static PIE that runs.
. "$(dirname "$0")/lib.sh"

need_tools nasm i686-linux-gnu-gcc
./build.sh >/dev/null 2>&1

tmp=$(mktemp -d /tmp/sam_x86pic.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

count=0
for src in tests/diff/*.c samples/*.c; do
    base=$(basename "$src" .c)
    ./main -c "$src" -o "$tmp/$base.o" 2>"$tmp/log" || continue
    ./main -fPIC -c "$src" -o "$tmp/$base.pic.o" 2>"$tmp/log" || fail "$src: -fPIC failed: $(head -3 "$tmp/log")"
    count=$((count + 1))
    # Linking is the slow part: every 16th program.
    [ $((count % 16)) -eq 1 ] || continue
    i686-linux-gnu-gcc -pie -Wl,-z,text "$tmp/$base.o" -o "$tmp/$base" -lm 2>"$tmp/log" \
        || fail "$src: the PIE link failed: $(head -3 "$tmp/log")"
    # An i386 shared object with thread-locals has a text relocation
    # (R_386_TLS_IE), which `-z text` refuses.
    if ! grep -qE '_Thread_local|thread_local' "$src"; then
        i686-linux-gnu-gcc -shared -Wl,-z,text "$tmp/$base.pic.o" -o "$tmp/$base.so" 2>"$tmp/log" \
            || fail "$src: the shared link failed: $(head -3 "$tmp/log")"
    fi
    ./main -target=x86_64 -fPIC -c "$src" -o "$tmp/$base.pic64.o" 2>"$tmp/log" \
        || fail "$src: x86_64 -fPIC failed: $(head -3 "$tmp/log")"
    gcc -shared -Wl,-z,text "$tmp/$base.pic64.o" -o "$tmp/$base.64.so" 2>"$tmp/log" \
        || fail "$src: the x86_64 shared link failed: $(head -3 "$tmp/log")"
done
[ "$count" -gt 20 ] || fail "only $count programs compiled"

bad=$(readelf -rW "$tmp"/*.o | awk '/^Relocation section/ { text = index($0, ".rel.text") > 0 } text && / R_386_32 /' | head -3)
[ -z "$bad" ] || fail "absolute relocations in .text: $bad"

# The default link: a static PIE that runs.
./main tests/link/main.c tests/link/lib.c -o "$tmp/prog" 2>"$tmp/log" || fail "default link: $(cat "$tmp/log")"
file "$tmp/prog" | grep -q 'static-pie linked' || fail "the default x86 link is not a static PIE: $(file "$tmp/prog")"
assert_eq "> link 21 7" "$("$tmp/prog")" "static PIE output"

pass
