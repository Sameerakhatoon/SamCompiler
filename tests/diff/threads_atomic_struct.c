// several threads update shared _Atomic structs and a long
// double with compare-exchange loops: a pair of 8 bytes (lock-free), a
// quad of 16 and a triple of 12 bytes (libatomic), and an _Atomic long
// double through +=. Each update keeps an invariant between the fields
// that a torn read or a lost update would break. A message
// passed with release and acquire, counters with relaxed adds. Each
// failed check sets a bit of the status.
#include <pthread.h>
#include <stdatomic.h>

#define THREADS 4
#define ROUNDS  3000

struct pair { int count, twice; };
struct quad { int count, twice, thrice, neg; };
struct triple { int count, twice, thrice; };

static _Atomic struct pair pair;
static _Atomic struct quad quad;
static _Atomic struct triple triple;
static _Atomic long double total;
static atomic_int relaxed;
static int broken;

static int message;
static atomic_int ready;
static int received;

static void* worker(void* arg){
    (void)arg;
    for(int i = 0; i < ROUNDS; i++){
        struct pair p = atomic_load(&pair);
        struct pair np;
        do {
            if(p.twice != 2 * p.count){
                broken = 1;
            }
            np.count = p.count + 1;
            np.twice = p.twice + 2;
        } while(!atomic_compare_exchange_weak(&pair, &p, np));
        struct quad q = atomic_load_explicit(&quad, memory_order_acquire);
        struct quad nq;
        do {
            if(q.twice != 2 * q.count || q.thrice != 3 * q.count || q.neg != -q.count){
                broken = 1;
            }
            nq.count = q.count + 1;
            nq.twice = q.twice + 2;
            nq.thrice = q.thrice + 3;
            nq.neg = q.neg - 1;
        } while(!atomic_compare_exchange_strong_explicit(&quad, &q, nq, memory_order_acq_rel, memory_order_acquire));
        struct triple t;
        __atomic_load(&triple, &t, __ATOMIC_ACQUIRE);
        struct triple nt;
        do {
            nt.count = t.count + 1;
            nt.twice = t.twice + 2;
            nt.thrice = t.thrice + 3;
        } while(!__atomic_compare_exchange(&triple, &t, &nt, 0, __ATOMIC_SEQ_CST, __ATOMIC_RELAXED));
        total += 1;
        atomic_fetch_add_explicit(&relaxed, 1, memory_order_relaxed);
    }
    return 0;
}

static void* consumer(void* arg){
    (void)arg;
    while(!atomic_load_explicit(&ready, memory_order_acquire)){
    }
    received = message;
    return 0;
}

int main(void){
    pthread_t threads[THREADS], reader;
    if(pthread_create(&reader, 0, consumer, 0) != 0){
        return 100;
    }
    for(long i = 0; i < THREADS; i++){
        if(pthread_create(&threads[i], 0, worker, (void*)i) != 0){
            return 100;
        }
    }
    message = 42;
    atomic_store_explicit(&ready, 1, memory_order_release);
    for(int i = 0; i < THREADS; i++){
        pthread_join(threads[i], 0);
    }
    pthread_join(reader, 0);
    atomic_thread_fence(memory_order_seq_cst);
    int status = 0;
    struct pair p = pair;
    struct quad q = quad;
    struct triple t = triple;
    int n = THREADS * ROUNDS;
    if(p.count != n || p.twice != 2 * n){
        status |= 1;
    }
    if(q.count != n || q.twice != 2 * n || q.thrice != 3 * n || q.neg != -n){
        status |= 2;
    }
    if(t.count != n || t.twice != 2 * n || t.thrice != 3 * n){
        status |= 4;
    }
    if(total != n){
        status |= 8;
    }
    if(atomic_load_explicit(&relaxed, memory_order_relaxed) != n){
        status |= 16;
    }
    if(received != 42){
        status |= 32;
    }
    if(broken){
        status |= 64;
    }
    return status;
}
