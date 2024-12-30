// Several threads update shared atomics, each its
// own _Thread_local counter, and a counter under a mutex. A lost update
// shows as a wrong total; each failed check sets a bit of the status.
#include <pthread.h>
#include <stdatomic.h>

#define THREADS 4
#define ROUNDS  20000

static _Atomic int hits;
static atomic_long added;
static _Atomic unsigned char bytes;
static _Atomic short halves;
static int synced;
static int cas_total;
static int locked;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static _Thread_local int mine;
static _Thread_local long own_sum = 1000;

static void* worker(void* arg){
    long id = (long)arg;
    for(int i = 0; i < ROUNDS; i++){
        hits++;
        atomic_fetch_add(&added, 3);
        bytes += 1;
        halves -= 2;
        __sync_fetch_and_add(&synced, 1);
        int seen = __atomic_load_n(&cas_total, __ATOMIC_SEQ_CST);
        while(!__atomic_compare_exchange_n(&cas_total, &seen, seen + 2, 1, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)){
        }
        pthread_mutex_lock(&lock);
        locked++;
        pthread_mutex_unlock(&lock);
        mine++;
        own_sum += id;
    }
    // Each thread saw only its own thread-locals.
    if(mine != ROUNDS || own_sum != 1000 + id * ROUNDS){
        return (void*)1;
    }
    return (void*)0;
}

int main(void){
    pthread_t threads[THREADS];
    for(long i = 0; i < THREADS; i++){
        if(pthread_create(&threads[i], 0, worker, (void*)i) != 0){
            return 100;
        }
    }
    int status = 0;
    for(int i = 0; i < THREADS; i++){
        void* result = 0;
        pthread_join(threads[i], &result);
        if(result){
            status |= 1;
        }
    }
    if(hits != THREADS * ROUNDS)                 status |= 2;
    if(added != 3L * THREADS * ROUNDS)           status |= 4;
    if(bytes != (unsigned char)(THREADS * ROUNDS)) status |= 8;
    if(halves != (short)(-2 * THREADS * ROUNDS)) status |= 16;
    if(synced != THREADS * ROUNDS)               status |= 32;
    if(cas_total != 2 * THREADS * ROUNDS)        status |= 64;
    if(locked != THREADS * ROUNDS)               status |= 64;
    // main's own thread-locals are untouched.
    if(mine != 0 || own_sum != 1000)             status |= 3;
    return status;
}
