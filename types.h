

#ifndef _RTYPES
#define _RTYPES

#include <stdlib.h>
#include "dr_api.h"

struct AddrRangeEntry {
    size_t start;
    size_t end;
    size_t ref;
};

struct AddrRange {
    size_t count;
    struct AddrRangeEntry addrs[1];
};

// Look-aside List for AddrRange
// Sorted Assend by Size
struct ARSizeEntry {
    size_t idx;
    size_t sz;
};

struct ARSizeList {
    size_t count;
    struct ARSizeEntry ent[1];
};

struct Page {
    size_t count;
    size_t addrs[1];
};

enum { _RAX,
       _RBX,
       _RCX,
       _RDX,
       _RSI,
       _RDI,
       _RBP,
       _RSP,
       _R8,
       _R9,
       _R10,
       _R11,
       _R12,
       _R13,
       _R14,
       _R15 };

static int DREG_MAP[] = {
    DR_REG_RAX, DR_REG_RBX, DR_REG_RCX, DR_REG_RDX,
    DR_REG_RSI, DR_REG_RDI, DR_REG_RBP, DR_REG_RSP,
    DR_REG_R8,  DR_REG_R9,  DR_REG_R10, DR_REG_R11,
    DR_REG_R12, DR_REG_R13, DR_REG_R14, DR_REG_R15  };

#define TODRREG(X)  DREG_MAP[(X)]

#ifdef X86

#define REG_LIST_ACCESSOR(V)  \
    V(xax)  \
    V(xbx)  \
    V(xcx)  \
    V(xdx)  \
    V(xsi)  \
    V(xdi)  \
    V(xbp)  \
    /* Skip xsp */  \
    V(r8)   \
    V(r9)   \
    V(r10)  \
    V(r11)  \
    V(r12)  \
    V(r13)  \
    V(r14)  \
    V(r15)

#define REG_BACKUP_LIST_ACCESSOR(V) \
    V(xax, _RAX)  \
    V(xbx, _RBX)  \
    V(xcx, _RCX)  \
    V(xdx, _RDX)  \
    V(xsi, _RSI)  \
    V(xdi, _RDI)  \
    V(xbp, _RBP)  \
    /* Skip xsp */  \
    V(r8,  _R8)   \
    V(r9,  _R9)   \
    V(r10, _R10)  \
    V(r11, _R11)  \
    V(r12, _R12)  \
    V(r13, _R13)  \
    V(r14, _R14)  \
    V(r15, _R15)

#endif

#endif

