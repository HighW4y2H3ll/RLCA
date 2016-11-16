
#include "base.h"
#include "error.h"
#include "utils.h"
#include "wrapper.h"


size_t max_cache_pages = 128;
size_t max_cache_ranges = 128;

size_t mmap_min_addr = 0;

// Storage for ALL Single Pages
struct Page *page_slot = 0;

// No Single Page should Appear Here
struct AddrRange *addr_range = 0;

// Look-aside List for Addr_Range
struct ARSizeList *size_list = 0;

// Helper Flag for Drop Detection
int pagedrop = 0;
int rangedrop = 0;


// Init
// Lazy Expand the List to ensure minium overhead, with maximum accuricy
void initAddrRange( int flag )
{
    if ( size_list )
        DRFREE(size_list, sizeof(struct ARSizeList) + sizeof(struct ARSizeEntry)*(RMMAP_MAXCACHERANGES - 1));
    if ( addr_range )
        DRFREE(addr_range, sizeof(struct AddrRange) + sizeof(struct AddrRangeEntry)*(RMMAP_MAXCACHERANGES - 1));

    if ( flag == INIT_EXPAND && rangedrop )
        RMMAP_MAXCACHERANGES <<= 1;

    size_t sz = sizeof(struct AddrRange);
    sz += sizeof(struct AddrRangeEntry) * (RMMAP_MAXCACHERANGES - 1);

    rangedrop = 0;

    addr_range = DRMALLOC(sz);
    if ( !addr_range )
        ERROR("Addr_Range Init Failed");
    memset( addr_range, 0, sz );

    sz = sizeof(struct ARSizeList);
    sz += sizeof(struct ARSizeEntry) * (RMMAP_MAXCACHERANGES - 1);

    size_list = DRMALLOC(sz);
    if ( !size_list )
        ERROR("Size_List Init Failed");
    memset( size_list, 0, sz );
}

void initPageSlot( int flag )
{
    if ( page_slot )
        DRFREE(page_slot, sizeof(struct Page) + sizeof(size_t)*(RMMAP_MAXCACHEPAGES - 1));

    if ( flag == INIT_EXPAND && pagedrop )
        RMMAP_MAXCACHEPAGES <<= 1;

    size_t sz = sizeof(struct Page);
    sz += sizeof(size_t) * (RMMAP_MAXCACHERANGES - 1);

    pagedrop = 0;

    page_slot = DRMALLOC(sz);
    if ( !page_slot )
        ERROR("Page_Slot Init Failed");
    memset( page_slot, 0, sz ); 
}

// Destroy
void freeAddrRange()
{
    if ( size_list )
        DRFREE(size_list, sizeof(struct ARSizeList) + sizeof(struct ARSizeEntry)*(RMMAP_MAXCACHERANGES - 1));
    if ( addr_range )
        DRFREE(addr_range, sizeof(struct AddrRange) + sizeof(struct AddrRangeEntry)*(RMMAP_MAXCACHERANGES - 1));
    rangedrop = 0;
}

void freePageSlot()
{
    if ( page_slot )
        DRFREE(page_slot, sizeof(struct Page) + sizeof(size_t)*(RMMAP_MAXCACHEPAGES - 1));
    pagedrop = 0;
}


// Insert Size_List Entry, with size & Addr_Range Entry index
// Should be called after Addr_Range Insertion
void insertSLEntry( size_t sz, size_t index )
{
    // The Upper bound is ensured by Addr_Range
    size_t idx = searchRangeBySz(sz);
    size_t end = size_list->count++;
    for (; idx < end; end-- )
    {
        size_list->ent[end].sz = size_list->ent[end - 1].sz;
        size_t ref = size_list->ent[end - 1].idx;
        size_list->ent[end].idx = ref;

        // Fix Addr_Range Entry Ref
        addr_range->addrs[ref].ref = end;
    }

    size_list->ent[idx].idx = index;
    size_list->ent[idx].sz = sz;

    // Fix Addr_Range Ref
    addr_range->addrs[index].ref = idx;
}

// Remove Size_List Entry, given the Size_List Entry index
// Should be called before Addr_Range Removeal
void deleteSLEntry( size_t index )
{
    size_list->ent[index].idx = 0;
    size_list->ent[index].sz = 0;

    index++;

    for (; index < size_list->count; index++)
    {
        size_list->ent[index - 1].sz = size_list->ent[index].sz;
        size_t ref = size_list->ent[index].idx;
        size_list->ent[index - 1].idx = ref;

        // Fix Addr_Range Entry Ref
        addr_range->addrs[ref].ref = index - 1;
    }

    size_list->count--;
}

// Simply Move the List Backward
// Increase the addr_range->count Here If Necessary
// istart :     Index of the beginging entry
// iend   :     Index of the last free slot
void moveAddrRangeBackward( size_t istart, size_t iend )
{
    // We Assume addr_range->count will never exceed RMMAP_MAXCACHEPAGES
    if ( iend >= addr_range->count )
        iend = addr_range->count++;

    for (; istart < iend; iend-- )
    {
        addr_range->addrs[iend].start = addr_range->addrs[iend - 1].start;
        addr_range->addrs[iend].end = addr_range->addrs[iend - 1].end;
        size_t idx = addr_range->addrs[iend - 1].ref;
        addr_range->addrs[iend].ref = idx;

        // Fix Size_List
        size_list->ent[idx].idx = iend;
    }
}

// Simply Move the List Foreward
// Decrease the addr_range->count Here If Necessary
// istart   :    free slot to fill
// iend     :    Last entry TO MOVE
void moveAddrRangeForeward( size_t istart, size_t iend )
{
    if ( iend == 0 )   return;
    if ( iend >= addr_range->count - 1 )
        iend = --addr_range->count;

    for (; istart < iend; istart++ )
    {
        addr_range->addrs[istart].start = addr_range->addrs[istart + 1].start;
        addr_range->addrs[istart].end   = addr_range->addrs[istart + 1].end;
        
        size_t ref = addr_range->addrs[istart + 1].ref;
        addr_range->addrs[istart].ref = ref;

        // Fix Size_List
        size_list->ent[ref].idx = istart;
    }

    addr_range->addrs[iend].start = 0;
    addr_range->addrs[iend].end = 0;
    addr_range->addrs[iend].ref = 0;
}

// Simply Move Backward & Increase the page_slot->count
// index    :   1st entry to Move
void movePageSlotBackward( size_t index )
{
    size_t cnt = page_slot->count++;

    for (; index < cnt; cnt-- )
        page_slot->addrs[cnt] = page_slot->addrs[cnt - 1];
}

// Simply Move Foreward & Decrease the page_slot->count
// index    :   1st entry Right After the free slot
void movePageSlotForeward( size_t index )
{
    if ( index == 0 || index >= page_slot->count )   return;

    for (; index < page_slot->count; index++ )
        page_slot->addrs[index - 1] = page_slot->addrs[index];

    page_slot->count--;
}

// Range Entry at "index" is modified.
// Use this to fix the Size_List Order
void fixRangeSzSeq( size_t index )
{
    while ( index < size_list->count - 1 )
    {
        if ( size_list->ent[index].sz <= size_list->ent[index + 1].sz )
            return;

        // Swap index, index + 1
        size_t tmpsz = size_list->ent[index + 1].sz;
        size_list->ent[index + 1].sz = size_list->ent[index].sz;
        size_list->ent[index].sz = tmpsz;

        size_t ref1 = size_list->ent[index].idx;
        size_t ref2 = size_list->ent[index + 1].idx;
        addr_range->addrs[ref1].ref = index + 1;
        addr_range->addrs[ref2].ref = index;
        size_list->ent[index].idx = ref2;
        size_list->ent[index + 1].idx = ref1;

        index++;
    }
}

void forcePageInsert( size_t page )
{
    size_t idx = page_slot->count;
    if ( idx < RMMAP_MAXCACHEPAGES )
    {
        page_slot->addrs[idx] = page;
        page_slot->count++;
    } else {

        // Perform Random Page Drop
        size_t nidx = randNumMod(RMMAP_MAXCACHEPAGES);
        page_slot->addrs[nidx] = page;
        pagedrop = 1;
    }
}

void forceRangeInsert( size_t start, size_t end )
{
    size_t idx = searchRangeByAddr(start);

    if ( addr_range->count >= RMMAP_MAXCACHERANGES )
    {
        int issmallest = 0;
        size_t rsz, ins_addr;

        if ( size_list->ent[0].sz < end - start )
        {
            rsz = size_list->ent[0].sz >> PAGESHIFT;
            ins_addr = addr_range->addrs[size_list->ent[0].idx].start;

        } else {

            rsz = (end - start) >> PAGESHIFT;
            ins_addr = start;
            issmallest = 1;
        }

        // First Try Fit into Page_Slot
        if ( page_slot->count + rsz <= RMMAP_MAXCACHEPAGES )
        {
            while(rsz--)
            {
                page_slot->addrs[page_slot->count++] = ins_addr;
                ins_addr += PAGESIZE;
            }

            // In case new range is handled
            if ( issmallest )  return;

            // Size_List[0] is handled
            size_t ref = size_list->ent[0].idx;
            deleteSLEntry(0);
            if ( idx <= ref )
                moveAddrRangeBackward(idx, ref);
            else
            {
                idx--;
                moveAddrRangeForeward(ref, idx);
            }

            // Trade off for Addr_Range Optimization
            if ( idx >= addr_range->count )
                addr_range->count++;

            addr_range->addrs[idx].start = start;
            addr_range->addrs[idx].end = end;
            insertSLEntry(end - start, idx);
            return;
        }

        // Cannot Fit, Throw away the smallest range
        //
        rangedrop = 1;

        // Fill PageSlot if possible
        size_t remain = (RMMAP_MAXCACHEPAGES - page_slot->count) >> PAGESHIFT;
        while(remain--)
        {
            page_slot->addrs[page_slot->count++] = ins_addr;
            ins_addr += PAGESIZE;
        }
        
        // If new range is handled
        if ( issmallest )  return;

        // If Size_List[0] is handled
        size_t ref = size_list->ent[0].idx;
        deleteSLEntry(0);
        if ( idx <= ref )
            moveAddrRangeBackward(idx, ref);
        else
        {
            idx--;
            moveAddrRangeForeward(ref, idx);
        }

        // Trade off for Addr_Range Optimization
        if ( idx >= addr_range->count )
            addr_range->count++;

        addr_range->addrs[idx].start = start;
        addr_range->addrs[idx].end = end;
        insertSLEntry(end - start, idx);
        return;
    }

    // Normal Condition Branch
    moveAddrRangeBackward(idx, addr_range->count);
    addr_range->addrs[idx].start = start;
    addr_range->addrs[idx].end = end;
    insertSLEntry(end - start, idx);
}

// Try to Merge page_slot, but still keeps Min Randomness
// Merged pages are force inserted into Addr_Range
// Pages cannot be merged are kept in Page_Slot
int releaseSlot( size_t start )
{
    sortSlot();
    int inserted = 0;
    size_t idx = 0;
    struct AddrRangeEntry range;

    range.start = MIN(start, page_slot->addrs[0]);
    range.end = range.start + PAGESIZE;
    inserted = CONDMIN(start, page_slot->addrs[0], 1, 0);

    if (!inserted)
        deletePageEntry(0);

    while ( idx < page_slot->count )
    {
        if ( !inserted )
        {
            // Try insert page
            if ( range.end == start )
            {
                range.end = start + PAGESIZE;
                inserted = 1;
            }
        }

        // Ensure Min Randomness
        if ( page_slot->count <= RMMAP_MINPAGECNT )
        {
            if (range.start + PAGESIZE == range.end)
                forcePageInsert(range.start);
            else
                forceRangeInsert(range.start, range.end);

            if ( inserted )
                return INSERT_DONE;
            else
                return LEAVE_UNDONE;
        }

        size_t try = page_slot->addrs[idx];
        if ( range.end == try )
        {
            deletePageEntry(idx);
            range.end = try + PAGESIZE;

        } else {

            // Insert Range
            if (range.start + PAGESIZE == range.end)
            {
                // We want to keep the sorted order
                movePageSlotBackward(idx);
                page_slot->addrs[idx++] = range.start;

            } else {

                if ( releaseRange(range.start, range.end, GC_QUICK) != INSERT_DONE )
                {
                    // Hack to enforce Range Drop
                    size_t tmp = page_slot->count;
                    page_slot->count = RMMAP_MAXCACHEPAGES;

                    forceRangeInsert(range.start, range.end);

                    page_slot->count = tmp;
                }
            }

            // Reset Range
            range.start = inserted ? page_slot->addrs[idx] :
                          MIN(start, page_slot->addrs[idx]);
            range.end = range.start + PAGESIZE;
            int orig_inserted = inserted;
            inserted = inserted ? 1 : CONDMIN(start, page_slot->addrs[idx], 1, 0);
            if (!orig_inserted && inserted)
                continue;
            deletePageEntry(idx);
        }
    }

    // In case every page is GCed, and sat Min Randomness
    // Last Handle of Range
    if (range.start + PAGESIZE == range.end)
        forcePageInsert(range.start);
    else
        forceRangeInsert(range.start, range.end);

    if (inserted)
        return INSERT_DONE;
    else
        return LEAVE_UNDONE;
}

// GC for AddrRange
int releaseRange( size_t start, size_t end, int flag )
{
    int inserted = 0, idx = 0;
    size_t try;
    size_t ref, ref1, ref2;

    if ( flag == GC_SKIPTRY )
        goto gc_range;

    // Just try merge new range First
    //
    try = searchRangeByAddr(start);

    if ( try < addr_range->count && addr_range->addrs[try].start == end )
    {
        addr_range->addrs[try].start = start;
        ref = addr_range->addrs[try].ref;
        size_list->ent[ref].sz += (end - start);
        fixRangeSzSeq(ref);
        inserted = 1;

        if ( flag == GC_QUICK )
            return INSERT_DONE;

    } else if ( try != 0 && addr_range->addrs[try - 1].end == start ) {

        addr_range->addrs[try - 1].end = end;
        ref = addr_range->addrs[try - 1].ref;
        size_list->ent[ref].sz += (end - start);
        fixRangeSzSeq(ref);
        inserted = 1;

        if ( flag == GC_QUICK )
            return INSERT_DONE;
    }

    // Keep GCing
    //
gc_range:
    idx = 0;
    while ( idx < addr_range->count - 1 )
    {
        if ( addr_range->addrs[idx].end == addr_range->addrs[idx + 1].start )
        {
            // Merge Entry
            addr_range->addrs[idx].end = addr_range->addrs[idx + 1].end;
            ref1 = addr_range->addrs[idx].ref;
            ref2 = addr_range->addrs[idx + 1].ref;
            size_list->ent[ref1].sz += size_list->ent[ref2].sz;

            deleteRangeEntry(idx + 1);
            fixRangeSzSeq(ref1);
            continue;
        }

        idx++;
    }

    if ( inserted )
        return INSERT_DONE;
    else
        return LEAVE_UNDONE;
}

void insertEntry( size_t start, size_t end )
{
    if ( start == end ) return;

    // For single page insertion
    if ( start + PAGESIZE == end )
    {
        if ( page_slot->count >= RMMAP_MAXCACHEPAGES )
            if ( releaseSlot(start) == INSERT_DONE )
                return;

        forcePageInsert(start);

    } else {

    // Range insertion

        if ( addr_range->count >= RMMAP_MAXCACHERANGES )
            if ( releaseRange(start, end, GC_QUICK) == INSERT_DONE )
                return;

        forceRangeInsert(start, end);
    }
}

void deleteRangeEntry( size_t index )
{
    deleteSLEntry(addr_range->addrs[index].ref);
    moveAddrRangeForeward(index, addr_range->count);
}

void deletePageEntry( size_t index )
{
    movePageSlotForeward(index + 1);
}

// Split the Specific Range Entry
// Note for index to be larger than ZERO
// index :  the Entry Index to split in Addr_Range
// start :  start bound to exclude
// end   :  end bound to exclude
void splitRangeEntry( size_t index, size_t start, size_t end )
{
    size_t tmpstart = addr_range->addrs[index].start;
    size_t tmpend = addr_range->addrs[index].end;
    size_t ref = addr_range->addrs[index].ref;
    int filled = 0;

    // First Try Handle One-Sided Situation
    if ( tmpstart == start )
    {
        if ( end == tmpend )
        {
            deleteRangeEntry(index);

        } else if ( end + PAGESIZE == tmpend ) {

            deleteRangeEntry(index);
            insertEntry(end, tmpend);

        } else {

            addr_range->addrs[index].start = end;
            size_list->ent[ref].sz = tmpend - end;
            fixRangeSzSeq(ref);
        }
        return;
    }

    if ( tmpend == end )
    {
        if ( tmpstart + PAGESIZE == start )
        {
            deleteRangeEntry(index);
            insertEntry(tmpstart, start);

        } else {

            addr_range->addrs[index].end = start;
            size_list->ent[ref].sz = start - tmpstart;
            fixRangeSzSeq(ref);
        }
        return;
    }

    // Now For the Two-Sided Case
    // Use forcePageInsert() for Page Insertion To Avoid GC
    if ( tmpstart + PAGESIZE == start )
        forcePageInsert(tmpstart);
    else
    {
        filled = 1;
        addr_range->addrs[index].end = start;
        size_list->ent[ref].sz = start - tmpstart;
        fixRangeSzSeq(ref);
    }

    if ( end + PAGESIZE == tmpend )
        forcePageInsert(end);
    else
    {
        if (filled)
        {
            deleteRangeEntry(index);
            insertEntry(end, tmpend);
        }
        else
        {
            filled = 1;
            addr_range->addrs[index].start = end;
            size_list->ent[ref].sz = tmpend - end;
            fixRangeSzSeq(ref);
        }
    }

    // Both the remainings are inserted into Page_Slot
    if ( !filled )
        deleteRangeEntry(index);
}



