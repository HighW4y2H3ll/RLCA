
#include "dr_api.h"
#include "drmgr.h"
#include "wrapper.h"
#include "error.h"
#include "types.h"

#include "threadlocal.h"

static int tls_idx;

static struct _ThreadMap *tls_map = 0;

static void tlsInitCallback(void *drcontext, bool new_depth)
{
    size_t idx;

    if (new_depth)
    {
        idx = getFirstFreeIdx(tls_map->bitmap);
        if ( idx == (~((size_t)(0))) )
            ERROR("Too Many Thread.");

        setBitMap(tls_map->bitmap, idx);
        drmgr_set_cls_field(drcontext, tls_idx, idx);

    } else {

        idx = drmgr_get_cls_field(drcontext, tls_idx);
    }

    memset(&tls_map->info[idx], 0, sizeof(struct _ThreadINFO));
}

static void tlsExitCallback(void *drcontext, bool process_exit)
{
    if (process_exit)
    {
        size_t idx = drmgr_get_cls_field(drcontext, tls_idx);
        clrBitMap(tls_map->bitmap, idx);
        memset(&tls_map->info[idx], 0, sizeof(struct _ThreadINFO));
    }
}

void initTLS()
{
    tls_map = DRMALLOC(sizeof(struct _ThreadMap) + sizeof(struct _ThreadINFO)*(TLI_DEFAULTSZ - 1));
    if ( !tls_map )
        ERROR("TLS MAP Init Failed");

    tls_map->bitmap = initBitMap(TLI_DEFAULTSZ);
    memset(tls_map->info, 0, sizeof(struct _ThreadINFO)*TLI_DEFAULTSZ);

    tls_idx = drmgr_register_cls_field( tlsInitCallback,
                                        tlsExitCallback );
}

void expandTLS()
{
}

void destroyTLS()
{
    destroyBitMap(tls_map->bitmap);
    DRFREE(tls_map, sizeof(struct _ThreadMap) + sizeof(struct _ThreadINFO)*(tls_map->bitmap->total - 1));
    drmgr_unregister_cls_field( tlsInitCallback,
                                tlsExitCallback,
                                tls_idx );
    tls_idx = 0;
}

void tlsSetFlag(void *drcontext)
{
    size_t idx = drmgr_get_cls_field(drcontext, tls_idx);
    tls_map->info[idx].torestore = 1;
}

void tlsClrFlag(void *drcontext)
{
    size_t idx = drmgr_get_cls_field(drcontext, tls_idx);
    tls_map->info[idx].torestore = 0;
}

bool tlsIsFlagSet(void *drcontext)
{
    size_t idx = drmgr_get_cls_field(drcontext, tls_idx);
    return (tls_map->info[idx].torestore == 1);
}

void tlsSetMoveFlag(void *drcontext)
{
    size_t idx = drmgr_get_cls_field(drcontext, tls_idx);
    tls_map->info[idx].tomove = 1;
}

void tlsClrMoveFlag(void *drcontext)
{
    size_t idx = drmgr_get_cls_field(drcontext, tls_idx);
    tls_map->info[idx].tomove = 0;
}

bool tlsIsMoveSet(void *drcontext)
{
    size_t idx = drmgr_get_cls_field(drcontext, tls_idx);
    return (tls_map->info[idx].tomove == 1);
}

void tlsChkStk(void *drcontext)
{
    size_t idx = drmgr_get_cls_field(drcontext, tls_idx);
    tls_map->info[idx].chkstk = 1;
}

void tlsUnChkStk(void *drcontext)
{
    size_t idx = drmgr_get_cls_field(drcontext, tls_idx);
    tls_map->info[idx].chkstk = 0;
}

bool isTlsChkStk(void *drcontext)
{
    size_t idx = drmgr_get_cls_field(drcontext, tls_idx);
    return (tls_map->info[idx].chkstk == 1);
}

void tlsSetMemWrite(void *drcontext)
{
    size_t idx = drmgr_get_cls_field(drcontext, tls_idx);
    tls_map->info[idx].memwrite = 1;
}

void tlsClrMemWrite(void *drcontext)
{
    size_t idx = drmgr_get_cls_field(drcontext, tls_idx);
    tls_map->info[idx].memwrite = 0;
}

bool isTlsMemWrite(void *drcontext)
{
    size_t idx = drmgr_get_cls_field(drcontext, tls_idx);
    return (tls_map->info[idx].memwrite == 1);
}

void tlsReset( void *drcontext )
{
    size_t idx = drmgr_get_cls_field(drcontext, tls_idx);
    memset(&tls_map->info[idx], 0, sizeof(struct _ThreadINFO));
}

void tlsSetReg(void *drcontext, size_t idx, reg_t value)
{
    size_t i = drmgr_get_cls_field(drcontext, tls_idx);
    tls_map->info[i].reg[idx] = value;
}

void tlsGetReg(void *drcontext, size_t idx, reg_t *dst)
{
    size_t i = drmgr_get_cls_field(drcontext, tls_idx);
    *dst = tls_map->info[i].reg[idx];
}

void tlsGetTaintReg(void *drcontext, size_t idx, size_t *key)
{
    size_t i = drmgr_get_cls_field(drcontext, tls_idx);
    *key = tls_map->info[i].taint_reg[idx];
}

void tlsSetTaintReg(void *drcontext, size_t idx, size_t key)
{
    size_t i = drmgr_get_cls_field(drcontext, tls_idx);
    tls_map->info[i].taint_reg[idx] = key;
}

void tlsGetDst(void *drcontext, size_t *dst)
{
    size_t i = drmgr_get_cls_field(drcontext, tls_idx);
    *dst = tls_map->info[i].dst;
}

void *tlsRegSlotAddr(void *drcontext, size_t ireg)
{
    size_t i = drmgr_get_cls_field(drcontext, tls_idx);
    return &(tls_map->info[i].reg[ireg]);
}

void *tlsDstSlotAddr(void *drcontext)
{
    size_t i = drmgr_get_cls_field(drcontext, tls_idx);
    return &(tls_map->info[i].dst);
}

void *tlsSrcSlotAddr(void *drcontext)
{
    size_t i = drmgr_get_cls_field(drcontext, tls_idx);
    return &(tls_map->info[i].src);
}

void *tlsDstOpndAddr(void *drcontext, size_t index)
{
    size_t i = drmgr_get_cls_field(drcontext, tls_idx);
    return &(tls_map->info[i].tmpdst[index]);
}

instr_t *tlsGetMovInst(void *drcontext, size_t index)
{
    size_t i = drmgr_get_cls_field(drcontext, tls_idx);
    return &(tls_map->info[i].MOV[index]);
}

instr_t *tlsGetLeaInst(void *drcontext)
{
    size_t i = drmgr_get_cls_field(drcontext, tls_idx);
    return &(tls_map->info[i].LEA);
}


