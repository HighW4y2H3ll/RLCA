
#ifndef _RBRK
#define _RBRK

#define RBRK_NORMAL 0
#define RBRK_INIT   -1
#define RBRK_MOVE   -2
#define RBRK_EXPAND -3

#define RBRK_MAP_FAILED -1

//#define RBRK_DEFAULT_SIZE   (0x1000*64)
#define RBRK_DEFAULT_SIZE   (0x1000*64)
#define RBRK_UPPERBOUND     (0x1000*64*4*1024*3)
//#define RBRK_UPPERBOUND     (-1)
#define RBRK_MINRANDOMNESS  16

struct Brk {
    void *base;
    void *end;
    size_t sz;
};

int rbrk( pid_t pid, void *end, void **addr, size_t *sz );
void rbrkexit();
void getBrkInfo( struct Brk *info );
void updateBrkInfo( struct Brk *info );

#endif

