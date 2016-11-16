
#ifndef _RBASE
#define _RBASE

#include "types.h"

#define PAGESHIFT    12
#define PAGESIZE    (1 << PAGESHIFT)
#define PAGEMASK    (PAGESIZE - 1)

#define MIN(X,Y)    (((X)<(Y)) ? (X) : (Y))
#define CONDMIN(X,Y,A,B)    (((X)<(Y)) ? (A) : (B))

#define LEAVE_UNDONE    0
#define INSERT_DONE     1

#define DIRECT_PASS     -1

#define CHKMAP_NOTMMAPED       -1
#define CHKMAP_ALLRIGHT        0

#define RMMAP_MAXCACHEPROC  8
#define RMMAP_MAXCACHEPAGES max_cache_pages
#define RMMAP_MAXCACHERANGES max_cache_ranges
#define RMMAP_MINPAGECNT  16
#define RMMAP_PADPAGE   5

extern size_t max_cache_pages;
extern size_t max_cache_ranges;


enum { INIT_INIT, INIT_EXPAND };

#define MMAP_MINADDR    mmap_min_addr
#if defined(__x86_64__)
#define MMAP_MAXADDR    0x800000000000
#elif defined(__i386)
#define MMAP_MAXADDR    0xc0000000
#elif defined(__arm__)
#define MMAP_MAXADDR    0xc0000000
#endif

enum { GC_QUICK, GC_SLOW, GC_SKIPTRY };


/*
 * Global Var Declare
 */
extern size_t mmap_min_addr;

// Storage for ALL Single Pages
extern struct Page *page_slot;

// No Single Page should Appear Here
extern struct AddrRange *addr_range;

// Look-aside List for Addr_Range
extern struct ARSizeList *size_list;

// Global Flag Identifies Random Drop Behavior
extern int pagedrop, rangedrop;


// Constructor
void initAddrRange ( int flag );
void initPageSlot ( int flag );

// Destructor
void freeAddrRange ( void );
void freePageSlot ( void );

// Internal Struct Operations
// **************************
// Internal Function
//void moveAddrRangeBackward ( size_t istart, size_t iend );
//void moveAddrRangeForeward ( size_t istart, size_t iend );
//void movePageSlotBackward ( size_t );
//void movePageSlotForeward ( size_t );

void fixRangeSzSeq ( size_t );

void insertEntry ( size_t, size_t );
void deleteRangeEntry ( size_t );
void deletePageEntry ( size_t );

void splitRangeEntry ( size_t, size_t, size_t );

int releaseSlot ( size_t );
int releaseRange ( size_t, size_t, int );

#endif
