// _Atomic objects. A load or store is atomic, and ++, -- and
// every compound assignment is one atomic read-modify-write (C11
// 6.5.2.4, 6.5.16.2), in the usual arithmetic conversions.
#include <stdatomic.h>

static unsigned h;

static void mix(long v){
    h = h * 31 + (unsigned)v;
}

struct counters {
    char tag;
    _Atomic int hits;
    _Atomic(unsigned short) small;
};

static _Atomic int g = 10;
static atomic_long gl;
static struct counters s = { 'x', 3, 65535 };
static _Atomic int arr[3] = { 1, 2, 3 };

static int bump(_Atomic int* p){
    return ++*p;
}

int main(void){
    _Atomic int x = 5;
    mix(x++);
    mix(++x);
    mix(x--);
    mix(--x);
    mix(x += 10);
    mix(x -= 3);
    mix(x *= 7);
    mix(x /= 4);
    mix(x %= 5);
    mix(x <<= 3);
    mix(x >>= 1);
    mix(x |= 0x30);
    mix(x &= 0x2c);
    mix(x ^= 0xff);
    mix(x);

    // Narrow objects wrap at their own width; the arithmetic is done in
    // int, so a division sees the promoted operands.
    _Atomic char c = 100;
    c += 100;
    mix(c);
    _Atomic unsigned char uc = 200;
    uc /= -1;
    mix(uc);
    uc = 255;
    mix(++uc);
    _Atomic(short) sh = 32767;
    sh++;
    mix(sh);
    _Atomic _Bool b = 0;
    b += 2;
    mix(b);
    b = 0;
    b++;
    mix(b);

    g += 5;
    mix(g);
    gl = 1;
    gl <<= 40 % (8 * sizeof(long) - 1);
    mix(gl != 0);
    gl--;
    mix(gl & 0xffff);

    mix(s.hits++);
    s.hits *= 3;
    mix(s.hits);
    s.small += 2;
    mix(s.small);
    mix(s.tag);

    arr[1] += 40;
    mix(arr[1]);
    mix(bump(&arr[2]));
    mix(bump(&g));

    // Atomic pointers step by elements.
    static int data[8];
    int* _Atomic p = data;
    p++;
    p += 3;
    mix(p - data);
    _Atomic(int*) q = data + 7;
    q -= 2;
    mix(q - data);
    mix(*q == 0);

    // <stdatomic.h>.
    atomic_int ai = ATOMIC_VAR_INIT(1);
    atomic_init(&ai, 2);
    mix(atomic_load(&ai));
    atomic_store(&ai, 7);
    mix(atomic_fetch_add(&ai, 3));
    mix(atomic_fetch_sub_explicit(&ai, 1, memory_order_relaxed));
    mix(atomic_exchange(&ai, 100));
    int want = 100;
    mix(atomic_compare_exchange_strong(&ai, &want, 200));
    mix(atomic_compare_exchange_weak(&ai, &want, 300));
    mix(want);
    mix(atomic_fetch_or(&ai, 1));
    mix(atomic_fetch_and(&ai, 0xf0));
    mix(atomic_fetch_xor(&ai, 0x0f));
    mix(atomic_load_explicit(&ai, memory_order_acquire));
    atomic_flag flag = ATOMIC_FLAG_INIT;
    mix(atomic_flag_test_and_set(&flag));
    mix(atomic_flag_test_and_set(&flag));
    atomic_flag_clear(&flag);
    mix(atomic_flag_test_and_set_explicit(&flag, memory_order_acquire));
    atomic_thread_fence(memory_order_seq_cst);
    atomic_signal_fence(memory_order_seq_cst);
    mix(atomic_is_lock_free(&ai));
    mix(ATOMIC_INT_LOCK_FREE);
    atomic_uint au = 0;
    au--;
    mix(au == 4294967295u);
    return h % 127;
}
