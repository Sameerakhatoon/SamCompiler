#!/usr/bin/env python3
"""Write mutated copies of C programs for the fuzz smoke test.

    tools/fuzz_mutate.py SEED PER_SOURCE OUT_DIR SOURCE...

Writes PER_SOURCE files OUT_DIR/<source>.<k>.c for every source. Each is
the source with one to three random edits: delete, duplicate or swap a
token, insert a stray token, delete or duplicate a line, or cut the file
short. The random state of <source>.<k> depends only on SEED, the
source's file name and k, so the same mutant comes back on every run and
on every machine, and adding a source does not change the others.
"""
import os
import random
import re
import sys

TOKEN = re.compile(r'[A-Za-z_][A-Za-z_0-9]*|0[xX][0-9a-fA-F]+|[0-9]+|"(?:\\.|[^"\\\n])*"|'
                   r"'(?:\\.|[^'\\\n])*'|<<=|>>=|->|\+\+|--|<<|>>|<=|>=|==|!=|&&|\|\||"
                   r'[-+*/%&|^]=|##|\.\.\.|\S')
STRAY = ['{', '}', '(', ')', '[', ']', ';', ',', '*', '&', '=', '+', '-', '<', '>', '?', ':',
         '#', '.', '->', '"', "'", '0', '1', '0x', 'int', 'char', 'struct', 'union', 'enum',
         'return', 'if', 'else', 'while', 'for', 'switch', 'case', 'default', 'break',
         'continue', 'goto', 'sizeof', 'typedef', 'static', 'unsigned', 'void', 'x',
         '#if', '#endif', '#define', '#include', '\n']


def tokens(text):
    return [(m.start(), m.end()) for m in TOKEN.finditer(text)]


def mutate(rng, text):
    toks = tokens(text)
    lines = text.split('\n')
    kind = rng.randrange(7)
    if kind <= 3 and toks:
        start, end = toks[rng.randrange(len(toks))]
        if kind == 0:   # delete a token
            return text[:start] + text[end:]
        if kind == 1:   # duplicate a token
            return text[:end] + ' ' + text[start:end] + text[end:]
        if kind == 2:   # insert a stray token before it
            return text[:start] + rng.choice(STRAY) + ' ' + text[start:]
        # swap with the next token
        i = toks.index((start, end))
        if i + 1 < len(toks):
            s2, e2 = toks[i + 1]
            return text[:start] + text[s2:e2] + text[end:s2] + text[start:end] + text[e2:]
        return text[:start]
    if kind == 4 and len(lines) > 1:
        del lines[rng.randrange(len(lines))]
        return '\n'.join(lines)
    if kind == 5 and lines:
        i = rng.randrange(len(lines))
        lines.insert(i, lines[i])
        return '\n'.join(lines)
    return text[:rng.randrange(len(text) + 1)]


def main():
    if len(sys.argv) < 5:
        sys.exit(__doc__)
    seed, per_source, out_dir = int(sys.argv[1]), int(sys.argv[2]), sys.argv[3]
    os.makedirs(out_dir, exist_ok=True)
    for src in sorted(sys.argv[4:]):
        base = os.path.basename(src)[:-2] if src.endswith('.c') else os.path.basename(src)
        with open(src) as f:
            original = f.read()
        for k in range(per_source):
            rng = random.Random('%d:%s:%d' % (seed, base, k))
            text = original
            for _ in range(rng.randint(1, 3)):
                text = mutate(rng, text)
            with open(os.path.join(out_dir, '%s.%d.c' % (base, k)), 'w') as f:
                f.write(text)


if __name__ == '__main__':
    main()
