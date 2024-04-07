# Shared test helpers. Source this from each test.
# Each test runs from the repo root.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

test_name="$(basename "${0%.*}")"

# Full link list for any test probe that wants to link the compiler
# libraries. Tests should use $LINK_OBJS instead of hand-listing files,
# so new modules added in later chapters don't require touching every
# test.
LINK_OBJS="$(ls "$REPO_ROOT"/build/*.o "$REPO_ROOT"/build/helpers/*.o 2>/dev/null | tr '\n' ' ')"

pass() { echo "$test_name ... ok"; exit 0; }
fail() { echo "$test_name ... FAIL: $*" >&2; exit 1; }

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
