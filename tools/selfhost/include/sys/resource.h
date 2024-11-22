#ifndef SELFHOST_SYS_RESOURCE_H
#define SELFHOST_SYS_RESOURCE_H
struct rlimit {
    unsigned long rlim_cur;
    unsigned long rlim_max;
};
#define RLIMIT_STACK 3
#define RLIM_INFINITY ((unsigned long)-1)
int getrlimit(int resource, struct rlimit* limit);
#endif
