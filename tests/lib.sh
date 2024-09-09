# Shared test helpers. Source this from each test.
# Each test runs from the repo root.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

test_name="$(basename "${0%.*}")"

# Build first so $LINK_OBJS sees the freshly produced object files.
# (Earlier we captured LINK_OBJS before build, which broke when a
# previous test's build had wiped build/.)
./build.sh >/dev/null 2>&1 || true

# Full link list for any test probe that wants to link the compiler
# libraries. Tests use $LINK_OBJS instead of hand-listing files, so
# new modules added in later chapters don't require touching every
# test.
LINK_OBJS="$(ls "$REPO_ROOT"/build/*.o "$REPO_ROOT"/build/helpers/*.o "$REPO_ROOT"/build/static-includes/*.o 2>/dev/null | tr '\n' ' ')"

fail() { echo "$test_name ... FAIL: $*" >&2; exit 1; }

# Missing toolchains. A test that needs a tool asks for it with
# need_tools (the whole test needs it) or have_tools (one part of the
# test, such as one target, needs it). A missing tool fails the test,
# so a machine without the toolchain can never pass silently. With
# ALLOW_MISSING_TOOLCHAIN=1 the test exits with SKIP_STATUS instead,
# and run-all.sh counts it as skipped, not passed. A test that ran
# only some of its parts is reported as skipped too.
SKIP_STATUS=77
skipped_parts=""

pass() {
    if [ -n "$skipped_parts" ]; then
        echo "$test_name ... SKIP (partly run, missing:$skipped_parts)"
        exit "$SKIP_STATUS"
    fi
    echo "$test_name ... ok"
    exit 0
}

# True when every tool is on the PATH. Otherwise fails the test, or,
# with ALLOW_MISSING_TOOLCHAIN=1, records the missing tools and
# returns false so the caller can leave that part out.
have_tools() {
    local t missing=""
    for t in "$@"; do
        command -v "$t" >/dev/null 2>&1 || missing="$missing $t"
    done
    [ -z "$missing" ] && return 0
    [ "${ALLOW_MISSING_TOOLCHAIN:-0}" = 1 ] || fail "missing toolchain:$missing (set ALLOW_MISSING_TOOLCHAIN=1 to skip)"
    skipped_parts="$skipped_parts$missing"
    return 1
}

need_tools() {
    have_tools "$@" && return 0
    echo "$test_name ... SKIP (missing:$skipped_parts)"
    exit "$SKIP_STATUS"
}

# The x86 target links with i686-linux-gnu-gcc, or gcc -m32 when that
# is missing.
need_x86_link() {
    command -v i686-linux-gnu-gcc >/dev/null 2>&1 && return 0
    gcc -m32 -c -x c /dev/null -o /dev/null >/dev/null 2>&1 && return 0
    need_tools i686-linux-gnu-gcc
}

assert_eq() {
    local want="$1" got="$2" what="${3:-value}"
    if [ "$want" != "$got" ]; then
        fail "$what: want='$want' got='$got'"
    fi
}

assert_contains() {
    local hay="$1" needle="$2" what="${3:-output}"
    case "$hay" in
        *"$needle"*) ;;
        *) fail "$what: expected to contain '$needle' but got: $hay" ;;
    esac
}
