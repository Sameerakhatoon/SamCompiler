#!/usr/bin/env bash
# There was no fuzz test. Every tests/diff program
# is mutated (tools/fuzz_mutate.py: deleted, duplicated, swapped or
# stray tokens, deleted or duplicated lines, a cut-off file) and the
# mutants are compiled for x86 and x86_64. The compiler may reject a
# mutant, but it must not crash (a signal) or hang.
#
# Malformed input still crashes in places, so the crashes
# this corpus finds today are listed in tests/fuzz/known-crashes.txt
# and only a crash missing from that list fails the test. A listed
# mutant that no longer crashes is reported so the list can shrink:
# FUZZ_UPDATE_KNOWN=1 rewrites the list from this run.
#
# FUZZ_SEED and FUZZ_PER_SOURCE (defaults 1 and 2) run a different or
# bigger corpus; the known list only applies to the default one.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1
need_tools python3

seed="${FUZZ_SEED:-1}"
per_source="${FUZZ_PER_SOURCE:-2}"
known_file=tests/fuzz/known-crashes.txt
default_corpus=0
[ "$seed" = 1 ] && [ "$per_source" = 2 ] && default_corpus=1

tmp=$(mktemp -d /tmp/sam_fuzz.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

python3 tools/fuzz_mutate.py "$seed" "$per_source" "$tmp/in" tests/diff/*.c
mutants=$(ls "$tmp/in" | wc -l)
[ "$mutants" -gt 100 ] || fail "only $mutants mutants"

known=""
[ "$default_corpus" = 1 ] && [ -f "$known_file" ] && known=$(grep -v '^#' "$known_file" | awk '{print $1 " " $2}')

crashes=()
new=()
fixed=()
for target in x86 x86_64; do
    for f in "$tmp"/in/*.c; do
        name=$(basename "$f" .c)
        # The braces take bash's own "Segmentation fault" line too.
        { timeout 30 "$REPO_ROOT/main" -target="$target" "$f" "$tmp/out.s" object >"$tmp/log" 2>&1; } 2>/dev/null && rc=0 || rc=$?
        is_known=0
        grep -qxF "$target $name" <<<"$known" && is_known=1
        if [ "$rc" = 124 ] || { [ "$rc" -ge 128 ] && [ "$rc" != 255 ]; }; then
            what="status $rc"
            [ "$rc" = 124 ] && what="hang"
            [ "$rc" = 134 ] && what="abort"
            [ "$rc" = 139 ] && what="segfault"
            crashes+=("$target $name $what")
            [ "$is_known" = 1 ] || new+=("$target $name ($what)")
        elif [ "$is_known" = 1 ]; then
            fixed+=("$target $name")
        fi
    done
done

if [ "${FUZZ_UPDATE_KNOWN:-0}" = 1 ] && [ "$default_corpus" = 1 ]; then
    {
        echo "# Mutants of the default fuzz corpus (seed 1, 2 per source) that"
        echo "# crash or hang the compiler today. See tests/368-fuzz-smoke.sh."
        echo "# Regenerate with FUZZ_UPDATE_KNOWN=1 bash tests/368-fuzz-smoke.sh"
        printf '%s\n' "${crashes[@]}"
    } > "$known_file"
    echo "  wrote ${#crashes[@]} entries to $known_file"
fi

echo "  $mutants mutants x 2 targets, ${#crashes[@]} crashes or hangs, ${#new[@]} not in $known_file"
for x in "${fixed[@]}"; do echo "  no longer crashes, remove from $known_file: $x"; done
if [ "${#new[@]}" -gt 0 ] && [ "${FUZZ_UPDATE_KNOWN:-0}" != 1 ]; then
    for x in "${new[@]}"; do echo "  new crash: $x" >&2; done
    fail "${#new[@]} mutants crash the compiler; reproduce with: python3 tools/fuzz_mutate.py $seed $per_source DIR tests/diff/<program>.c"
fi
pass
