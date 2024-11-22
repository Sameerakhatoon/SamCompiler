#ifndef SELFHOST_UNISTD_H
#define SELFHOST_UNISTD_H
#include <stddef.h>
typedef long ssize_t;
int     unlink(const char* path);
int     close(int fd);
ssize_t readlink(const char* path, char* out, size_t size);
#endif
