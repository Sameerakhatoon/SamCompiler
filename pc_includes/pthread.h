#ifndef PTHREAD_H
#define PTHREAD_H
#include <stddef.h>

// POSIX threads from glibc, whose libc has them
// since 2.34 (no -lpthread needed). glibc's own header is beyond this
// compiler, so the types are opaque blocks of glibc's size and
// alignment on each target, and the functions are declared here. Two
// blocks instead of #else.

typedef unsigned long pthread_t;
typedef unsigned int pthread_key_t;
typedef int pthread_once_t;

// i386: attr 36 bytes, mutex 24, cond 48; x86_64 and riscv64: 56, 40,
// 48; aarch64: 64, 48, 48, and 8-byte attribute objects.
#ifndef __LP64__
typedef struct { long __sam_word[9]; } pthread_attr_t;
typedef struct { long __sam_word[6]; } pthread_mutex_t;
typedef struct { long __sam_word[12]; } pthread_cond_t;
#endif
#ifdef __LP64__
#ifndef __aarch64__
typedef struct { long __sam_word[7]; } pthread_attr_t;
typedef struct { long __sam_word[5]; } pthread_mutex_t;
#endif
#ifdef __aarch64__
typedef struct { long __sam_word[8]; } pthread_attr_t;
typedef struct { long __sam_word[6]; } pthread_mutex_t;
#endif
typedef struct { long __sam_word[6]; } pthread_cond_t;
#endif
#ifndef __aarch64__
typedef struct { int __sam_word; } pthread_mutexattr_t;
typedef struct { int __sam_word; } pthread_condattr_t;
#endif
#ifdef __aarch64__
typedef struct { long __sam_word; } pthread_mutexattr_t;
typedef struct { long __sam_word; } pthread_condattr_t;
#endif

// A default mutex and condition variable are all zero bytes in glibc.
#define PTHREAD_MUTEX_INITIALIZER { { 0 } }
#define PTHREAD_COND_INITIALIZER { { 0 } }
#define PTHREAD_ONCE_INIT 0

#define PTHREAD_MUTEX_NORMAL     0
#define PTHREAD_MUTEX_RECURSIVE  1
#define PTHREAD_MUTEX_ERRORCHECK 2
#define PTHREAD_MUTEX_DEFAULT    0
#define PTHREAD_CREATE_JOINABLE  0
#define PTHREAD_CREATE_DETACHED  1

int pthread_create(pthread_t* thread, const pthread_attr_t* attr, void* (*start)(void*), void* arg);
int pthread_join(pthread_t thread, void** result);
int pthread_detach(pthread_t thread);
void pthread_exit(void* result);
pthread_t pthread_self(void);
int pthread_equal(pthread_t a, pthread_t b);
int pthread_attr_init(pthread_attr_t* attr);
int pthread_attr_destroy(pthread_attr_t* attr);
int pthread_attr_setdetachstate(pthread_attr_t* attr, int state);
int pthread_attr_setstacksize(pthread_attr_t* attr, size_t size);

int pthread_mutex_init(pthread_mutex_t* mutex, const pthread_mutexattr_t* attr);
int pthread_mutex_destroy(pthread_mutex_t* mutex);
int pthread_mutex_lock(pthread_mutex_t* mutex);
int pthread_mutex_trylock(pthread_mutex_t* mutex);
int pthread_mutex_unlock(pthread_mutex_t* mutex);
int pthread_mutexattr_init(pthread_mutexattr_t* attr);
int pthread_mutexattr_destroy(pthread_mutexattr_t* attr);
int pthread_mutexattr_settype(pthread_mutexattr_t* attr, int type);

int pthread_cond_init(pthread_cond_t* cond, const pthread_condattr_t* attr);
int pthread_cond_destroy(pthread_cond_t* cond);
int pthread_cond_wait(pthread_cond_t* cond, pthread_mutex_t* mutex);
int pthread_cond_signal(pthread_cond_t* cond);
int pthread_cond_broadcast(pthread_cond_t* cond);

int pthread_once(pthread_once_t* once, void (*init)(void));
int pthread_key_create(pthread_key_t* key, void (*destructor)(void*));
int pthread_key_delete(pthread_key_t key);
void* pthread_getspecific(pthread_key_t key);
int pthread_setspecific(pthread_key_t key, const void* value);
#endif
