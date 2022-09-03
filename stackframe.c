#include <assert.h>
#include "compiler.h"
#include "helpers/vector.h"

// ch115: per-function stack-frame bookkeeping. Each push (local,
// saved register, pushed value) becomes a stack_frame_element on the
// function's frame.elements vector. Codegen / resolver later walk
// this to compute EBP offsets and assert frame-balance invariants.

void stackframe_pop(struct node* func_node){
    struct stack_frame* frame = &func_node->func.frame;
    vector_pop(frame->elements);
}

struct stack_frame_element* stackframe_back(struct node* func_node){
    return vector_back_or_null(func_node->func.frame.elements);
}

// G05-flagged: the book's parens make the type check trump the
// short-circuit, so this returns the wrong thing for a NULL element.
// We replicate verbatim and fix in g05.
struct stack_frame_element* stackframe_back_expect(struct node* func_node, int expecting_type, const char* expecting_name){
    struct stack_frame_element* element = stackframe_back(func_node);
    if(element && element->type != expecting_type || !S_EQ(element->name, expecting_name)){
        return 0;
    }
    return element;
}

void stackframe_pop_expecting(struct node* func_node, int expecting_type, const char* expecting_name){
    struct stack_frame_element* last = stackframe_back(func_node);
    assert(last);
    assert(last->type == expecting_type && S_EQ(last->name, expecting_name));
    stackframe_pop(func_node);
}

void stackframe_peek_start(struct node* func_node){
    struct stack_frame* frame = &func_node->func.frame;
    vector_set_peek_pointer(frame->elements, 0);
    vector_set_flag(frame->elements, VECTOR_FLAG_PEEK_DECREMENT);
}

struct stack_frame_element* stackframe_peek(struct node* func_node){
    struct stack_frame* frame = &func_node->func.frame;
    return vector_peek(frame->elements);
}

void stackframe_push(struct node* func_node, struct stack_frame_element* element){
    struct stack_frame* frame = &func_node->func.frame;
    // Stack grows down: each new element sits STACK_PUSH_SIZE bytes
    // below the previous one, measured from EBP.
    element->offset_from_bp = -(int)(vector_count(frame->elements) * STACK_PUSH_SIZE);
    vector_push(frame->elements, element);
}

void stackframe_sub(struct node* func_node, int type, const char* name, size_t amount){
    assert((amount % STACK_PUSH_SIZE) == 0);
    size_t total_pushes = amount / STACK_PUSH_SIZE;
    for(size_t i = 0; i < total_pushes; i++){
        stackframe_push(func_node, &(struct stack_frame_element){ .type = type, .name = name });
    }
}

void stackframe_add(struct node* func_node, int type, const char* name, size_t amount){
    (void)type; (void)name;
    assert((amount % STACK_PUSH_SIZE) == 0);
    size_t total_pushes = amount / STACK_PUSH_SIZE;
    for(size_t i = 0; i < total_pushes; i++){
        stackframe_pop(func_node);
    }
}

void stackframe_assert_empty(struct node* func_node){
    struct stack_frame* frame = &func_node->func.frame;
    assert(vector_count(frame->elements) == 0);
}
