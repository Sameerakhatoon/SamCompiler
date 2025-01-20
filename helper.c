#include <assert.h>
#include "compiler.h"
#include "helpers/vector.h"

// Misc parser-side helpers, ch52+.

size_t variable_size(struct node* var_node){
    assert(var_node->type == NODE_TYPE_VARIABLE);
    return datatype_size(&var_node->var.type);
}

// For a struct/union variable, follow the type back to its body node.
struct node* variable_struct_or_union_body_node(struct node* node){
    if(!node_is_struct_or_union_variable(node) || !node->var.type.struct_node){
        return 0;
    }
    if(node->var.type.type == DATA_TYPE_STRUCT){
        // none yet for a struct only declared (`struct x;`).
        return node->var.type.struct_node ? node->var.type.struct_node->_struct.body_n : 0;
    }
    return node->var.type.union_node->_union.body_n;
}

// Sum the sizes of every variable in a NODE_TYPE_VARIABLE_LIST.
size_t variable_size_for_list(struct node* var_list_node){
    assert(var_list_node->type == NODE_TYPE_VARIABLE_LIST);
    size_t size = 0;
    vector_set_peek_pointer(var_list_node->var_list.list, 0);
    struct node* v = vector_peek_ptr(var_list_node->var_list.list);
    while(v){
        size += variable_size(v);
        v = vector_peek_ptr(var_list_node->var_list.list);
    }
    return size;
}

// Bytes of padding needed so that `val` becomes a multiple of `to`.
int padding(int val, int to){
    if(to <= 0){
        return 0;
    }
    if((val % to) == 0){
        return 0;
    }
    return to - (val % to) % to;
}

// Round `val` up to the nearest multiple of `to`.
int align_value(int val, int to){
    // a void* local is "aligned" to its pointee's size, 0.
    if(to > 0 && val % to){
        val += padding(val, to);
    }
    return val;
}

// Same, but negative inputs align toward negative-infinity.
int align_value_treat_positive(int val, int to){
    assert(to >= 0);
    if(val < 0){
        to = -to;
    }
    return align_value(val, to);
}

// Sum every variable node's padding field across a body's statements.
int compute_sum_padding(struct vector* vec){
    int total = 0;
    vector_set_peek_pointer(vec, 0);
    struct node* cur = vector_peek_ptr(vec);
    while(cur){
        if(cur->type == NODE_TYPE_VARIABLE){
            total += cur->var.padding;
        }
        cur = vector_peek_ptr(vec);
    }
    return total;
}

// ch119: walk the array bracket vector starting at index+1 and
// multiply each declared dimension into index_value. Returns the
// element multiplier for index `index` into a (possibly multi-dim)
// array. Non-array datatype returns index_value unchanged.
int array_multiplier(struct datatype* dtype, int index, int index_value){
    if(!(dtype->flags & DATATYPE_FLAG_IS_ARRAY)){
        return index_value;
    }
    vector_set_peek_pointer(dtype->array.brackets->n_brackets, index + 1);
    int size_sum = index_value;
    struct node* bracket = vector_peek_ptr(dtype->array.brackets->n_brackets);
    while(bracket){
        assert(bracket->bracket.inner->type == NODE_TYPE_NUMBER);
        int declared = bracket->bracket.inner->llnum;
        size_sum *= declared;
        bracket = vector_peek_ptr(dtype->array.brackets->n_brackets);
    }
    return size_sum;
}

// ch124: struct field-offset walk. Iterates the struct body's
// variable nodes (forward or backward), summing sizes and aligning to
// each new member, until it finds `var_name` (or runs out). Sets
// *var_node_out to whichever variable node it stopped on.
struct node* body_largest_variable_node(struct node* body_node){
    if(!body_node){
        return 0;
    }
    if(body_node->type != NODE_TYPE_BODY){
        return 0;
    }
    return body_node->body.largest_var_node;
}

struct node* variable_struct_or_union_largest_variable_node(struct node* var_node){
    return body_largest_variable_node(variable_struct_or_union_body_node(var_node));
}

static struct node* struct_member_direct(struct vector* statements, const char* name){
    vector_set_peek_pointer(statements, 0);
    struct node* statement = vector_peek_ptr(statements);
    while(statement){
        if(statement->type == NODE_TYPE_VARIABLE && S_EQ(statement->var.name, name)){
            return statement;
        }
        if(statement->type == NODE_TYPE_VARIABLE_LIST){
            struct vector* list = statement->var_list.list;
            vector_set_peek_pointer(list, 0);
            struct node* member = vector_peek_ptr(list);
            while(member){
                if(S_EQ(member->var.name, name)){
                    return member;
                }
                member = vector_peek_ptr(list);
            }
        }
        statement = vector_peek_ptr(statements);
    }
    return 0;
}

// The member called `name` among a body's statements, looking inside
// `int a, b;` lists too; NULL if there is none. The members of an
// anonymous struct or union member are members too (C11 6.7.2.1p13):
// one found through it comes back as a copy whose aoffset is from the
// start of this body. Copies are kept per (anonymous member, name).
struct struct_member_alias {
    struct node*                anonymous;
    const char*                 name;
    struct node*                copy;
    struct struct_member_alias* next;
};
static struct struct_member_alias* struct_member_aliases;

struct node* struct_member_named(struct vector* statements, const char* name){
    struct node* member = struct_member_direct(statements, name);
    if(member){
        return member;
    }
    for(int i = 0; i < vector_count(statements); i++){
        struct node* anonymous = *(struct node**)vector_at(statements, i);
        if(anonymous->type != NODE_TYPE_VARIABLE || !(anonymous->flags & NODE_FLAG_ANONYMOUS_MEMBER)){
            continue;
        }
        struct node* body = variable_struct_or_union_body_node(anonymous);
        struct node* inner = body ? struct_member_named(body->body.statements, name) : 0;
        if(!inner){
            continue;
        }
        for(struct struct_member_alias* alias = struct_member_aliases; alias; alias = alias->next){
            if(alias->anonymous == anonymous && S_EQ(alias->name, name)){
                return alias->copy;
            }
        }
        struct struct_member_alias* alias = calloc(1, sizeof(struct struct_member_alias));
        alias->anonymous = anonymous;
        alias->name      = inner->var.name;
        alias->copy      = malloc(sizeof(struct node));
        *alias->copy     = *inner;
        alias->copy->var.aoffset += anonymous->var.aoffset;
        alias->next      = struct_member_aliases;
        struct_member_aliases = alias;
        return alias->copy;
    }
    return 0;
}

int struct_offset(struct compile_process* compile_proc, const char* struct_name, const char* var_name, struct node** var_node_out, int last_pos, int flags){
    struct symbol* struct_sym = symresolver_get_tag(compile_proc, struct_name);
    // A member of a struct not defined yet (`static struct s v;` before
    // `struct s {...}`) failed these asserts.
    if(!struct_sym || struct_sym->type != SYMBOL_TYPE_NODE || !node_is_struct_or_union(struct_sym->data)){
        compiler_error(compile_proc, "struct %s is not defined here, so it has no member %s", struct_name, var_name);
    }
    struct node* node = struct_sym->data;

    (void)flags;
    if(!node->_struct.body_n){
        compiler_error(compile_proc, "%s is an incomplete type, it has no member %s", struct_name, var_name);
    }
    // The parser recorded each member's offset when it laid the body out
    // (parser_scope_offset_for_structure; union members are at 0). This
    // used to recompute the offsets with different rules, and those did
    // not align a nested struct or count a whole array.
    *var_node_out = struct_member_named(node->_struct.body_n->body.statements, var_name);
    if(!*var_node_out){
        compiler_error(compile_proc, "%s has no member named %s", struct_name, var_name);
    }
    return last_pos + (*var_node_out)->var.aoffset;
}

// ch127: access / array / parens operator + node predicates. Moved
// is_array_node here so all three families live together.
bool is_access_operator(const char* op){
    return S_EQ(op, "->") || S_EQ(op, ".");
}

bool is_access_node(struct node* node){
    return node->type == NODE_TYPE_EXPRESSION && is_access_operator(node->exp.op);
}

bool is_access_node_with_op(struct node* node, const char* op){
    return is_access_node(node) && S_EQ(node->exp.op, op);
}

bool is_array_operator(const char* op){
    return S_EQ(op, "[]");
}

bool is_array_node(struct node* node){
    return node->type == NODE_TYPE_EXPRESSION && is_array_operator(node->exp.op);
}

bool is_parentheses_operator(const char* op){
    return S_EQ(op, "()");
}

bool is_parentheses_node(struct node* node){
    return node->type == NODE_TYPE_EXPRESSION && is_parentheses_operator(node->exp.op);
}

// ch128: argument (comma) predicates.
bool is_argument_operator(const char* op){
    return S_EQ(op, ",");
}

bool is_argument_node(struct node* node){
    return node->type == NODE_TYPE_EXPRESSION && is_argument_operator(node->exp.op);
}

// ch130: unary operator predicates.
bool is_unary_operator(const char* op){
    return S_EQ(op, "-") || S_EQ(op, "!") || S_EQ(op, "~")
        || S_EQ(op, "*") || S_EQ(op, "&") || S_EQ(op, "+")
        || S_EQ(op, "++") || S_EQ(op, "--");
}

bool op_is_indirection(const char* op){
    return S_EQ(op, "*");
}

bool is_increment_operator(const char* op){
    return S_EQ(op, "++") || S_EQ(op, "--");
}

const char* assignment_arithmetic_operator(const char* op){
    static const char* table[][2] = {
        { "+=", "+" },   { "-=", "-" },   { "*=", "*" },  { "/=", "/" },
        { "%=", "%" },   { "<<=", "<<" }, { ">>=", ">>" },
        { "&=", "&" },   { "|=", "|" },   { "^=", "^" },
        { "++", "+" },   { "--", "-" },
    };
    for(size_t i = 0; i < sizeof(table) / sizeof(table[0]); i++){
        if(S_EQ(op, table[i][0])){
            return table[i][1];
        }
    }
    return 0;
}

// ch132: address-of unary.
bool op_is_address(const char* op){
    return S_EQ(op, "&");
}

// ch146: return whichever of the two dtypes is a pointer, or NULL
// if neither is. Tie-breaker is d1.
struct datatype* datatype_thats_a_pointer(struct datatype* d1, struct datatype* d2){
    if(d1->flags & DATATYPE_FLAG_IS_POINTER){ return d1; }
    if(d2->flags & DATATYPE_FLAG_IS_POINTER){ return d2; }
    return 0;
}

// ch147: logical operator predicates.
bool is_logical_operator(const char* op){
    return S_EQ(op, "&&") || S_EQ(op, "||");
}

bool is_logical_node(struct node* node){
    return node->type == NODE_TYPE_EXPRESSION && is_logical_operator(node->exp.op);
}

// ch146: clone dtype, drop pointer_depth by `by`, and clear
// IS_POINTER once we hit zero or below.
struct datatype* datatype_pointer_reduce(struct datatype* datatype, int by){
    struct datatype* new_dtype = calloc(1, sizeof(struct datatype));
    memcpy(new_dtype, datatype, sizeof(struct datatype));
    new_dtype->pointer_depth -= by;
    if(new_dtype->pointer_depth <= 0){
        new_dtype->flags &= ~DATATYPE_FLAG_IS_POINTER;
        new_dtype->pointer_depth = 0;
    }
    return new_dtype;
}

// ch138: synthetic datatype for numeric literals (int, 4 bytes,
// IS_LITERAL flag set).
struct datatype datatype_for_numeric(void){
    struct datatype dtype = {0};
    // A plain constant is a signed int (C11 6.4.4.1); without the flag
    // `a / 5` divided unsigned.
    dtype.flags    |= DATATYPE_FLAG_IS_LITERAL | DATATYPE_FLAG_IS_SIGNED;
    dtype.type      = DATA_TYPE_INTEGER;
    dtype.type_str  = "int";
    dtype.size      = DATA_SIZE_DWORD;
    return dtype;
}

// A computed int: the type of `!x`, a comparison, or `a && b`.
struct datatype datatype_for_int(void){
    struct datatype dtype = datatype_for_numeric();
    dtype.flags &= ~DATATYPE_FLAG_IS_LITERAL;
    return dtype;
}

// ch176: tiny predicates for the parser-side unary-operand check.
bool is_parentheses(const char* op){
    return S_EQ(op, "(");
}

// Postfix ++ and -- bind tighter than any prefix operator, so `-x++`
// is -(x++) and `*p++` is *(p++). The bracket token is "[";
// is_array_operator matches the "[]" node operator, so with it here
// `&a[1]` parsed as (&a)[1] and `-a[1]` as (-a)[1].
bool unary_operand_compatible(struct token* token){
    return is_access_operator(token->sval)
        || S_EQ(token->sval, "[")
        || is_parentheses(token->sval)
        || is_increment_operator(token->sval);
}

// ch170: synthetic datatype for string literals - `const char*`
// flagged IS_LITERAL.
struct datatype datatype_for_string(void){
    struct datatype dtype = {0};
    dtype.type          = DATA_TYPE_INTEGER;
    dtype.type_str      = "char";
    dtype.flags        |= DATATYPE_FLAG_IS_POINTER | DATATYPE_FLAG_IS_LITERAL;
    dtype.pointer_depth = 1;
    // A pointer records the size of what it points to, as `char* p`
    // does; datatype_size and datatype_element_size give the pointer's
    // own size. The pointer size here made "abc" + 1 step by 4 or 8.
    dtype.size          = DATA_SIZE_BYTE;
    return dtype;
}

// ch129: drop one level of pointer-depth; clear IS_POINTER once we
// fall to 0 or below.
void datatype_decrement_pointer(struct datatype* dtype){
    dtype->pointer_depth--;
    if(dtype->pointer_depth <= 0){
        dtype->flags &= ~DATATYPE_FLAG_IS_POINTER;
    }
}

// ch119: byte offset for the index-th access into dtype.
int array_offset(struct datatype* dtype, int index, int index_value){
    if(!(dtype->flags & DATATYPE_FLAG_IS_ARRAY)
       || (index == (int)vector_count(dtype->array.brackets->n_brackets) - 1)){
        return index_value * datatype_element_size(dtype);
    }
    return array_multiplier(dtype, index, index_value) * datatype_element_size(dtype);
}

// ch213: arithmetic shim used by the preprocessor expression evaluator.
// Returns the result of left op right for a fixed set of operators;
// writes false to *success and returns 0 when op is unsupported.
// `%`, `&`, `|` and `^` were missing, so `#if 7 % 4 == 3` failed, the
// result was cut to an int, and dividing by zero crashed the compiler.
long arithmetic(struct compile_process* compiler, long left_operand, long right_operand, const char* op, bool* success){
    *success = true;
    long result = 0;
    if((S_EQ(op, "/") || S_EQ(op, "%")) && right_operand == 0){
        compiler_error(compiler, "Division by zero in a preprocessor expression\n");
    }
    if(S_EQ(op, "*")){
        result = left_operand * right_operand;
    } else if(S_EQ(op, "/")){
        result = left_operand / right_operand;
    } else if(S_EQ(op, "%")){
        result = left_operand % right_operand;
    } else if(S_EQ(op, "&")){
        result = left_operand & right_operand;
    } else if(S_EQ(op, "|")){
        result = left_operand | right_operand;
    } else if(S_EQ(op, "^")){
        result = left_operand ^ right_operand;
    } else if(S_EQ(op, "+")){
        result = left_operand + right_operand;
    } else if(S_EQ(op, "-")){
        result = left_operand - right_operand;
    } else if(S_EQ(op, "==")){
        result = left_operand == right_operand;
    } else if(S_EQ(op, "!=")){
        result = left_operand != right_operand;
    } else if(S_EQ(op, ">")){
        result = left_operand > right_operand;
    } else if(S_EQ(op, "<")){
        result = left_operand < right_operand;
    } else if(S_EQ(op, ">=")){
        result = left_operand >= right_operand;
    } else if(S_EQ(op, "<=")){
        result = left_operand <= right_operand;
    } else if(S_EQ(op, "<<")){
        result = left_operand << right_operand;
    } else if(S_EQ(op, ">>")){
        result = left_operand >> right_operand;
    } else if(S_EQ(op, "&&")){
        result = left_operand && right_operand;
    } else if(S_EQ(op, "||")){
        result = left_operand || right_operand;
    } else {
        *success = false;
    }
    return result;
}

// ch229: probe whether a path refers to an openable file.
bool file_exists(const char* filename){
    FILE* f = fopen(filename, "r");
    if(!f){
        return false;
    }
    fclose(f);
    return true;
}

// The one place that resolves a path. realpath is POSIX; Windows has
// _fullpath, which makes the path absolute without resolving links.
bool compiler_real_path(const char* path, char* out){
#ifdef _WIN32
    return _fullpath(out, path, PATH_MAX) != NULL;
#else
    return realpath(path, out) != NULL;
#endif
}
