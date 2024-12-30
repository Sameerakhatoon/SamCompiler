// C11 <threads.h> on glibc: thrd_create and thrd_join, a
// mtx_t, call_once, thread_local, and an atomic_flag spin lock.
#include <threads.h>
#include <stdatomic.h>

#define THREADS 3
#define ROUNDS  10000

static mtx_t mutex;
static int guarded;
static int spun;
static atomic_flag spin = ATOMIC_FLAG_INIT;
static atomic_int once_calls;
static once_flag once = ONCE_FLAG_INIT;
static thread_local int local_count;

static void setup(void){
    once_calls++;
}

static int run(void* arg){
    int step = *(int*)arg;
    call_once(&once, setup);
    for(int i = 0; i < ROUNDS; i++){
        mtx_lock(&mutex);
        guarded += step;
        mtx_unlock(&mutex);
        while(atomic_flag_test_and_set(&spin)){
        }
        spun++;
        atomic_flag_clear(&spin);
        local_count++;
    }
    return local_count == ROUNDS ? step : -1;
}

int main(void){
    if(mtx_init(&mutex, mtx_plain) != thrd_success){
        return 100;
    }
    thrd_t threads[THREADS];
    int steps[THREADS];
    for(int i = 0; i < THREADS; i++){
        steps[i] = i + 1;
        if(thrd_create(&threads[i], run, &steps[i]) != thrd_success){
            return 101;
        }
    }
    int status = 0;
    int results = 0;
    for(int i = 0; i < THREADS; i++){
        int result = 0;
        thrd_join(threads[i], &result);
        results += result;
    }
    mtx_destroy(&mutex);
    if(results != 1 + 2 + 3)                status |= 1;
    if(guarded != (1 + 2 + 3) * ROUNDS)     status |= 2;
    if(spun != THREADS * ROUNDS)            status |= 4;
    if(once_calls != 1)                     status |= 8;
    if(local_count != 0)                    status |= 16;
    return status;
}
