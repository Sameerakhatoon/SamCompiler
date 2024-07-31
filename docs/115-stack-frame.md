# ch115 - building the stack frame functionality

New `stackframe.c` module + matching `compiler.h` declarations. No
caller yet; the codegen wires it up in later chapters.

What landed in `compiler.h`:
- `struct stack_frame_data { struct datatype dtype; }`.
- `struct stack_frame_element { flags, type, name, offset_from_bp,
   data }`.
- `struct stack_frame { struct vector* elements; }` nested inside
  `func` in the node union.
- `STACK_PUSH_SIZE` (= 4) and the enums
  `STACK_FRAME_ELEMENT_TYPE_*` and `STACK_FRAME_ELEMENT_FLAG_*`.
- Forward decls for stackframe_pop / back / back_expect /
  pop_expecting / peek_start / peek / push / sub / add /
  assert_empty.

What landed in `stackframe.c`:
- `stackframe_push`: writes `offset_from_bp = -count *
  STACK_PUSH_SIZE` (stack grows down) and pushes onto
  `func.frame.elements`.
- `stackframe_pop`: drops the top element.
- `stackframe_back` / `stackframe_back_expect`: most-recent element
  (the latter type+name-checks; G05 documents an upstream operator-
  precedence bug we'll patch shortly).
- `stackframe_pop_expecting`: assert before pop.
- `stackframe_peek_start` / `stackframe_peek`: iterate top-down via
  `VECTOR_FLAG_PEEK_DECREMENT`.
- `stackframe_sub(amount)` / `stackframe_add(amount)`: push / pop
  `amount / STACK_PUSH_SIZE` synthetic elements (`amount` must be a
  multiple of `STACK_PUSH_SIZE`).
- `stackframe_assert_empty`: assert the frame is balanced.

Build wiring: new `stackframe.o` target added to `Makefile`.

Test: `tests/65-stackframe-basic.sh` builds a NODE_TYPE_FUNCTION,
allocates its `frame.elements` vector by hand (no caller initializes
it yet), and checks push / back / add / sub behavior plus the
expected `-STACK_PUSH_SIZE` offset stepping.
