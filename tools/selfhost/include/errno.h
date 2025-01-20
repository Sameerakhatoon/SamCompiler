#ifndef SELFHOST_ERRNO_H
#define SELFHOST_ERRNO_H
/* glibc's errno: a function giving the thread's errno address. */
int* __errno_location(void);
#define errno (*__errno_location())
#endif
