#!/usr/bin/env bash
# A test whose toolchain is missing used to print
# "skip" and count as passed. need_tools / have_tools in lib.sh now
# fail it, or with ALLOW_MISSING_TOOLCHAIN=1 exit 77, which run-all.sh
# counts as skipped. Also checks test 03 leaves ./test.c alone.
. "$(dirname "$0")/lib.sh"

tmp=$(mktemp -d /tmp/sam_skips.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

cat > "$tmp/1-ok.sh" <<EOF
. "$REPO_ROOT/tests/lib.sh"
pass
EOF
cat > "$tmp/2-needs.sh" <<EOF
. "$REPO_ROOT/tests/lib.sh"
need_tools sam-no-such-tool-xyz
pass
EOF
cat > "$tmp/3-partial.sh" <<EOF
. "$REPO_ROOT/tests/lib.sh"
targets="x86_64"
have_tools sam-no-such-cross-gcc && targets="\$targets other"
[ "\$targets" = x86_64 ] || fail "have_tools said yes"
pass
EOF

out=$(env -u ALLOW_MISSING_TOOLCHAIN bash tests/run-all.sh "$tmp"/*.sh 2>&1 && echo "rc=0" || echo "rc=$?")
assert_contains "$out" "passed: 1   failed: 2   skipped: 0" "missing tools fail by default"
assert_contains "$out" "missing toolchain: sam-no-such-tool-xyz (set ALLOW_MISSING_TOOLCHAIN=1 to skip)" "message"
assert_contains "$out" "rc=1" "run-all exit code"

out=$(ALLOW_MISSING_TOOLCHAIN=1 bash tests/run-all.sh "$tmp"/*.sh 2>&1 && echo "rc=0" || echo "rc=$?")
assert_contains "$out" "passed: 1   failed: 0   skipped: 2" "skips are counted, not passed"
assert_contains "$out" "2-needs ... SKIP (missing: sam-no-such-tool-xyz)" "skip line"
assert_contains "$out" "3-partial ... SKIP (partly run, missing: sam-no-such-cross-gcc)" "partial skip line"
assert_contains "$out" "rc=0" "run-all exit code with skips allowed"

# Test 03 must not touch the checkout's test.c.
before=$(git hash-object test.c)
bash tests/03-missing-input.sh >/dev/null 2>&1 || fail "test 03 failed"
[ -f test.c ] || fail "test 03 removed test.c"
assert_eq "$before" "$(git hash-object test.c)" "test.c after test 03"
[ ! -e test.c.bak ] || fail "test 03 left test.c.bak"
pass
