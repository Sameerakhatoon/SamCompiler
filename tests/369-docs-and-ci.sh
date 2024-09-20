#!/usr/bin/env bash
# CI, and docs that did not match the
# code. Checks the CI workflow parses and runs the same commands as a
# local run, and that the false statements found earlier stay gone.
. "$(dirname "$0")/lib.sh"

ci=.github/workflows/ci.yml
[ -f "$ci" ] || fail "no $ci"
if python3 -c 'import yaml' 2>/dev/null; then
    steps=$(python3 - "$ci" <<'EOF'
import sys, yaml
doc = yaml.safe_load(open(sys.argv[1]))
for step in doc["jobs"]["test"]["steps"]:
    if "run" in step:
        print(step["run"].strip())
EOF
) || fail "$ci does not parse"
else
    steps=$(cat "$ci")
fi
assert_contains "$steps" "./build.sh clean" "CI builds"
assert_contains "$steps" "bash tools/gcc_differential.sh" "CI runs the gcc differential"
assert_contains "$steps" "bash tests/run-all.sh" "CI runs the suite"
for pkg in nasm gcc-i686-linux-gnu gcc-aarch64-linux-gnu gcc-riscv64-linux-gnu qemu-user python3; do
    assert_contains "$steps" "$pkg" "CI installs"
done
case "$steps" in *ALLOW_MISSING_TOOLCHAIN=1*) fail "CI must not allow missing toolchains" ;; esac

# Docs that did not match the code.
using=docs/guides/USING_THE_COMPILER.md
grep -q 'no nasm, no gcc' "$using" && fail "$using says object mode skips nasm"
grep -q 'it includes line:col' "$using" && fail "$using claims file:line:col errors"
grep -q 'errors, missing identifiers' "$using" && fail "$using claims the validator catches every missing identifier"
grep -q 'nasm` + `gcc -m32`' docs/guides/FEATURE_TESTING.md && fail "FEATURE_TESTING.md says the link uses gcc -m32"
grep -q 'create/teardown' README.md && fail "README.md says cprocess.c tears down"
grep -q 'clean + make' README.md && fail "README.md says build.sh cleans"
grep -q 'Right now just emits a placeholder' compiler.c && fail "compiler.c codegen comment is stale"
grep -q 'Currently scaffolded - returns ALL_OK' compiler.c && fail "compiler.c validator comment is stale"
grep -q 'just stop at the .asm' main.c && fail "main.c driver comment is stale"

pass
