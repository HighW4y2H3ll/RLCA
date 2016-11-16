
#ifndef _RBITMAP
#define _RBITMAP

#include <stdlib.h>


struct _BitMap {
    size_t total;
    size_t count;
    size_t map[1];  // SZ/8/sizeof(size_t)
};


struct _BitMap *initBitMap(size_t sz);
void expandBitMap( struct _BitMap *map );

void destroyBitMap( struct _BitMap *map );

void setBitMap(struct _BitMap *map, size_t idx);
void clrBitMap(struct _BitMap *map, size_t idx);

size_t getFirstFreeIdx(struct _BitMap *map);

#endif

