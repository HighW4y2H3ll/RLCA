
#ifndef _RMMAP
#define _RMMAP

#include <stdlib.h>
#include <sys/mman.h>
#include "base.h"

#ifndef MREMAP_MAYMOVE
#define MREMAP_MAYMOVE  1
#endif

#ifndef MREMAP_FIXED
#define MREMAP_FIXED  2
#endif

void *rmmap( pid_t, void *, size_t, int, int );
void rmunmap( pid_t, void *, size_t );
void *rmremap( pid_t, void *, size_t, size_t, int, void *);
void rmexit();

#endif
