#ifndef SELFHOST_SYS_STAT_H
#define SELFHOST_SYS_STAT_H
typedef unsigned int mode_t;
mode_t umask(mode_t mask);
int    fchmod(int fd, mode_t mode);
#endif
