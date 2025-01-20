#ifndef SELFHOST_SYS_WAIT_H
#define SELFHOST_SYS_WAIT_H
#include <sys/types.h>
pid_t waitpid(pid_t pid, int* status, int options);
#define WEXITSTATUS(status) (((status) & 0xff00) >> 8)
#define WIFEXITED(status)   (((status) & 0x7f) == 0)
#endif
