#include <stdio.h>

#define OK  1
#define FAIL    0

#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)

#define ERROR(fmt, ...)  \
    {   \
        printf(fmt, ##__VA_ARGS__);    \
        printf("\n");    \
        exit(-1);   \
    }
#define EXIT ERROR
