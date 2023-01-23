# ch235 - adding our source file as an included file

Closes the loop on the include-tracking bookkeeping. The source
file itself now ends up on
`preprocessor->includes` so any future `#pragma once`-style
guard can compare against canonical paths.

What landed:
- `cprocess.c`: after the top-level / nested branch picks a
  preprocessor + include_dirs, compile_process_create now
  resolves the source filename to absolute via `realpath`,
  stores it on `cfile.abs_path`, and calls
  `node_set_vector(node_vec, node_tree_vec)` so the global
  node module is bound to this process's vectors.
- `preprocessor/preprocessor.c`: `preprocessor_run` opens
  with `preprocessor_add_included_file(compiler->preprocessor,
  compiler->cfile.abs_path)`, replacing the prior
  `#warning "add our source file as an included file"` stub.

Test: `tests/165-source-as-included.sh` runs the real lex +
preprocessor pipeline on a temp source file and confirms the
preprocessor->includes vector contains exactly one entry whose
filename includes the temp source basename.
