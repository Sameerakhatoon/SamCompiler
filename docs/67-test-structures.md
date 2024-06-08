# ch67 - testing our structure functionality

No code change in this chapter - it's a coverage round on the post-ch64
struct path. The book's test.c becomes
`struct dog { int x; int y; };` to demonstrate the end-to-end flow.

Smoke test (`tests/44-named-struct-body.sh`) feeds the same input and
asserts:
- `symresolver_get_symbol(cp, "dog")` finds it.
- `body_n->body.size == 8` (two ints).
- Two statements in the body.
