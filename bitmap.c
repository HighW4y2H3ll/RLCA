
#include "bitmap.h"
#include "wrapper.h"
#include "error.h"

// sz Must Be 8*sizeof(size_t) Alligned
struct _BitMap *initBitMap( size_t sz )
{
    struct _BitMap *res = DRMALLOC(sizeof(struct _BitMap) + sizeof(size_t)*((sz/8/sizeof(size_t)) - 1));
    if ( !res )
        ERROR("BitMap Init Failed.");

    res->total = res->count = sz;
    memset(res->map, 0, sz/8);

    return res;
}

void expandBitMap( struct _BitMap *map )
{
}

void destroyBitMap( struct _BitMap *map )
{
    DRFREE(map, sizeof(struct _BitMap) + sizeof(size_t)*((map->total/8/sizeof(size_t)) - 1));
}

void setBitMap( struct _BitMap *map, size_t idx )
{
    size_t base = 8*sizeof(size_t);
    size_t i = idx / base;
    size_t off = idx % base;
    size_t *rec = &map->map[i];
    size_t mask = (size_t)(1 << off);

    *rec |= mask;
}

void clrBitMap( struct _BitMap *map, size_t idx )
{
    size_t base = 8*sizeof(size_t);
    size_t i = idx / base;
    size_t off = idx % base;
    size_t *rec = &map->map[i];
    size_t mask = (~((size_t)(1 << off)));

    *rec &= mask;
}

size_t getFirstFreeIdx( struct _BitMap *map )
{
    size_t i = 0, idx = 0;
    for (; i < map->total/8/sizeof(size_t); i++)
    {
        if ( map->map[i] != (~((size_t)(0))) )
        {
            idx = i * 8 * sizeof(size_t);
            size_t rec = map->map[i];
            size_t cnt = sizeof(size_t)*8;
            while ( (rec&1) && cnt-- )
            {
                idx++;
                rec >>= 1;
            }

            return idx;
        }
    }

    return (~((size_t)(0)));
}

