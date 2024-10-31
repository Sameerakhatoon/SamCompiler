#include "compiler.h"

// Integer constant expressions (C11 6.6), folded where the language
// needs a value at compile time: case labels, enumerator values, array
// sizes and the initializers of globals. Each value keeps its C type, so
// `-1u > 0` and `(char)300` come out as gcc has them. Ordinary
// expressions are not folded; their code is what the backends emit.

// A floating constant (a float or double type) holds the bits of its
// value as a double, rounded to float precision for a float; the
// integer rules below do not apply to it.

// `value` cut to the width of `dtype` and extended back by its
// signedness: the canonical 64-bit form of a value of that type.
static unsigned long long constant_convert(unsigned long long value, struct datatype* dtype){
    if(datatype_is_floating(dtype)){
        return value;
    }
    size_t size = datatype_element_size(dtype);
    if(size >= DATA_SIZE_DDWORD || size == 0){
        return value;
    }
    int bits = (int)size * 8;
    unsigned long long mask = (1ULL << bits) - 1;
    value &= mask;
    if((dtype->flags & DATATYPE_FLAG_IS_SIGNED) && (value >> (bits - 1))){
        value |= ~mask;
    }
    return value;
}

// Set while folding the branch of ?: that is not taken: it only gives
// the result its type, so its errors (a division by zero) do not count.
static bool constant_quiet;

static bool constant_is_floating(struct constant* c){
    return datatype_is_floating(&c->type);
}

// The value of any arithmetic constant as a double.
static double constant_double(struct constant* c){
    if(constant_is_floating(c)){
        return floating_value(c->value, DATA_SIZE_DDWORD);
    }
    unsigned long long v = constant_convert(c->value, &c->type);
    if(c->type.flags & DATATYPE_FLAG_IS_SIGNED || datatype_element_size(&c->type) < DATA_SIZE_DDWORD){
        return (double)(long long)v;
    }
    return (double)v;
}

// `value` as a constant of the floating type dtype: a float is rounded
// to float precision.
static void constant_set_double(struct constant* out, double value, struct datatype dtype){
    if(dtype.size == DATA_SIZE_DWORD){
        value = (float)value;
    }
    out->type  = dtype;
    out->value = floating_bits(value, DATA_SIZE_DDWORD);
}

static bool constant_is_true(struct constant* c){
    if(constant_is_floating(c)){
        return constant_double(c) != 0;
    }
    return constant_convert(c->value, &c->type) != 0;
}

static void constant_set(struct constant* out, unsigned long long value, struct datatype dtype){
    out->type  = dtype;
    out->value = constant_convert(value, &dtype);
}

// A constant converted to the integer type dtype: a floating value
// truncates toward zero (C11 6.3.1.4).
static unsigned long long constant_integer_bits(struct constant* c, struct datatype* dtype){
    if(!constant_is_floating(c)){
        return c->value;
    }
    double d = constant_double(c);
    if(!(dtype->flags & DATATYPE_FLAG_IS_SIGNED) && datatype_element_size(dtype) >= DATA_SIZE_DDWORD && d >= 9223372036854775808.0){
        return (unsigned long long)d;
    }
    return (unsigned long long)(long long)d;
}

// One operator on floating operands, in the common floating type.
static bool constant_floating_operator(struct compile_process* process, const char* op, struct constant* left,
                                       struct constant* right, struct datatype* operation, struct constant* out){
    (void)process;
    double a = constant_double(left);
    double b = constant_double(right);
    if(operation->size == DATA_SIZE_DWORD){
        a = (float)a;
        b = (float)b;
    }
    int r = 0;
    if(S_EQ(op, "+"))       { constant_set_double(out, a + b, *operation); return true; }
    else if(S_EQ(op, "-"))  { constant_set_double(out, a - b, *operation); return true; }
    else if(S_EQ(op, "*"))  { constant_set_double(out, a * b, *operation); return true; }
    else if(S_EQ(op, "/"))  { constant_set_double(out, a / b, *operation); return true; }
    else if(S_EQ(op, "==")) r = a == b;
    else if(S_EQ(op, "!=")) r = a != b;
    else if(S_EQ(op, "<"))  r = a < b;
    else if(S_EQ(op, ">"))  r = a > b;
    else if(S_EQ(op, "<=")) r = a <= b;
    else if(S_EQ(op, ">=")) r = a >= b;
    else return false;
    constant_set(out, r, datatype_for_int());
    return true;
}

static bool constant_unary(struct compile_process* process, struct node* node, struct constant* out){
    const char* op = node->unary.op;
    struct constant operand;
    if(!(S_EQ(op, "-") || S_EQ(op, "~") || S_EQ(op, "!"))
       || !constant_fold(process, node->unary.operand, &operand)){
        return false;
    }
    if(S_EQ(op, "!")){
        constant_set(out, !constant_is_true(&operand), datatype_for_int());
        return true;
    }
    if(constant_is_floating(&operand)){
        if(!S_EQ(op, "-")){
            return false;
        }
        constant_set_double(out, -constant_double(&operand), operand.type);
        return true;
    }
    struct datatype type = datatype_promote(&operand.type);
    unsigned long long value = constant_convert(operand.value, &type);
    constant_set(out, S_EQ(op, "-") ? (0 - value) : ~value, type);
    return true;
}

static bool constant_signed_less(unsigned long long a, unsigned long long b, bool is_signed){
    return is_signed ? (long long)a < (long long)b : a < b;
}

// One binary operator, computed in the type the usual arithmetic
// conversions give (datatype_binary_types), as the generated code would.
static bool constant_binary_operator(struct compile_process* process, const char* op,
                                     struct constant* left, struct constant* right, struct constant* out){
    struct datatype_binary types;
    datatype_binary_types(&left->type, &right->type, op, &types);
    if(types.scale_left > 1 || types.scale_right > 1 || types.pointer_difference > 1
       || (left->type.flags & DATATYPE_FLAG_IS_POINTER) || (right->type.flags & DATATYPE_FLAG_IS_POINTER)){
        return false;
    }
    struct datatype* operation = &types.operation;
    if(datatype_is_floating(operation)){
        return constant_floating_operator(process, op, left, right, operation, out);
    }
    bool is_signed = operation->flags & DATATYPE_FLAG_IS_SIGNED;
    unsigned long long a = constant_convert(left->value, operation);
    unsigned long long b = constant_convert(right->value, operation);
    int bits = (int)datatype_element_size(operation) * 8;
    unsigned long long r = 0;
    if(S_EQ(op, "+"))       r = a + b;
    else if(S_EQ(op, "-"))  r = a - b;
    else if(S_EQ(op, "*"))  r = a * b;
    else if(S_EQ(op, "/") || S_EQ(op, "%")){
        if(constant_convert(b, operation) == 0){
            if(constant_quiet){
                return false;
            }
            compiler_error(process, "Division by zero in a constant expression\n");
        }
        if(is_signed){
            long long sa = (long long)a, sb = (long long)b;
            // The one signed overflow: the minimum divided by -1.
            if(sb == -1){
                r = S_EQ(op, "/") ? 0 - a : 0;
            } else {
                r = S_EQ(op, "/") ? (unsigned long long)(sa / sb) : (unsigned long long)(sa % sb);
            }
        } else {
            r = S_EQ(op, "/") ? a / b : a % b;
        }
    }
    else if(S_EQ(op, "<<")) r = a << (b & (bits - 1));
    else if(S_EQ(op, ">>")) r = is_signed ? (unsigned long long)((long long)a >> (b & (bits - 1))) : a >> (b & (bits - 1));
    else if(S_EQ(op, "&"))  r = a & b;
    else if(S_EQ(op, "|"))  r = a | b;
    else if(S_EQ(op, "^"))  r = a ^ b;
    else if(S_EQ(op, "==")) r = a == b;
    else if(S_EQ(op, "!=")) r = a != b;
    else if(S_EQ(op, "<"))  r = constant_signed_less(a, b, is_signed);
    else if(S_EQ(op, ">"))  r = constant_signed_less(b, a, is_signed);
    else if(S_EQ(op, "<=")) r = !constant_signed_less(b, a, is_signed);
    else if(S_EQ(op, ">=")) r = !constant_signed_less(a, b, is_signed);
    else return false;
    constant_set(out, r, types.result);
    return true;
}

static bool constant_expression(struct compile_process* process, struct node* node, struct constant* out){
    const char* op = node->exp.op;
    struct constant left;
    struct constant right;
    if(!constant_fold(process, node->exp.left, &left)){
        return false;
    }

    // && || and ?: evaluate only the operand they use, so `0 && 1 / 0`
    // is 0 and not an error.
    if(S_EQ(op, "&&") || S_EQ(op, "||")){
        bool l = constant_is_true(&left);
        if(S_EQ(op, "&&") ? !l : l){
            constant_set(out, l, datatype_for_int());
            return true;
        }
        if(!constant_fold(process, node->exp.right, &right)){
            return false;
        }
        constant_set(out, constant_is_true(&right), datatype_for_int());
        return true;
    }
    if(S_EQ(op, "?")){
        struct node* tenary = node->exp.right;
        if(tenary->type != NODE_TYPE_TENARY){
            return false;
        }
        bool take_true = constant_is_true(&left);
        struct node* taken_node = take_true ? tenary->tenary.true_node : tenary->tenary.false_node;
        struct node* other_node = take_true ? tenary->tenary.false_node : tenary->tenary.true_node;
        struct constant taken;
        struct constant other;
        if(!constant_fold(process, taken_node, &taken)){
            return false;
        }
        bool was_quiet = constant_quiet;
        constant_quiet = true;
        bool other_ok = constant_fold(process, other_node, &other);
        constant_quiet = was_quiet;
        // The result has the common type of the two branches.
        struct datatype type = datatype_promote(&taken.type);
        if(other_ok){
            struct constant zero = { 0, other.type };
            struct constant common;
            if(constant_binary_operator(process, "+", &taken, &zero, &common)){
                type = common.type;
            }
        }
        if(datatype_is_floating(&type)){
            constant_set_double(out, constant_double(&taken), type);
            return true;
        }
        constant_set(out, constant_convert(taken.value, &taken.type), type);
        return true;
    }

    if(!constant_fold(process, node->exp.right, &right)){
        return false;
    }
    return constant_binary_operator(process, op, &left, &right, out);
}

bool constant_fold(struct compile_process* process, struct node* node, struct constant* out){
    if(!node){
        return false;
    }
    switch(node->type){
        case NODE_TYPE_NUMBER: {
            struct datatype type = datatype_for_number_node(node);
            constant_set(out, node->llnum, type);
            return true;
        }
        case NODE_TYPE_EXPRESSION_PARENTHESES:
            return constant_fold(process, node->parenthesis.exp, out);
        case NODE_TYPE_UNARY:
            return constant_unary(process, node, out);
        case NODE_TYPE_EXPRESSION:
            return constant_expression(process, node, out);
        case NODE_TYPE_CAST: {
            struct constant operand;
            struct datatype* type = &node->cast.dtype;
            if(datatype_is_struct_or_union(type) && !(type->flags & DATATYPE_FLAG_IS_POINTER)){
                return false;
            }
            if(!constant_fold(process, node->cast.operand, &operand)){
                return false;
            }
            if(datatype_is_floating(type)){
                constant_set_double(out, constant_double(&operand), *type);
                return true;
            }
            if(constant_is_floating(&operand) && (type->flags & DATATYPE_FLAG_IS_POINTER)){
                return false;
            }
            constant_set(out, constant_integer_bits(&operand, type), *type);
            return true;
        }
        default:
            return false;
    }
}

// A NODE_TYPE_NUMBER that stands for the constant: its value, and its
// type promoted to int, unsigned int, long or unsigned long.
struct node* constant_node(struct constant* c){
    if(constant_is_floating(c)){
        // A floating constant, as the lexer makes one.
        node_create(&(struct node){
            .type      = NODE_TYPE_NUMBER,
            .llnum     = c->value,
            .num.flags = NUMBER_FLAG_FLOATING | (c->type.size == DATA_SIZE_DWORD ? NUMBER_FLAG_FLOAT : 0),
        });
        return node_pop();
    }
    struct datatype type = datatype_promote(&c->type);
    if(type.flags & DATATYPE_FLAG_IS_POINTER){
        type = c->type;
        type.flags &= ~DATATYPE_FLAG_IS_SIGNED;
    }
    int flags = NUMBER_FLAG_TYPED;
    if(!(type.flags & DATATYPE_FLAG_IS_SIGNED)){
        flags |= NUMBER_FLAG_UNSIGNED;
    }
    if(datatype_element_size(&type) > DATA_SIZE_DWORD || type.type == DATA_TYPE_LONG){
        flags |= NUMBER_FLAG_LONG;
    }
    // node_create also pushes the node on the parser's node stack; the
    // caller places it, so it comes off again.
    node_create(&(struct node){
        .type      = NODE_TYPE_NUMBER,
        .llnum     = constant_convert(c->value, &type),
        .num.flags = flags,
    });
    return node_pop();
}

// The constant converted to `dtype`, as storing it in an object of that
// type does (`char c = 300` holds 44).
// A floating dtype gets the double bits of the value rounded to it, so
// the caller's constant (given that type) is a floating constant; an
// integer dtype gets a floating value truncated, and _Bool its truth.
unsigned long long constant_value_as(struct constant* c, struct datatype* dtype){
    if(datatype_is_floating(dtype)){
        double d = constant_double(c);
        if(dtype->size == DATA_SIZE_DWORD){
            d = (float)d;
        }
        return floating_bits(d, DATA_SIZE_DDWORD);
    }
    if(constant_is_floating(c) && (dtype->flags & DATATYPE_FLAG_IS_BOOL)){
        return constant_is_true(c);
    }
    return constant_convert(constant_integer_bits(c, dtype), dtype);
}

bool constant_is_integer(struct constant* c){
    return !constant_is_floating(c);
}
