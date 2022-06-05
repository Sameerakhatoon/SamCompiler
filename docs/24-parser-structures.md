# ch24 - creating our parser structures

Parser groundwork - just the data shapes, no logic yet.

- `NODE_TYPE_*` enum names every AST node kind we'll grow over the next
  ~70 chapters: expressions, statements (return / if / while / for /
  do-while / switch / case / default / goto / break / continue),
  declarations (variable / variable-list / function / body / struct /
  union / label), and helpers (unary / tenary / bracket / cast /
  blank). Note the typo'd `TENARY` (instead of `TERNARY`) is kept
  verbatim from the book and the rest of the codebase will reference
  it as such.
- `struct node` itself - the AST node:
  - `type`, `flags`, `pos` for the basics.
  - `binded { owner, function }` records where the node sits in the
    tree: its owning body, and the function it lives inside. Both NULL
    while a node is being built; the parser fills them in when it
    binds the node into the tree.
  - Anonymous union with `cval / sval / inum / lnum / llnum` for
    leaf-style nodes (NUMBER, STRING, IDENTIFIER). Composite node
    types add their own structs in later chapters.

Smoke test (`tests/20-node-struct.sh`) compiles a tiny probe that
declares a node, sets `type / llnum / pos.line`, and prints them - a
build-level sanity check that the new types compile and link.
