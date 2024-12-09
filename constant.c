#include <stdlib.h>
#include <string.h>
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
            // a long double is not folded in double precision;
            // a global's initializer goes through long_double_fold.
            if(node->num.flags & NUMBER_FLAG_LONG_DOUBLE){
                return false;
            }
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
            if((datatype_is_struct_or_union(type) && !(type->flags & DATATYPE_FLAG_IS_POINTER)) || datatype_is_long_double(type)){
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

// ---------------------------------------------------------------
// long double constants
// ---------------------------------------------------------------
//
// A long double constant is worked out here to the bits of the target's
// format: the x87 80-bit extended format (a 64-bit significand with its
// integer bit, then 15 bits of exponent and the sign; 16 bytes with the
// padding) on x86 and x86_64, IEEE binary128 (a 112-bit fraction, 15
// bits of exponent and the sign) on aarch64 and riscv64. Both have the
// exponent bias 16383. A decimal constant is converted exactly and
// rounded once to nearest, ties to even, with big integers, so the
// result does not depend on the C library or the host's long double.

#define LD_LIMBS 1024

struct ld_big {
    int          n;                 // limbs in use, the top one not 0
    unsigned int d[LD_LIMBS];       // 32 bits each, least significant first
};

static void ld_big_set(struct ld_big* b, unsigned long long v){
    b->n = 0;
    while(v){
        b->d[b->n++] = (unsigned int)v;
        v >>= 32;
    }
}

static void ld_big_mul_add(struct ld_big* b, unsigned int m, unsigned int add){
    unsigned long long carry = add;
    for(int i = 0; i < b->n; i++){
        unsigned long long t = (unsigned long long)b->d[i] * m + carry;
        b->d[i] = (unsigned int)t;
        carry = t >> 32;
    }
    if(carry && b->n < LD_LIMBS){
        b->d[b->n++] = (unsigned int)carry;
    }
}

static int ld_big_bits(struct ld_big* b){
    if(b->n == 0){
        return 0;
    }
    int bits = (b->n - 1) * 32;
    for(unsigned int top = b->d[b->n - 1]; top; top >>= 1){
        bits++;
    }
    return bits;
}

static int ld_big_bit(struct ld_big* b, int i){
    return i / 32 < b->n ? (b->d[i / 32] >> (i % 32)) & 1 : 0;
}

static void ld_big_shl(struct ld_big* b, int k){
    int words = k / 32, bits = k % 32;
    int n = b->n + words + 1;
    if(n > LD_LIMBS){
        n = LD_LIMBS;
    }
    for(int i = n - 1; i >= 0; i--){
        int from = i - words;
        unsigned long long v = from >= 0 && from < b->n ? b->d[from] : 0;
        unsigned long long below = from - 1 >= 0 && from - 1 < b->n ? b->d[from - 1] : 0;
        b->d[i] = (unsigned int)((v << bits) | (bits ? below >> (32 - bits) : 0));
    }
    b->n = n;
    while(b->n && !b->d[b->n - 1]){
        b->n--;
    }
}

static int ld_big_cmp(struct ld_big* a, struct ld_big* b){
    if(a->n != b->n){
        return a->n < b->n ? -1 : 1;
    }
    for(int i = a->n - 1; i >= 0; i--){
        if(a->d[i] != b->d[i]){
            return a->d[i] < b->d[i] ? -1 : 1;
        }
    }
    return 0;
}

// a -= b, a >= b.
static void ld_big_sub(struct ld_big* a, struct ld_big* b){
    long long borrow = 0;
    for(int i = 0; i < a->n; i++){
        long long t = (long long)a->d[i] - (i < b->n ? b->d[i] : 0) - borrow;
        borrow = t < 0;
        a->d[i] = (unsigned int)(t + (borrow ? 0x100000000LL : 0));
    }
    while(a->n && !a->d[a->n - 1]){
        a->n--;
    }
}

// The value sign * q * 2^e, q (hi:lo) exact but for `sticky` (nonzero
// bits below it), in the target's long double format.
static void ld_pack(bool negative, unsigned long long hi, unsigned long long lo, int e, bool sticky, unsigned char out[16]){
    bool x87 = long_double_is_x87();
    int precision = x87 ? 64 : 113;
    int emin = -16382;
    memset(out, 0, 16);
    int bits = 0;
    for(int i = 127; i >= 0; i--){
        if(i >= 64 ? (hi >> (i - 64)) & 1 : (lo >> i) & 1){
            bits = i + 1;
            break;
        }
    }
    unsigned long long sig_hi = 0, sig_lo = 0;
    int exponent_field = 0;
    if(bits){
        // The weight of the significand's lowest bit: the value is
        // 1.f * 2^top, or a subnormal below 2^emin.
        int top = e + bits - 1;
        int low = top - (precision - 1);
        if(top < emin){
            low = emin - (precision - 1);
        }
        int drop = low - e;
        if(drop > 128){
            hi = lo = 0;
        } else if(drop > 0){
            // Round to nearest, ties to even.
            bool half = false, rest = sticky;
            for(int i = 0; i < drop && i < 128; i++){
                int bit = i >= 64 ? (int)((hi >> (i - 64)) & 1) : (int)((lo >> i) & 1);
                if(i == drop - 1){
                    half = bit;
                } else if(bit){
                    rest = true;
                }
            }
            for(int i = 0; i < drop; i++){
                lo = (lo >> 1) | (hi << 63);
                hi >>= 1;
            }
            if(half && (rest || (lo & 1))){
                lo++;
                if(lo == 0){
                    hi++;
                }
            }
        } else {
            for(int i = 0; i < -drop; i++){
                hi = (hi << 1) | (lo >> 63);
                lo <<= 1;
            }
        }
        // hi:lo is now the significand counted from 2^low; rounding up
        // may have carried it one bit further.
        bool carried = precision > 64 ? (hi >> (precision - 64)) & 1 : (precision == 64 ? hi != 0 : 0);
        if(carried){
            lo = (lo >> 1) | (hi << 63);
            hi >>= 1;
            low++;
        }
        bool normal = precision > 64 ? (hi >> (precision - 65)) & 1 : (lo >> 63) & 1;
        int exponent = low + precision - 1;
        if(exponent > 16383){
            // Infinity.
            exponent_field = 0x7FFF;
            hi = 0;
            lo = x87 ? 0x8000000000000000ULL : 0;
        } else {
            exponent_field = normal ? exponent + 16383 : 0;
        }
        sig_hi = hi;
        sig_lo = lo;
    }
    if(x87){
        for(int i = 0; i < 8; i++){
            out[i] = (unsigned char)(sig_lo >> (8 * i));
        }
        out[8] = (unsigned char)exponent_field;
        out[9] = (unsigned char)((exponent_field >> 8) | (negative ? 0x80 : 0));
        return;
    }
    // binary128: the fraction is the significand without its top bit.
    sig_hi &= 0xFFFFFFFFFFFFULL;
    for(int i = 0; i < 8; i++){
        out[i] = (unsigned char)(sig_lo >> (8 * i));
    }
    for(int i = 0; i < 6; i++){
        out[8 + i] = (unsigned char)(sig_hi >> (8 * i));
    }
    out[14] = (unsigned char)exponent_field;
    out[15] = (unsigned char)((exponent_field >> 8) | (negative ? 0x80 : 0));
}

void long_double_from_integer(unsigned long long value, bool negative, unsigned char out[16]){
    ld_pack(negative, 0, value, 0, false, out);
}

// Exactly: a double has 53 bits, both long double formats more.
void long_double_from_double(double value, unsigned char out[16]){
    unsigned long long bits = floating_bits(value, DATA_SIZE_DDWORD);
    bool negative = bits >> 63;
    int exponent = (int)((bits >> 52) & 0x7FF);
    unsigned long long fraction = bits & 0xFFFFFFFFFFFFFULL;
    if(exponent == 0x7FF){
        // Infinity or a NaN (a quiet one, keeping the top fraction bits).
        memset(out, 0, 16);
        bool x87 = long_double_is_x87();
        if(x87){
            unsigned long long sig = 0x8000000000000000ULL | (fraction << 11);
            for(int i = 0; i < 8; i++){
                out[i] = (unsigned char)(sig >> (8 * i));
            }
            out[8] = 0xFF;
            out[9] = 0x7F | (negative ? 0x80 : 0);
        } else {
            unsigned long long sig_hi = fraction >> 4;
            out[7] = (unsigned char)((fraction & 0xF) << 4);
            for(int i = 0; i < 6; i++){
                out[8 + i] = (unsigned char)(sig_hi >> (8 * i));
            }
            out[14] = 0xFF;
            out[15] = 0x7F | (negative ? 0x80 : 0);
        }
        return;
    }
    if(exponent == 0){
        ld_pack(negative, 0, fraction, -1074, false, out);
        return;
    }
    ld_pack(negative, 0, fraction | (1ULL << 52), exponent - 1075, false, out);
}

static int ld_hex_digit(char c){
    if(c >= '0' && c <= '9') return c - '0';
    if(c >= 'a' && c <= 'f') return c - 'a' + 10;
    if(c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// A floating constant's spelling, without its suffix (the lexer checked
// it): decimal with an optional e exponent, or hexadecimal with a p one.
void long_double_from_text(const char* text, unsigned char out[16]){
    static struct ld_big num, den, rem;
    bool hex = text[0] == '0' && (text[1] == 'x' || text[1] == 'X');
    const char* p = hex ? text + 2 : text;
    int base = hex ? 16 : 10;
    long exponent = 0;          // of the base (10), or of 2 for hex
    int fraction_digits = 0;
    bool in_fraction = false;
    ld_big_set(&num, 0);
    for(; *p; p++){
        if(*p == '.'){
            in_fraction = true;
            continue;
        }
        int digit = hex ? ld_hex_digit(*p) : (*p >= '0' && *p <= '9' ? *p - '0' : -1);
        if(digit < 0){
            break;
        }
        // Digits past what any long double can tell apart only make
        // the number longer: they count as a sticky 1.
        if(ld_big_bits(&num) < 4000){
            ld_big_mul_add(&num, base, digit);
            fraction_digits += in_fraction;
        } else if(!in_fraction){
            exponent += hex ? 4 : 1;
        }
    }
    if(*p == 'e' || *p == 'E' || *p == 'p' || *p == 'P'){
        exponent += strtol(p + 1, 0, 10);
    }
    if(num.n == 0){
        ld_pack(false, 0, 0, 0, false, out);
        return;
    }
    int e2 = 0;
    ld_big_set(&den, 1);
    if(hex){
        e2 = (int)exponent - 4 * fraction_digits;
    } else {
        long e10 = exponent - fraction_digits;
        if(e10 > 5000){
            ld_pack(false, 1, 0, 20000, false, out);
            return;
        }
        if(e10 < -5200){
            ld_pack(false, 0, 0, 0, false, out);
            return;
        }
        for(long i = 0; i < (e10 < 0 ? -e10 : e10); i++){
            ld_big_mul_add(e10 < 0 ? &den : &num, 10, 0);
        }
    }
    // q = num / den with 116 bits or so, and whether anything is left.
    int k = 116 - (ld_big_bits(&num) - ld_big_bits(&den));
    if(k > 0){
        ld_big_shl(&num, k);
    } else {
        ld_big_shl(&den, -k);
    }
    e2 -= k;
    unsigned long long hi = 0, lo = 0;
    ld_big_set(&rem, 0);
    for(int i = ld_big_bits(&num) - 1; i >= 0; i--){
        ld_big_shl(&rem, 1);
        if(ld_big_bit(&num, i)){
            if(rem.n == 0){
                rem.n = 1;
                rem.d[0] = 0;
            }
            rem.d[0] |= 1;
        }
        int q = 0;
        if(ld_big_cmp(&rem, &den) >= 0){
            ld_big_sub(&rem, &den);
            q = 1;
        }
        hi = (hi << 1) | (lo >> 63);
        lo = (lo << 1) | q;
    }
    ld_pack(false, hi, lo, e2, rem.n != 0, out);
}

// The value of a constant expression of arithmetic type as a long
// double: a long double constant from its spelling, other constants
// exactly from their double or integer value, and `-`, `+`, casts and
// parentheses on top. Anything else is folded as constant_fold does (in
// double) and then converted.
bool long_double_fold(struct compile_process* process, struct node* node, unsigned char out[16]){
    if(!node){
        return false;
    }
    switch(node->type){
        case NODE_TYPE_NUMBER:
            if(node->num.flags & NUMBER_FLAG_LONG_DOUBLE){
                long_double_from_text(node->num.text, out);
                return true;
            }
            break;
        case NODE_TYPE_EXPRESSION_PARENTHESES:
            return long_double_fold(process, node->parenthesis.exp, out);
        case NODE_TYPE_UNARY:
            if(S_EQ(node->unary.op, "-") || S_EQ(node->unary.op, "+")){
                if(!long_double_fold(process, node->unary.operand, out)){
                    return false;
                }
                if(S_EQ(node->unary.op, "-")){
                    out[long_double_is_x87() ? 9 : 15] ^= 0x80;
                }
                return true;
            }
            break;
        case NODE_TYPE_CAST:
            if(datatype_is_long_double(&node->cast.dtype)){
                return long_double_fold(process, node->cast.operand, out);
            }
            break;
    }
    struct constant c;
    if(!constant_fold(process, node, &c)){
        return false;
    }
    if(constant_is_floating(&c)){
        long_double_from_double(constant_double(&c), out);
        return true;
    }
    unsigned long long v = constant_convert(c.value, &c.type);
    bool is_signed = (c.type.flags & DATATYPE_FLAG_IS_SIGNED) || datatype_element_size(&c.type) < DATA_SIZE_DDWORD;
    if(is_signed && (long long)v < 0){
        long_double_from_integer(0 - v, true, out);
    } else {
        long_double_from_integer(v, false, out);
    }
    return true;
}
