
#include "base.h"
#include "error.h"

#include "mmap.h"
#include "memtrace.h"

#include "brk.h"

static struct Brk _brk_cache;
struct Brk *brk_cache = 0;




void rbrkexit()
{
    destroyMemTrace();
}

static void onInit( pid_t pid, int flag )
{
    initMemTrace(flag);
}

size_t brkTryFindMap( size_t size )
{
    size_t idx = searchRangeBySz(size);

    if ( idx >= size_list->count )
        return RBRK_MAP_FAILED;

    // Randomly Get One
    idx += randNumMod(size_list->count - idx);
    size_t sz = randNumMod(MIN(RBRK_MINRANDOMNESS,
            (size_list->ent[idx].sz - size) >> PAGESHIFT));

    size_t ref = size_list->ent[idx].idx;
    size_t start = (sz << PAGESHIFT) + addr_range->addrs[ref].start;

    splitRangeEntry(ref, start, start + size);

    return start;
}

static void initBrk()
{
    size_t start = brkTryFindMap(RBRK_DEFAULT_SIZE);

    if ( start == RBRK_MAP_FAILED )
        ERROR("BRK Region INIT Failed.");

    brk_cache = &_brk_cache;
    brk_cache->base = brk_cache->end = start;
    brk_cache->sz = RBRK_DEFAULT_SIZE;
}

bool brkTryMap( pid_t pid, size_t addr, size_t size )
{
    return (rmmap(pid, addr, size, MAP_FIXED, 0) != DIRECT_PASS);
}

int rbrk( pid_t pid, void *end, void **addr, size_t *sz )
{
    if ( !page_slot || !addr_range || !isMemTraceInit() )
        onInit(pid, INIT_INIT);

    if ( !brk_cache )
    {
        if ( end )
            ERROR("Attach Mode Not Supported.");

        initBrk();

        // Intentionally Invoke Mmap Syscall to Pre-MMAP a BRK memory
        *addr = brk_cache->base;
        *sz = RBRK_DEFAULT_SIZE;
        return RBRK_INIT;
    }

    if ( !end )
    {
        *addr = brk_cache->end;
        return RBRK_NORMAL;
    }

    // Page-Align
    end = ((size_t)end + PAGEMASK) & (~PAGEMASK);

    if ( brk_cache->base + RBRK_UPPERBOUND >= end )
    {
        *addr = brk_cache->end;
        return RBRK_NORMAL;
    }

    if ( brk_cache->base >= end )
    {
        *addr = brk_cache->end;
        return RBRK_NORMAL;
    }

    if ( brk_cache->base + brk_cache->sz >= end )
    {
        *addr = end;
        if ( end < brk_cache->end )
        {
            // Clear The Shrinked Region
            memset(end, 0, brk_cache->end - end);
        }

        brk_cache->end = end;
        return RBRK_NORMAL;
    }

    // if ( brk_cache->base + brk_cache->sz < end )
    // First Try Direct Expand
    size_t start = brk_cache->base + brk_cache->sz;
    size_t size = brk_cache->sz;
    while ( size < end - brk_cache->base )
    {
        size <<= 1;
    }

    ///*
    if ( brkTryMap(pid, start, size - brk_cache->sz) )
    {
        *addr = start;
        *sz = size - brk_cache->sz;
        brk_cache->sz = size;
        brk_cache->end = end;
        return RBRK_EXPAND;
    }
    //*/

    // Now Try Alloc at Another Place
    start = brkTryFindMap(size);
    if ( start != RBRK_MAP_FAILED )
    {
        *addr = start;
        *sz = size;

        // Leave This Later After Ref-Fix Stage
        /*
        brk_cache->end = start + end - brk_cache->base;
        brk_cache->base = start;
        brk_cache->sz = size;
        */
        return RBRK_MOVE;
    }

    // If Alloc Failed
    *addr = brk_cache->end;
    return RBRK_NORMAL;
}

void getBrkInfo( struct Brk *info )
{
    info->base = brk_cache->base;
    info->end = brk_cache->end;
    info->sz = brk_cache->sz;
}

void updateBrkInfo( struct Brk *info )
{
    brk_cache->base = info->base;
    brk_cache->end = info->end;
    brk_cache->sz = info->sz;
}

