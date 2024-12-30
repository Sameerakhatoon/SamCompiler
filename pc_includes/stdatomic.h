#ifndef STDATOMIC_H
#define STDATOMIC_H

// C11 7.17 on gcc's __atomic builtins, as gcc's
// own stdatomic.h is. Every operation is sequentially consistent
// whatever memory order is given; the order is still evaluated.
// typedef is a token substitution here, so the types are written out.

typedef int memory_order;
#define memory_order_relaxed __ATOMIC_RELAXED
#define memory_order_consume __ATOMIC_CONSUME
#define memory_order_acquire __ATOMIC_ACQUIRE
#define memory_order_release __ATOMIC_RELEASE
#define memory_order_acq_rel __ATOMIC_ACQ_REL
#define memory_order_seq_cst __ATOMIC_SEQ_CST

// Every size this compiler has is lock-free.
#define ATOMIC_BOOL_LOCK_FREE     2
#define ATOMIC_CHAR_LOCK_FREE     2
#define ATOMIC_CHAR16_T_LOCK_FREE 2
#define ATOMIC_CHAR32_T_LOCK_FREE 2
#define ATOMIC_WCHAR_T_LOCK_FREE  2
#define ATOMIC_SHORT_LOCK_FREE    2
#define ATOMIC_INT_LOCK_FREE      2
#define ATOMIC_LONG_LOCK_FREE     2
#define ATOMIC_LLONG_LOCK_FREE    2
#define ATOMIC_POINTER_LOCK_FREE  2

typedef _Atomic _Bool              atomic_bool;
typedef _Atomic char               atomic_char;
typedef _Atomic signed char        atomic_schar;
typedef _Atomic unsigned char      atomic_uchar;
typedef _Atomic short              atomic_short;
typedef _Atomic unsigned short     atomic_ushort;
typedef _Atomic int                atomic_int;
typedef _Atomic unsigned int       atomic_uint;
typedef _Atomic long               atomic_long;
typedef _Atomic unsigned long      atomic_ulong;
typedef _Atomic long long          atomic_llong;
typedef _Atomic unsigned long long atomic_ullong;
typedef _Atomic unsigned short     atomic_char16_t;
typedef _Atomic unsigned int       atomic_char32_t;
typedef _Atomic signed char        atomic_int_least8_t;
typedef _Atomic unsigned char      atomic_uint_least8_t;
typedef _Atomic short              atomic_int_least16_t;
typedef _Atomic unsigned short     atomic_uint_least16_t;
typedef _Atomic int                atomic_int_least32_t;
typedef _Atomic unsigned int       atomic_uint_least32_t;
typedef _Atomic long               atomic_intptr_t;
typedef _Atomic unsigned long      atomic_uintptr_t;
typedef _Atomic unsigned long      atomic_size_t;
typedef _Atomic long               atomic_ptrdiff_t;
typedef _Atomic long               atomic_intmax_t;
typedef _Atomic unsigned long      atomic_uintmax_t;
// wchar_t: an int, unsigned on aarch64 (stddef.h). Two blocks, not
// #else.
#ifndef __aarch64__
typedef _Atomic int                atomic_wchar_t;
#endif
#ifdef __aarch64__
typedef _Atomic unsigned int       atomic_wchar_t;
#endif

// atomic_flag is the one byte __atomic_test_and_set and __atomic_clear
// work on.
typedef _Atomic _Bool atomic_flag;
#define ATOMIC_FLAG_INIT 0
#define ATOMIC_VAR_INIT(value) (value)
#define atomic_init(object, value) __atomic_store_n(object, value, __ATOMIC_RELAXED)
#define kill_dependency(y) (y)

#define atomic_thread_fence(order) __atomic_thread_fence(order)
#define atomic_signal_fence(order) __atomic_signal_fence(order)
#define atomic_is_lock_free(object) __atomic_is_lock_free(sizeof(*(object)), (object))

#define atomic_store(object, desired) __atomic_store_n(object, desired, __ATOMIC_SEQ_CST)
#define atomic_store_explicit(object, desired, order) __atomic_store_n(object, desired, order)
#define atomic_load(object) __atomic_load_n(object, __ATOMIC_SEQ_CST)
#define atomic_load_explicit(object, order) __atomic_load_n(object, order)
#define atomic_exchange(object, desired) __atomic_exchange_n(object, desired, __ATOMIC_SEQ_CST)
#define atomic_exchange_explicit(object, desired, order) __atomic_exchange_n(object, desired, order)
#define atomic_compare_exchange_strong(object, expected, desired) \
    __atomic_compare_exchange_n(object, expected, desired, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)
#define atomic_compare_exchange_strong_explicit(object, expected, desired, success, failure) \
    __atomic_compare_exchange_n(object, expected, desired, 0, success, failure)
#define atomic_compare_exchange_weak(object, expected, desired) \
    __atomic_compare_exchange_n(object, expected, desired, 1, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)
#define atomic_compare_exchange_weak_explicit(object, expected, desired, success, failure) \
    __atomic_compare_exchange_n(object, expected, desired, 1, success, failure)

#define atomic_fetch_add(object, operand) __atomic_fetch_add(object, operand, __ATOMIC_SEQ_CST)
#define atomic_fetch_add_explicit(object, operand, order) __atomic_fetch_add(object, operand, order)
#define atomic_fetch_sub(object, operand) __atomic_fetch_sub(object, operand, __ATOMIC_SEQ_CST)
#define atomic_fetch_sub_explicit(object, operand, order) __atomic_fetch_sub(object, operand, order)
#define atomic_fetch_or(object, operand) __atomic_fetch_or(object, operand, __ATOMIC_SEQ_CST)
#define atomic_fetch_or_explicit(object, operand, order) __atomic_fetch_or(object, operand, order)
#define atomic_fetch_xor(object, operand) __atomic_fetch_xor(object, operand, __ATOMIC_SEQ_CST)
#define atomic_fetch_xor_explicit(object, operand, order) __atomic_fetch_xor(object, operand, order)
#define atomic_fetch_and(object, operand) __atomic_fetch_and(object, operand, __ATOMIC_SEQ_CST)
#define atomic_fetch_and_explicit(object, operand, order) __atomic_fetch_and(object, operand, order)

#define atomic_flag_test_and_set(object) __atomic_test_and_set(object, __ATOMIC_SEQ_CST)
#define atomic_flag_test_and_set_explicit(object, order) __atomic_test_and_set(object, order)
#define atomic_flag_clear(object) __atomic_clear(object, __ATOMIC_SEQ_CST)
#define atomic_flag_clear_explicit(object, order) __atomic_clear(object, order)
#endif
