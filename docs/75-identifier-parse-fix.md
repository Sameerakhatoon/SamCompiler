# ch75 - changing the parsing of identifiers

Upstream this chapter ships the exact fix that SamCompiler shipped as
**g01** way back at ch32: `assert(... == TOKEN_TYPE_IDENTIFIER)`
instead of `NODE_TYPE_IDENTIFIER`. The book chose to leave the bug in
for 43 chapters and fix it cosmetically here.

No code change needed - we fixed this immediately at g01. Marker
chapter only.
