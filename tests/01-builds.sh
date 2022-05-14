#!/usr/bin/env bash
# Ch4: the project builds cleanly with no warnings/errors.
. "$(dirname "$0")/lib.sh"

out="$(./build.sh 2>&1)"
if ! [ -x ./main ]; then
    fail "./main was not produced. build output: $out"
fi
pass
