#ifndef THREADS_H
#define THREADS_H
#include <pthread.h>

// C11 7.26 threads from glibc's libc: thrd_t is
// a pthread_t, mtx_t and cnd_t have the sizes of glibc's mutex and
// condition variable.

#define thread_local _Thread_local
#define ONCE_FLAG_INIT { 0 }
#define TSS_DTOR_ITERATIONS 4

typedef unsigned long thrd_t;
typedef unsigned int tss_t;
typedef int (*thrd_start_t)(void*);
typedef void (*tss_dtor_t)(void*);
typedef pthread_mutex_t mtx_t;
typedef pthread_cond_t cnd_t;
typedef struct { int __sam_data; } once_flag;

enum { thrd_success = 0, thrd_busy = 1, thrd_error = 2, thrd_nomem = 3, thrd_timedout = 4 };
enum { mtx_plain = 0, mtx_recursive = 1, mtx_timed = 2 };

int thrd_create(thrd_t* thread, thrd_start_t start, void* arg);
int thrd_join(thrd_t thread, int* result);
int thrd_detach(thrd_t thread);
thrd_t thrd_current(void);
int thrd_equal(thrd_t a, thrd_t b);
void thrd_exit(int result);
void thrd_yield(void);

int mtx_init(mtx_t* mutex, int type);
int mtx_lock(mtx_t* mutex);
int mtx_trylock(mtx_t* mutex);
int mtx_unlock(mtx_t* mutex);
void mtx_destroy(mtx_t* mutex);

int cnd_init(cnd_t* cond);
int cnd_signal(cnd_t* cond);
int cnd_broadcast(cnd_t* cond);
int cnd_wait(cnd_t* cond, mtx_t* mutex);
void cnd_destroy(cnd_t* cond);

void call_once(once_flag* flag, void (*func)(void));
int tss_create(tss_t* key, tss_dtor_t destructor);
void* tss_get(tss_t key);
int tss_set(tss_t key, void* value);
void tss_delete(tss_t key);
#endif
