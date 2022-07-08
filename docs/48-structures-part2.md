# ch48 - implementing structures (part 2)

Tiny chapter: add the `_struct` payload to `struct node`'s composite
union. Carries:

- `name` - the struct's spelling.
- `body_n` - the body node (will become a NODE_TYPE_BODY filled in
  ch49+).
- `var` - optional attached variable for the
  `struct foo { ... } v;` form. NULL otherwise.

No behaviour change yet; the parser stub still walks-and-discards.
The shape just exists so ch49+ has somewhere to write.
