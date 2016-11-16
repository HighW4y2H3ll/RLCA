
#ifndef _RWRAPPER
#define _RWRAPPER


// TODO: Try dr_raw_mem_alloc()?
// DynamoRIO private alloc support
#include "dr_api.h"
#define DRMALLOC(X)      dr_nonheap_alloc((X),  DR_MEMPROT_READ|DR_MEMPROT_WRITE)
#define DRFREE(X,Y)        {    \
    dr_nonheap_free((X), (Y));  \
    (X) = 0;    \
}

#endif

