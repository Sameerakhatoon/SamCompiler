#ifndef SELFHOST_UNISTD_H
#define SELFHOST_UNISTD_H
#include <stddef.h>
#include <sys/types.h>
typedef long ssize_t;
#define R_OK 4
#define W_OK 2
#define X_OK 1
#define F_OK 0
pid_t   fork(void);
pid_t   getpid(void);
int     access(const char* path, int mode);
int     rmdir(const char* path);
int     unlink(const char* path);
int     close(int fd);
ssize_t readlink(const char* path, char* out, size_t size);
#endif
