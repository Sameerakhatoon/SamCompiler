#!/usr/bin/env bash
# A mutation fuzzer.
# Every program in tests/diff gets one random edit per target, from a
# fixed seed so a failure reproduces: characters deleted, a token
# inserted, the file truncated, punctuation replaced, two lines swapped
# or an identifier renamed. The compiler must accept the mutant or
# reject it with an error; a signal (segfault, assert) or a timeout fails.
# SAM_FUZZ_ROUNDS raises the number of mutants per program.
. "$(dirname "$0")/lib.sh"

./build.sh >/dev/null 2>&1

tmp=$(mktemp -d /tmp/sam_fuzz.XXXXXX)
trap 'rm -rf "$tmp"' EXIT

cat > "$tmp/mutate.pl" <<'PERL'
use strict;
my ($file, $seed) = @ARGV;
srand($seed);
open(my $fh, '<', $file) or die "cannot read $file";
local $/;
my $s = <$fh>;
close $fh;
my @tokens = ('}', '{', ';', '(', ')', '*', '&', 'break;', 'continue;', 'x', '.', '->', ',',
    'return;', '"s"', 'int ', 'struct q ', '[', ']', "\n#if\n", "\n#define\n", "\n#endif\n",
    'sizeof(', '0x', "'", 'const ', 'void ', '=', '++', 'case 1:', 'default:', 'goto l;',
    'f(', '1', 'main', 'else ', 'if(', 'while(', '?', ':', 'static ', 'unsigned ', '...');
my $len = length($s);
my $op = int(rand(6));
my $at = $len ? int(rand($len)) : 0;
if ($op == 0) {
    substr($s, $at, 1 + int(rand(8))) = '';
} elsif ($op == 1) {
    substr($s, $at, 0) = $tokens[int(rand(@tokens))];
} elsif ($op == 2) {
    $s = substr($s, 0, $at);
} elsif ($op == 3) {
    my @p = grep { substr($s, $_, 1) =~ /[{}();,\[\]*&.=<>+-]/ } 0 .. $len - 1;
    if (@p) {
        my $i = $p[int(rand(@p))];
        substr($s, $i, 1) = substr('{}();,[]*&.=<>+-', int(rand(16)), 1);
    }
} elsif ($op == 4) {
    my @l = split(/\n/, $s, -1);
    if (@l > 2) {
        my ($a, $b) = (int(rand(@l)), int(rand(@l)));
        @l[$a, $b] = @l[$b, $a];
    }
    $s = join("\n", @l);
} else {
    my @w;
    while ($s =~ /\b([A-Za-z_]\w*)\b/g) { push @w, [$-[1], length($1)]; }
    if (@w) {
        my $w = $w[int(rand(@w))];
        substr($s, $w->[0], $w->[1]) = ('zz', 'int', 'struct', 'return', 'x1', 'sizeof')[int(rand(6))];
    }
}
print $s;
PERL

rounds="${SAM_FUZZ_ROUNDS:-1}"
seed=0
mutants=0
for f in tests/diff/*.c; do
    for round in $(seq 1 "$rounds"); do
        for target in x86 x86_64; do
            seed=$((seed + 1))
            perl "$tmp/mutate.pl" "$f" "$seed" > "$tmp/m.c"
            rc=0
            timeout 60 ./main -target=$target "$tmp/m.c" "$tmp/m.o" object >/dev/null 2>"$tmp/err" || rc=$?
            if [ $rc -ge 124 ] && [ $rc != 255 ]; then
                cp "$tmp/m.c" "/tmp/sam_fuzz_failure.c"
                fail "$(basename "$f") seed $seed ($target): status $rc, mutant saved as /tmp/sam_fuzz_failure.c: $(head -c 300 "$tmp/err")"
            fi
            mutants=$((mutants + 1))
        done
    done
done
[ $mutants -gt 100 ] || fail "only $mutants mutants"

pass
