#!/usr/bin/env bash
# The driver and its exit code: the driver must not erase its
# input, must not run paths as shell syntax, must not overflow on long
# identifiers or include names, and must fail when assembling or linking
# fails.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

tmp=$(mktemp -d /tmp/sam_driver_safety.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

cat > "$tmp/ok.c" <<'EOF'
int main(void){ return 7; }
EOF

# E1: the same path as input and output is refused and the source survives.
cp "$tmp/ok.c" "$tmp/same.c"
rc=0
samcc -target=x86_64 "$tmp/same.c" "$tmp/same.c" >/dev/null 2>&1 || rc=$?
[ $rc != 0 ] || fail "same input and output path was accepted"
cmp -s "$tmp/ok.c" "$tmp/same.c" || fail "the source file was modified"

# E1: a failed compile leaves the previous output untouched and no
# temporary file behind.
echo "previous output" > "$tmp/out"
printf "int main(void){ return 'a; }\n" > "$tmp/bad.c"
rc=0
samcc -target=x86_64 "$tmp/bad.c" "$tmp/out" >/dev/null 2>&1 || rc=$?
[ $rc != 0 ] || fail "a bad program compiled"
assert_eq "previous output" "$(cat "$tmp/out")" "output after a failed compile"
leftover=$(ls "$tmp" | grep -c '^out\.' || true)
assert_eq 0 "$leftover" "temporary files left after a failed compile"

# E2: shell syntax in the output path stays part of the path.
mkdir -p "$tmp/with space"
out="$tmp/with space/prog; touch sam_pwned_251"
rm -f sam_pwned_251
samcc -target=x86_64 "$tmp/ok.c" "$out" >/dev/null 2>&1 || fail "a path with spaces did not compile"
if [ -e sam_pwned_251 ]; then
    rm -f sam_pwned_251
    fail "the output path was run as a shell command"
fi
rc=0
"$out" || rc=$?
assert_eq 7 "$rc" "exit status of the program at the quoted path"

# E3: a 255-character identifier works, a longer one is a clean error.
name255=$(printf 'v%.0s' $(seq 255))
printf 'int %s = 5;\nint main(void){ return %s; }\n' "$name255" "$name255" > "$tmp/long_ok.c"
samcc -target=x86_64 "$tmp/long_ok.c" "$tmp/long_ok" >/dev/null 2>&1 || fail "a 255-character identifier was rejected"
rc=0
"$tmp/long_ok" || rc=$?
assert_eq 5 "$rc" "exit status with a 255-character identifier"

name300=$(printf 'w%.0s' $(seq 300))
printf 'int %s = 5;\nint main(void){ return %s; }\n' "$name300" "$name300" > "$tmp/long_bad.c"
rc=0
err=$(samcc -target=x86_64 "$tmp/long_bad.c" "$tmp/long_bad" 2>&1 >/dev/null) || rc=$?
[ $rc != 0 ] && [ $rc -lt 128 ] || [ $rc = 255 ] || fail "a 300-character identifier gave status $rc"
assert_contains "$err" "the limit is 255" "long identifier diagnostic"

# E4: an include name longer than PATH_MAX is a clean error.
longinc=$(printf 'i%.0s' $(seq 5000))
printf '#include "%s.h"\nint main(void){ return 0; }\n' "$longinc" > "$tmp/long_inc.c"
rc=0
samcc -target=x86_64 "$tmp/long_inc.c" "$tmp/long_inc" >/dev/null 2>&1 || rc=$?
[ $rc != 0 ] || fail "a 5000-character include name compiled"
[ $rc = 255 ] || [ $rc -lt 128 ] || fail "a 5000-character include name crashed with status $rc"

# A link failure is a non-zero exit status, not 0.
printf 'int helper(void){ return 1; }\n' > "$tmp/nomain.c"
rc=0
samcc -target=x86_64 "$tmp/nomain.c" "$tmp/nomain" >/dev/null 2>&1 || rc=$?
[ $rc != 0 ] || fail "a failed link exited with status 0"

pass
