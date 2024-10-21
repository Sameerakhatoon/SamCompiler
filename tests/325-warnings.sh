#!/usr/bin/env bash
# Diagnostics: warnings for an
# unused local, a missing return and a pointer stored in an integer. They
# go to stderr and change neither the status nor the program.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

tmp=$(mktemp -d /tmp/sam_warnings.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

cat > "$tmp/warn.c" <<'SRC'
int pick(int a){
    if (a)
        return 1;
}
int main(void){
    int unused;
    int value = 7;
    int *p = &value;
    long address = p;
    int status = pick(1);
    address = p;
    return status + (address != 0) + 39;
}
SRC
for target in x86 x86_64; do
    rc=0
    out=$(./main -target=$target "$tmp/warn.c" "$tmp/warn_$target" 2>"$tmp/err_$target") || rc=$?
    err=$(cat "$tmp/err_$target")
    assert_eq 0 "$rc" "status with warnings ($target)"
    assert_contains "$out" "everything compiled fine" "stdout ($target)"
    case "$out" in *"warning:"*) fail "a warning went to stdout ($target)" ;; esac
    assert_contains "$err" "warning: Unused variable unused on line 6," "unused variable ($target)"
    assert_contains "$err" "warning: Control reaches the end of function pick, which returns a value" "missing return ($target)"
    assert_contains "$err" "warning: Initialization makes long from a pointer (int*) without a cast on line 9," "pointer in an initializer ($target)"
    assert_contains "$err" "warning: Assignment makes long from a pointer (int*) without a cast on line 11," "pointer in an assignment ($target)"
done
rc=0
"$tmp/warn_x86_64" || rc=$?
assert_eq 41 "$rc" "exit status of the program with warnings"

# Nothing to warn about here.
cat > "$tmp/clean.c" <<'SRC'
void stop(int code);
int first(int *a, int n){
    for (;;) {
        if (n > 0)
            return a[0];
        stop(1);
    }
}
int second(int n){
    switch (n) {
        case 1: return 10;
        default: return 20;
    }
}
int third(int n){
    if (n)
        return 1;
    else
        return 2;
}
int fourth(int n){
    if (n)
        return 1;
    stop(n);
}
int main(void){
    int values[2];
    int count = sizeof(values) / sizeof(values[0]);
    _Bool has = values;
    values[0] = count;
    return first(values, count) + second(1) + third(0) + fourth(1) + has;
}
SRC
err=$(./main -target=x86_64 "$tmp/clean.c" "$tmp/clean.o" object 2>&1 >/dev/null) || fail "the clean program was rejected: $err"
case "$err" in *"warning:"*) fail "unexpected warning: $err" ;; esac

pass
