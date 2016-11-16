
#include "utils.h"

extern struct AddrRange *addr_range;
extern struct ARSizeList *size_list;
extern struct Page *page_slot;


// Quick Sort Utils
static int cmpSlot( const void *lexpr, const void *rexpr )
{
    size_t lval = *(size_t *)lexpr;
    size_t rval = *(size_t *)rexpr;
    return (lval > rval) ? 1 : (lval < rval) ? -1 : 0;
}

void sortSlot()
{
    qsort(page_slot->addrs, page_slot->count, sizeof(size_t), cmpSlot);
}

static int cmpRangeAddr( const void *lexpr, const void *rexpr )
{
    struct AddrRangeEntry *lval = lexpr;
    struct AddrRangeEntry *rval = rexpr;
    return (lval->start > rval->start) ? 1 : (lval->start < rval->start) ? -1 : 0;
}

void sortRangeByAddr()
{
    qsort(addr_range->addrs, addr_range->count, sizeof(struct AddrRangeEntry), cmpRangeAddr);
}

static int cmpRangeSz( const void *lexpr, const void *rexpr )
{
    struct AddrRangeEntry *lval = lexpr;
    struct AddrRangeEntry *rval = rexpr;
    size_t lgap = lval->end - lval->start;
    size_t rgap = rval->end - rval->start;
    return (lgap > rgap) ? 1 : (lgap < rgap) ? -1 : 0;
}

void sortRangeBySz()
{
    qsort(addr_range->addrs, addr_range->count, sizeof(struct AddrRangeEntry), cmpRangeSz);
}

// Binary Searcy Utils
// Find the index to insert a new entry
// If Equal, return the 1st equal entry index
size_t searchRangeByAddr( size_t start )
{
    if ( addr_range->count == 0 )
        return 0;

    size_t left = 0, right = addr_range->count - 1;
    size_t index = right/2;

    if ( addr_range->addrs[left].start >= start )
        return left;
    if ( addr_range->addrs[right].start < start )
        return right + 1;

    while(1)
    {
        if ( left + 1 == right )
            return right;

        size_t addr = addr_range->addrs[index].start;
        if ( addr < start )
        {
            left = index;
        } else {
            right = index;
        }

        index = (left + right)/2;
    }
}

// Find the Index to insert in Size_List
// If Equal, return the 1st equal entry index
size_t searchRangeBySz( size_t size )
{
    if ( size_list->count == 0 )
        return 0;

    size_t left = 0, right = size_list->count - 1;
    size_t index = right/2;

    if ( size_list->ent[left].sz >= size )
        return left;
    if ( size_list->ent[right].sz < size )
        return right + 1;

    while(1)
    {
        if ( left + 1 == right )
            return right;

        if ( size_list->ent[index].sz < size )
        {
            left = index;
        } else {
            right = index;
        }

        index = (left + right)/2;
    }
}


