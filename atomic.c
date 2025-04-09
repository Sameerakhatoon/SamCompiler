#include <stdio.h>
#include <string.h>
#include "compiler.h"

// gcc's __atomic and __sync builtins, which <stdatomic.h>
// is written in. Each is a native function (preprocessor/native.c);
// exp64_atomic_builtin generates them on the 64-bit targets and
// codegen_x86_native_atomic on i386. A constant memory order argument
// picks the instructions (atomic_order); any other is
// evaluated and the operation is sequentially consistent.
static const struct atomic_builtin atomic_builtins[] = {
    { "__atomic_load_n",              ATOMIC_BUILTIN_LOAD,             0,      2 },
    { "__atomic_load",                ATOMIC_BUILTIN_GENERIC_LOAD,     0,      3 },
    { "__atomic_store_n",             ATOMIC_BUILTIN_STORE,            0,      3 },
    { "__atomic_store",               ATOMIC_BUILTIN_GENERIC_STORE,    0,      3 },
    { "__atomic_exchange_n",          ATOMIC_BUILTIN_EXCHANGE,         0,      3 },
    { "__atomic_exchange",            ATOMIC_BUILTIN_GENERIC_EXCHANGE, 0,      4 },
    { "__atomic_compare_exchange_n",  ATOMIC_BUILTIN_CAS,              0,      6 },
    { "__atomic_compare_exchange",    ATOMIC_BUILTIN_GENERIC_CAS,      0,      6 },
    { "__atomic_fetch_add",           ATOMIC_BUILTIN_FETCH_OP,         "+",    3 },
    { "__atomic_fetch_sub",           ATOMIC_BUILTIN_FETCH_OP,         "-",    3 },
    { "__atomic_fetch_and",           ATOMIC_BUILTIN_FETCH_OP,         "&",    3 },
    { "__atomic_fetch_or",            ATOMIC_BUILTIN_FETCH_OP,         "|",    3 },
    { "__atomic_fetch_xor",           ATOMIC_BUILTIN_FETCH_OP,         "^",    3 },
    { "__atomic_fetch_nand",          ATOMIC_BUILTIN_FETCH_OP,         "nand", 3 },
    { "__atomic_add_fetch",           ATOMIC_BUILTIN_OP_FETCH,         "+",    3 },
    { "__atomic_sub_fetch",           ATOMIC_BUILTIN_OP_FETCH,         "-",    3 },
    { "__atomic_and_fetch",           ATOMIC_BUILTIN_OP_FETCH,         "&",    3 },
    { "__atomic_or_fetch",            ATOMIC_BUILTIN_OP_FETCH,         "|",    3 },
    { "__atomic_xor_fetch",           ATOMIC_BUILTIN_OP_FETCH,         "^",    3 },
    { "__atomic_nand_fetch",          ATOMIC_BUILTIN_OP_FETCH,         "nand", 3 },
    { "__atomic_test_and_set",        ATOMIC_BUILTIN_TEST_AND_SET,     0,      2 },
    { "__atomic_clear",               ATOMIC_BUILTIN_CLEAR,            0,      2 },
    { "__atomic_thread_fence",        ATOMIC_BUILTIN_FENCE,            0,      1 },
    { "__atomic_signal_fence",        ATOMIC_BUILTIN_SIGNAL_FENCE,     0,      1 },
    { "__atomic_always_lock_free",    ATOMIC_BUILTIN_LOCK_FREE,        0,      2 },
    { "__atomic_is_lock_free",        ATOMIC_BUILTIN_LOCK_FREE,        0,      2 },
    { "__sync_fetch_and_add",         ATOMIC_BUILTIN_FETCH_OP,         "+",    2 },
    { "__sync_fetch_and_sub",         ATOMIC_BUILTIN_FETCH_OP,         "-",    2 },
    { "__sync_fetch_and_and",         ATOMIC_BUILTIN_FETCH_OP,         "&",    2 },
    { "__sync_fetch_and_or",          ATOMIC_BUILTIN_FETCH_OP,         "|",    2 },
    { "__sync_fetch_and_xor",         ATOMIC_BUILTIN_FETCH_OP,         "^",    2 },
    { "__sync_fetch_and_nand",        ATOMIC_BUILTIN_FETCH_OP,         "nand", 2 },
    { "__sync_add_and_fetch",         ATOMIC_BUILTIN_OP_FETCH,         "+",    2 },
    { "__sync_sub_and_fetch",         ATOMIC_BUILTIN_OP_FETCH,         "-",    2 },
    { "__sync_and_and_fetch",         ATOMIC_BUILTIN_OP_FETCH,         "&",    2 },
    { "__sync_or_and_fetch",          ATOMIC_BUILTIN_OP_FETCH,         "|",    2 },
    { "__sync_xor_and_fetch",         ATOMIC_BUILTIN_OP_FETCH,         "^",    2 },
    { "__sync_nand_and_fetch",        ATOMIC_BUILTIN_OP_FETCH,         "nand", 2 },
    { "__sync_val_compare_and_swap",  ATOMIC_BUILTIN_CAS_VALUE,        0,      3 },
    { "__sync_bool_compare_and_swap", ATOMIC_BUILTIN_CAS_BOOL,         0,      3 },
    { "__sync_lock_test_and_set",     ATOMIC_BUILTIN_EXCHANGE,         0,      2 },
    { "__sync_lock_release",          ATOMIC_BUILTIN_RELEASE,          0,      1 },
    { "__sync_synchronize",           ATOMIC_BUILTIN_FENCE,            0,      0 },
    { 0 },
};

const struct atomic_builtin* atomic_builtin_get(const char* name){
    if(!name || (strncmp(name, "__atomic_", 9) && strncmp(name, "__sync_", 7))){
        return 0;
    }
    for(const struct atomic_builtin* b = atomic_builtins; b->name; b++){
        if(S_EQ(b->name, name)){
            return b;
        }
    }
    return 0;
}

// Every builtin as a native function, so a call to one resolves (and
// the validator accepts it) without a declaration, as in gcc.
void atomic_builtins_register(struct compile_process* process){
    for(const struct atomic_builtin* b = atomic_builtins; b->name; b++){
        if(!native_function_get(process, b->name)){
            native_create_function(process, b->name, &(struct native_function_callbacks){ .call = codegen_x86_native_atomic });
        }
    }
}

// NASM only takes a global symbol in a TLS reference (`wrt
// ..gottpoff`, `wrt ..tlsie`), and a static thread-local variable has a
// local one. The two x86 backends give each thread-local variable they
// define a hidden global alias, unique to the source file, and refer to
// it through that: `name.tls.<hash of the file's path>`.
const char* tls_alias(struct compile_process* process, const char* name){
    static char buffers[4][IDENTIFIER_MAX_LENGTH + 32];
    static int next;
    unsigned hash = 2166136261u;
    const char* path = process->cfile.abs_path ? process->cfile.abs_path : "";
    for(const unsigned char* p = (const unsigned char*)path; *p; p++){
        hash = (hash ^ *p) * 16777619u;
    }
    char* out = buffers[next++ % 4];
    snprintf(out, sizeof(buffers[0]), "%s.tls.%08x", name, hash);
    return out;
}

// the memory order argument `order` of an access, as gcc's
// get_memmodel and its checks per builtin have it.
int atomic_order(struct compile_process* process, struct node* order, int access, bool* constant){
    struct constant c;
    *constant = constant_fold(process, order, &c) && constant_is_integer(&c);
    if(!*constant || c.value > ATOMIC_ORDER_SEQ_CST){
        return ATOMIC_ORDER_SEQ_CST;
    }
    int o = (int)c.value;
    if(o == ATOMIC_ORDER_CONSUME){
        o = ATOMIC_ORDER_ACQUIRE;
    }
    if(access == ATOMIC_ACCESS_LOAD && (o == ATOMIC_ORDER_RELEASE || o == ATOMIC_ORDER_ACQ_REL)){
        return ATOMIC_ORDER_SEQ_CST;
    }
    if(access == ATOMIC_ACCESS_STORE && (o == ATOMIC_ORDER_ACQUIRE || o == ATOMIC_ORDER_ACQ_REL)){
        return ATOMIC_ORDER_SEQ_CST;
    }
    return o;
}

bool atomic_order_acquires(int order){
    return order == ATOMIC_ORDER_CONSUME || order == ATOMIC_ORDER_ACQUIRE || order == ATOMIC_ORDER_ACQ_REL
        || order == ATOMIC_ORDER_SEQ_CST;
}

bool atomic_order_releases(int order){
    return order == ATOMIC_ORDER_RELEASE || order == ATOMIC_ORDER_ACQ_REL || order == ATOMIC_ORDER_SEQ_CST;
}

// A failure order that releases is seq_cst for both, and so is a
// success order weaker than the failure one (gcc's checks). The
// instructions then acquire when either order does and release when
// the success one does.
int atomic_order_cas(int success, int failure){
    if(failure == ATOMIC_ORDER_RELEASE || failure == ATOMIC_ORDER_ACQ_REL || failure > success
       || success == ATOMIC_ORDER_SEQ_CST || failure == ATOMIC_ORDER_SEQ_CST){
        return ATOMIC_ORDER_SEQ_CST;
    }
    bool acquire = atomic_order_acquires(success) || atomic_order_acquires(failure);
    bool release = atomic_order_releases(success);
    return acquire && release ? ATOMIC_ORDER_ACQ_REL : acquire ? ATOMIC_ORDER_ACQUIRE
         : release ? ATOMIC_ORDER_RELEASE : ATOMIC_ORDER_RELAXED;
}

bool atomic_size_is_lock_free(long long size){
    return size == 1 || size == 2 || size == 4 || size == 8;
}

// Whether an object of type t is _Atomic: the type is, at its own
// pointer depth. `_Atomic int *p` makes *p atomic and not p.
bool datatype_is_atomic(struct datatype* t){
    return t->atomic_level && !(t->flags & DATATYPE_FLAG_IS_ARRAY) && t->pointer_depth == t->atomic_level - 1;
}
