// gcc's __atomic builtins on 1, 2, 4 and 8 byte objects
// (long is 4 bytes on i386), signed and unsigned, and pointers.
static unsigned h;

static void mix(long v){
    h = h * 31 + (unsigned)v;
}

static signed char sc = -100;
static unsigned short us = 65530;
static int si = -7;
static unsigned long ul = 5;

int main(void){
    mix(__atomic_load_n(&sc, __ATOMIC_SEQ_CST));
    mix(__atomic_fetch_add(&sc, 100, __ATOMIC_RELAXED));
    mix(__atomic_add_fetch(&sc, 50, __ATOMIC_ACQ_REL));
    mix(sc);
    mix(__atomic_fetch_sub(&sc, 200, __ATOMIC_SEQ_CST));
    mix(sc);

    mix(__atomic_fetch_add(&us, 10, __ATOMIC_SEQ_CST));
    mix(us);
    mix(__atomic_sub_fetch(&us, 1, __ATOMIC_SEQ_CST));
    mix(__atomic_fetch_or(&us, 0x8000, __ATOMIC_SEQ_CST));
    mix(__atomic_and_fetch(&us, 0xff0f, __ATOMIC_SEQ_CST));
    mix(__atomic_fetch_xor(&us, 0xffff, __ATOMIC_SEQ_CST));
    mix(__atomic_nand_fetch(&us, 0x00ff, __ATOMIC_SEQ_CST));
    mix(us);

    mix(__atomic_exchange_n(&si, 42, __ATOMIC_SEQ_CST));
    mix(si);
    __atomic_store_n(&si, -1, __ATOMIC_RELEASE);
    mix(__atomic_load_n(&si, __ATOMIC_ACQUIRE));
    mix(__atomic_fetch_nand(&si, 6, __ATOMIC_SEQ_CST));
    mix(si);
    mix(__atomic_xor_fetch(&si, 3, __ATOMIC_SEQ_CST));
    mix(__atomic_or_fetch(&si, 64, __ATOMIC_SEQ_CST));
    mix(__atomic_fetch_and(&si, 0x41, __ATOMIC_SEQ_CST));
    mix(si);

    int expected = 0;
    mix(__atomic_compare_exchange_n(&si, &expected, 9, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST));
    mix(expected);
    mix(si);
    expected = si;
    mix(__atomic_compare_exchange_n(&si, &expected, 9, 1, __ATOMIC_SEQ_CST, __ATOMIC_RELAXED));
    mix(expected);
    mix(si);

    signed char want = -100;
    signed char c = -100;
    mix(__atomic_compare_exchange_n(&c, &want, -3, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST));
    mix(c);
    want = 5;
    mix(__atomic_compare_exchange_n(&c, &want, 7, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST));
    mix(want);
    mix(c);

    unsigned short uwant = 65535;
    unsigned short u = 65535;
    mix(__atomic_compare_exchange_n(&u, &uwant, 1, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST));
    mix(u);

    mix(__atomic_fetch_add(&ul, 1000, __ATOMIC_SEQ_CST));
    mix(__atomic_sub_fetch(&ul, 2000, __ATOMIC_SEQ_CST) > 1000);
    mix(ul);

    // The generic forms pass values by address.
    int a = 3, r = 0, v = 8;
    __atomic_load(&a, &r, __ATOMIC_SEQ_CST);
    mix(r);
    __atomic_store(&a, &v, __ATOMIC_SEQ_CST);
    mix(a);
    v = 11;
    __atomic_exchange(&a, &v, &r, __ATOMIC_SEQ_CST);
    mix(r);
    mix(a);
    int e = 11, d = 12;
    mix(__atomic_compare_exchange(&a, &e, &d, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST));
    mix(a);

    // A pointer object; the builtins do not scale the operand.
    static int arr[4];
    int* p = arr;
    int* old = __atomic_exchange_n(&p, arr + 2, __ATOMIC_SEQ_CST);
    mix(old - arr);
    mix(p - arr);
    __atomic_fetch_add(&p, sizeof(int), __ATOMIC_SEQ_CST);
    mix(p - arr);

    // test_and_set and clear work on a byte.
    unsigned char flag = 0;
    mix(__atomic_test_and_set(&flag, __ATOMIC_SEQ_CST));
    mix(__atomic_test_and_set(&flag, __ATOMIC_SEQ_CST));
    mix(flag != 0);
    __atomic_clear(&flag, __ATOMIC_SEQ_CST);
    mix(flag);

    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    __atomic_signal_fence(__ATOMIC_SEQ_CST);
    mix(__atomic_always_lock_free(sizeof(int), 0));
    mix(__atomic_is_lock_free(sizeof(long), &ul));
    // Used inside larger expressions.
    int n = 5;
    mix(__atomic_add_fetch(&n, 1, __ATOMIC_SEQ_CST) * 2 + __atomic_load_n(&n, __ATOMIC_SEQ_CST));
    return h % 127;
}
