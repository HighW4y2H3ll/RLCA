#include "error.h"
#include "randutil.h"
#include "mmap.h"
#include "utils.h"
#include "parsemap.h"



// Destruct All the Structs on Exit
void rmexit()
{
    freeAddrRange();
    freePageSlot();
}

static void onInit( pid_t pid, int flag )
{
    initAddrRange(flag);
    initPageSlot(flag);

    setMinAddr();
    parseMaps(pid);
}

// Remove All the Occurance Records in range hint, hint + sz
void removeRangeOccurance( size_t hint, size_t sz )
{
    size_t istart = searchRangeByAddr(hint);
    size_t iend = searchRangeByAddr(hint + sz);

    // Remove All the Occurance
    if ( istart != 0 && hint < addr_range->addrs[istart - 1].end )
    {
        size_t split = istart - 1;
        if ( addr_range->addrs[istart - 1].start + PAGESIZE == hint )
            istart--;

        splitRangeEntry(split, hint, addr_range->addrs[split].end);
    }

    if ( iend != 0 && hint + sz < addr_range->addrs[iend - 1].end )
    {
        size_t split = iend - 1;
        if ( hint + sz + PAGESIZE == addr_range->addrs[iend - 1].end )
            iend--;

        splitRangeEntry(split, addr_range->addrs[split].start, hint + sz);
    }

    for (; istart < iend; iend-- )
    {
        deleteRangeEntry(iend - 1);
    }

    // Remove Page Occurance
    for ( istart = page_slot->count; istart > 0; istart-- )
    {
        if ( page_slot->addrs[istart - 1] < hint + sz
           && page_slot->addrs[istart - 1] >= hint )
            deletePageEntry(istart - 1);
    }
}

// Garbage Collection Procedure
void GC(pid_t pid)
{
#ifdef BRK_ON
    onInit(pid, INIT_EXPAND);
    return;
#endif

    size_t page, ref;
    size_t idx;
    releaseRange(0, 0, GC_SKIPTRY);

    // Get the last entry of Page_Slot to do releaseSlot()
    //
    // We are done if Page_Slot Empty
    if ( page_slot->count == 0 )    return;

    size_t addr = page_slot->addrs[--page_slot->count];
    if ( releaseSlot(addr) == LEAVE_UNDONE )
        insertEntry(addr, addr + PAGESIZE);

    // Merge Addr_Range Again
    releaseRange(0, 0, GC_SKIPTRY);

    // Try Handle the last descreate Pages
    size_t i = 0;
    for (; i < page_slot->count; i++ )
    {
        page = page_slot->addrs[i];
        idx = searchRangeByAddr(page);

        if ( idx == 0 && page + PAGESIZE == addr_range->addrs[idx].start )
        {
            addr_range->addrs[idx].start = page;
            ref = addr_range->addrs[idx].ref;
            size_list->ent[ref].sz += PAGESIZE;
            fixRangeSzSeq(ref);
            continue;
        }
        if ( idx != 0 && page == addr_range->addrs[idx - 1].end )
        {
            addr_range->addrs[idx - 1].end += PAGESIZE;
            ref = addr_range->addrs[idx - 1].ref;
            size_list->ent[ref].sz += PAGESIZE;
            fixRangeSzSeq(ref);
            continue;
        }
    }

    // Do It One Last Time
    releaseRange(0, 0, GC_SKIPTRY);
}

// Fill Page_Slot to satify Min Randomness
void fillPageSlot()
{
    if ( page_slot->count >= RMMAP_MINPAGECNT )
        return;

    size_t fill = RMMAP_MINPAGECNT - page_slot->count;
    size_t idx = 0;
    for (; fill && idx < addr_range->count; idx++ )
    {
        size_t gap = size_list->ent[idx].sz >> PAGESHIFT;
        if ( fill >= gap )
        {
            // Fill page_slot
            size_t ref = size_list->ent[idx].idx;
            size_t start = addr_range->addrs[ref].start;
            size_t end = addr_range->addrs[ref].end;
            for (; start < end; start += PAGESIZE)
                forcePageInsert(start);

            deleteRangeEntry(ref);
            fill -= gap;

        } else {

            // Just make it to RMMAP_MINPAGECNT
            size_t ref = size_list->ent[idx].idx;
            size_t start = addr_range->addrs[ref].start;
            while ( fill-- )
            {
                forcePageInsert(start);
                start += PAGESIZE;
            }

            // Keep Addr_Range clean from pages
            if ( start + PAGESIZE == addr_range->addrs[ref].end )
            {
                forcePageInsert(start);
                deleteRangeEntry(ref);
                break;
            }

            // Still got remainings
            addr_range->addrs[ref].start = start;
            size_list->ent[idx].sz = addr_range->addrs[ref].end - start;

            fixRangeSzSeq(idx);
            break;
        }
    }
}

// Return a randomized address for mmap
void *rmmap( pid_t pid, void *hint, size_t len, int flags, int overlap_tolerate )
{
    size_t sz = (len + PAGEMASK) & (~PAGEMASK);
    void *addr = 0;
    int mapfixed = 0;
    int findnear = 0;
    int likelyfail = 0;
    int lasttried = 0;

    size_t idx, bestfit, gap, padding, ref;

    if ( !page_slot || !addr_range )
        onInit(pid, INIT_INIT);
    
    if ( flags & MAP_FIXED )
    {
        if ( !hint )
            return DIRECT_PASS;

        // Must be page aligned
        if ( (size_t)hint & PAGEMASK )
            return DIRECT_PASS;

        mapfixed = 1;
        addr = hint;

    } else {

        // Find nearest hint
        if ( hint )
            findnear = 1;

        addr = (size_t)hint & (~PAGEMASK);

    }

retry:
    // Optimize for 1 page mmap
    // since PHK malloc mmap 1 page extensively for small chunk
    if ( sz == PAGESIZE )
    {
        // Lazily Ensure randomization
        if ( page_slot->count < RMMAP_MINPAGECNT )
            fillPageSlot();

        if ( mapfixed )
        {
            // Search Page_Slot
            idx = 0;
            for (; idx < page_slot->count; idx++ )
            {
                if ( page_slot->addrs[idx] == addr )
                {
                    deletePageEntry(idx);
                    return addr;
                }
            }
            
            // Not Found in Page_Slot, Try Addr_Range
            idx = searchRangeByAddr(addr);
            // Check Equal
            if ( idx < addr_range->count && addr == addr_range->addrs[idx].start )
            {
                splitRangeEntry(idx, addr, addr + PAGESIZE);
                return addr;
            }
            // Check Overlap
            if ( idx != 0 && addr < addr_range->addrs[idx - 1].end )
            {
                splitRangeEntry(idx - 1, addr, addr + PAGESIZE);
                return addr;
            }

            // Not Found Everywhere
            if ( lasttried )
                goto direct_out;
            likelyfail = 1;

        } else {

            // Randomly pick one in Page_Slot
            idx = randNumMod(page_slot->count);
            addr = page_slot->addrs[idx];
            deletePageEntry(idx);

            // TODO Maybe... Just Ignore hint
            if ( findnear )
            {
            }

            return addr;
        }
    } else {

        // Range Mmap
        if ( mapfixed )
        {
            idx = searchRangeByAddr(addr);

            // Perfect Fit
            // -- Check Equal
            if ( idx < addr_range->count
               && addr == addr_range->addrs[idx].start
               && addr + sz <= addr_range->addrs[idx].end )
            {
                splitRangeEntry(idx, addr, addr + sz);
                return addr;
            }
            // -- Check Overlap
            if ( idx != 0
               && addr < addr_range->addrs[idx - 1].end
               && addr + sz <= addr_range->addrs[idx - 1].end )
            {
                splitRangeEntry(idx - 1, addr, addr + sz);
                return addr;
            }

            // Bad Fit
            // We have an overlapped Mmap
            if ( lasttried )
            {
                // Just to Make Sure We Really Have to Do This
                if ( rangedrop || pagedrop )
                {
                    goto last_try;
                }

                if ( !overlap_tolerate )
                    return DIRECT_PASS;

                removeRangeOccurance(addr, sz);

                return addr;
            }

            if ( likelyfail )
                goto last_try;

            // Try Merge
            GC(pid);
            likelyfail = 1;
            goto retry;

            // Not Found in the Range
            if ( lasttried )
                goto direct_out;
            likelyfail = 1;
            goto last_try;

        } else {

            // Randomly Pick a Range
            bestfit = searchRangeBySz(sz);

            if ( bestfit >= size_list->count )
            {
                // Bail Out
                if ( lasttried )
                    goto direct_out;
                if ( likelyfail )
                    goto last_try;

                // Try Merge & Fit
                GC(pid);
                likelyfail = 1;
                goto retry;
            }

            // Randomly select a range
            idx = bestfit + randNumMod(size_list->count - bestfit);
            gap = size_list->ent[idx].sz;
            padding = randNumMod( MIN( RMMAP_PADPAGE, (gap - sz)>>PAGESHIFT ) );
            ref = size_list->ent[idx].idx;
            addr = addr_range->addrs[ref].start + PAGESIZE * padding;

            splitRangeEntry(ref, addr, addr + sz);

            return addr;
        }
    }

last_try:
    if ( likelyfail && ( pagedrop || rangedrop ) )
    {
        lasttried = 1;

        // parseMaps again ?
        onInit(pid, INIT_EXPAND);
        goto retry;
    }

direct_out:
    // Unlikey to mmap such a range
    return DIRECT_PASS;
}

// Depreciated : Should NOT be used explicitly
//               Always use checkRangeMaped()
// Check the specified address is mmaped
// Return CHKMAP_NOTMMAPED, if not mmaped
// Return CHKMAP_ALLRIGHT, if all mmaped
int checkPageMaped( size_t hint )
{
    // Check Page_Slot first
    size_t idx = 0;
    for (; idx < page_slot->count; idx++ )
    {
        if ( page_slot->addrs[idx] == hint )
            return CHKMAP_NOTMMAPED;
    }

    // Not Found
    return CHKMAP_ALLRIGHT;
}

// Check the specified address range is fully mmaped
// Return CHKMAP_NOTMMAPED, if not mmaped
// Return CHKMAP_ALLRIGHT, if all mmaped
int checkRangeMaped( pid_t pid, size_t hint, size_t size )
{
    size_t idx, rsz, addr;

range_check_retry:

    idx = searchRangeByAddr(hint);

    // Check Equal
    if ( idx < addr_range->count && addr_range->addrs[idx].start == hint )
        return CHKMAP_NOTMMAPED;

    // Check Overlap
    if ( idx != 0 && addr_range->addrs[idx - 1].end > hint )
        return CHKMAP_NOTMMAPED;
    if ( idx < addr_range->count && addr_range->addrs[idx].start < hint + size )
        return CHKMAP_NOTMMAPED;

check_page_slot:
    // Check Page_Slot
    rsz = size >> PAGESHIFT;
    addr = hint;
    while (rsz--)
    {
        if ( checkPageMaped(addr) != CHKMAP_ALLRIGHT )
            return CHKMAP_NOTMMAPED;
        addr += PAGESIZE;
    }

    // Check if we miss anything due to Page/Range Drop
    if ( rangedrop || pagedrop )
    {
        onInit(pid, INIT_EXPAND);
        goto range_check_retry;
    }

    return CHKMAP_ALLRIGHT;
}

void rmunmap( pid_t pid, void *hint, size_t len )
{

    if ( !page_slot || !addr_range )
        onInit(pid, INIT_INIT);

    if ( (size_t)hint & PAGEMASK )
        return;

    size_t sz = (len + PAGEMASK) & (~PAGEMASK);

    // Try to Handle Malicious Unmap
    if ( checkRangeMaped(pid, hint, sz) != CHKMAP_ALLRIGHT )
    {
        removeRangeOccurance(hint, sz);
    }

    // Normal Branch
    insertEntry(hint, hint + sz);
}

void *rmremap( pid_t pid, void *oaddr, size_t old_size, size_t new_size, int flags, void *naddr )
{

    int maymove = 0, fixed = 0;

    if ( flags & MREMAP_MAYMOVE )
        maymove = 1;
    if ( flags & MREMAP_FIXED )
        fixed = 1;

    // MREMAP_FIXED always follow MREMAP_MAYMOVE
    if ( fixed && !maymove )
        return DIRECT_PASS;

    if ( (size_t)oaddr & PAGEMASK )
        return DIRECT_PASS;

    if ( !page_slot || !addr_range )
        onInit(pid, INIT_INIT);

    size_t osz = (old_size + PAGEMASK) & (~PAGEMASK);
    size_t nsz = (new_size + PAGEMASK) & (~PAGEMASK);

    // new_size cannot be ZERO
    if ( nsz == 0 )
        return DIRECT_PASS;

    // Handle MREMAP_FIXED
    if ( fixed )
    {
        if ( (size_t)naddr & (~PAGEMASK) )
            return DIRECT_PASS;

        // oaddr & naddr cannot overlap
        if ( oaddr <= naddr && oaddr + osz > naddr )
            return DIRECT_PASS;
        if ( naddr <= oaddr && naddr + nsz > oaddr )
            return DIRECT_PASS;

        // old_size must be fully mmaped, NOTE old_size can be 0
        if ( checkRangeMaped(pid, oaddr, osz) != CHKMAP_ALLRIGHT )
            return DIRECT_PASS;

        // Unmap oaddr + old_size
        insertEntry(oaddr, oaddr + osz);
        // Mmap naddr + new_size
        removeRangeOccurance(naddr, nsz);
        return naddr;
    }

    // If Shrink, Simply Unmap the tip
    // Didn't Check in the Linux Source, If old_size is fully mmaped
    // We keep it here
    if ( nsz <= osz )
    {
        insertEntry(oaddr + osz, oaddr + nsz);
        return oaddr;
    }

    // Handle MREMAP_MAYMOVE
    if ( maymove )
    {
        // Check whether old_size is fully mmaped
        if ( checkRangeMaped(pid, oaddr, osz) != CHKMAP_ALLRIGHT )
            return DIRECT_PASS;

        // Unmap
        insertEntry(oaddr, oaddr + osz);
        // Mmap at new location
        return rmmap(pid, 0, nsz, 0, 1);
    }

    // Handle Default: Direct Expand
    if ( !flags )
    {
        // Check whether old_size is fully mmaped
        if ( checkRangeMaped(pid, oaddr, osz) != CHKMAP_ALLRIGHT )
            return DIRECT_PASS;

        // Check Unmmaped
        size_t addr = oaddr + osz;
        size_t sz = nsz - osz;
        size_t cnt = sz >> PAGESHIFT;

        while ( cnt-- )
        {
            if ( checkRangeMaped(pid, addr, addr + PAGESIZE) == CHKMAP_ALLRIGHT )
                return DIRECT_PASS;

            addr += PAGESIZE;
        }

        if ( rmmap(pid, oaddr + osz, sz, MAP_FIXED, 1) == DIRECT_PASS )
            return DIRECT_PASS;
        else
            return oaddr;
    }

    // Unknown Flags
    return DIRECT_PASS;
}

