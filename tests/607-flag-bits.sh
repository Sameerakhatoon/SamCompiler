#!/usr/bin/env bash
# The flags of one family share a flags word, so each must be its own
# bit: two changes made side by side twice gave a new COMPILE_PROCESS_* flag a bit another
# one already had. Every constant of
# each family is read from compiler.h, its value printed by a program
# that includes compiler.h, and checked to be a single bit that no other
# member of the family has. A new member is checked without touching
# this test.
. "$(dirname "$0")/lib.sh"
need_tools gcc

tmp=$(mktemp -d /tmp/sam_flag_bits.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

families="COMPILE_PROCESS_ NODE_FLAG_ FUNCTION_NODE_FLAG_ DATATYPE_FLAG_ RESOLVER_ENTITY_FLAG_ RESOLVER_RESULT_FLAG_"

{
    echo '#include <stdio.h>'
    echo '#include "compiler.h"'
    echo 'int main(void){'
    for family in $families; do
        # Enumerators are defined as `NAME = value` at the start of a line.
        grep -oE "^[[:space:]]+${family}[A-Z0-9_]+[[:space:]]*=" compiler.h \
            | tr -d ' \t=' | sort -u \
            | while read -r name; do
                echo "    printf(\"$family %s %llu\\n\", \"$name\", (unsigned long long)$name);"
            done
    done
    echo '    return 0;'
    echo '}'
} > "$tmp/flags.c"

gcc -I"$REPO_ROOT" -I"$REPO_ROOT/helpers" -w -o "$tmp/flags" "$tmp/flags.c" 2>"$tmp/gcc.log" \
    || fail "the flag listing did not compile: $(head -5 "$tmp/gcc.log")"
"$tmp/flags" > "$tmp/values"

for family in $families; do
    count=$(grep -c "^$family " "$tmp/values" || true)
    [ "$count" -gt 1 ] || fail "found $count flags of family $family in compiler.h"
done

# One bit each, and no bit twice in a family.
declare -A seen
report=""
while read -r family name value; do
    if [ "$value" = 0 ] || [ $(( value & (value - 1) )) != 0 ]; then
        report="$report $name is $value, not a single bit;"
    fi
    key="$family$value"
    if [ -n "${seen[$key]:-}" ]; then
        report="$report ${seen[$key]} and $name are the same bit ($value);"
    fi
    seen[$key]=$name
done < "$tmp/values"
[ -z "$report" ] || fail "$report"
pass
