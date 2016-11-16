
// Enable struct _instr_t
#define DR_FAST_IR

#include "memtrace.h"
#include "mmap.h"
#include "parsemap.h"
#include "wrapper.h"
#include "types.h"

#include "threadlocal.h"
#include <unistd.h>

#define ISREG(X)    (((X) >= 0) && ((X) < 16))

size_t mt_key_sz = MT_DEFAULT_SZ;
size_t mt_taint_sz = MT_DEFAULT_SZ;

static struct KeyTab *key_tab = 0;
static struct TaintTab *taint_tab = 0;

void initKeyTab( int flag )
{
    size_t size = sizeof(struct KeyTab) + sizeof(struct KeyEnt)*(MT_KEY_SZ - 1);
    struct KeyTab *tab = key_tab;
    size_t nsz;

    if ( !key_tab )
    {
        key_tab = DRMALLOC(size);
        if ( !key_tab )
            ERROR("MemTrace Key Tab Init Failed.");

        memset(key_tab, 0, size);
        return;
    }

    if ( flag == INIT_EXPAND )
    {
        if ( key_tab->count < MT_KEY_SZ )
            return;

        MT_KEY_SZ <<= 1;

        // Avoid Overflow
        if ( MT_KEY_SZ <= MT_DEFAULT_SZ )
            ERROR("Too Many Taint.");

        nsz = sizeof(struct KeyTab) + sizeof(struct KeyEnt)*(MT_KEY_SZ - 1);

        key_tab = DRMALLOC(nsz);
        if ( !key_tab )
            ERROR("MemTrace Key Tab Expand Failed.");

        memset(key_tab, 0, nsz);
        memcpy(key_tab, tab, size);

        DRFREE(tab, size);
    }
}

void initTaintTab( int flag )
{
    size_t size = sizeof(struct TaintTab) + sizeof(struct TaintEnt)*(MT_TAINT_SZ - 1);
    struct TaintTab *tab = taint_tab;
    size_t nsz;

    if ( !taint_tab )
    {
        taint_tab = DRMALLOC(size);
        if ( !taint_tab )
            ERROR("MemTrace Taint Tab Init Failed.");

        memset(taint_tab, 0, size);
        return;
    }

    if ( flag == INIT_EXPAND )
    {
        if ( taint_tab->count < MT_TAINT_SZ )
            return;

        MT_TAINT_SZ <<= 1;

        // Avoid Overflow
        if ( MT_TAINT_SZ <= MT_DEFAULT_SZ )
            ERROR("Too Many Taint.");

        nsz = sizeof(struct TaintTab) + sizeof(struct TaintEnt)*(MT_TAINT_SZ - 1);

        taint_tab = DRMALLOC(nsz);
        if ( !taint_tab )
            ERROR("MemTrace Taint Tab Expand Failed.");

        memset(taint_tab, 0, nsz);
        memcpy(taint_tab, tab, size);

        DRFREE(tab, size);
    }
}

void initMemTrace( int flag )
{
    initAddrRange(flag);
    initPageSlot(flag);

    // Init Mem Trace
    initKeyTab(flag);
    initTaintTab(flag);

    setMinAddr();
    pid_t pid = getpid();
    parseMaps(pid);
}

void destroyMemTrace()
{
    if (key_tab)
        DRFREE(key_tab, sizeof(struct KeyTab) + sizeof(struct KeyEnt)*(MT_KEY_SZ - 1));

    if (taint_tab)
        DRFREE(taint_tab, sizeof(struct TaintTab) + sizeof(struct TaintEnt)*(MT_TAINT_SZ - 1));
}

/*
void setRegTaint(void *drcontext, size_t idx, reg_t key)
{
    size_t i = 0;
    for (; i < key_tab->count; i++)
    {
        if ( key_tab->keys[i].key == (size_t)key )
        {
            key_tab->keys[i].ref_cnt++;
            tlsSetTaintReg(drcontext, idx, i);
            return;
        }
    }

    // New Key
    if ( key_tab->count >= MT_KEY_SZ )
        initMemTrace(INIT_EXPAND);

    key_tab->keys[i].key = key;
    key_tab->keys[i].ref_cnt++;
    key_tab->count++;
    tlsSetTaintReg(drcontext, idx, i);
}
*/

bool isAddrTainted( size_t reg )
{
    size_t i = 0;

    for (; i < taint_tab->count; i++ )
    {
        if ( taint_tab->taints[i].val == reg )
            return true;
    }

    return false;
}

void insertTaintTab(size_t reg, size_t index)
{
    if ( taint_tab->count >= MT_TAINT_SZ )
        initMemTrace(INIT_EXPAND);

    size_t end = taint_tab->count++;

    taint_tab->taints[end].val = reg;
    taint_tab->taints[end].ref = index;
}

void setAddrTaint(reg_t ref, reg_t key)
{
    size_t i = 0;
    for (; i < key_tab->count; i++)
    {
        if ( key_tab->keys[i].key == (size_t)key )
        {
            key_tab->keys[i].ref_cnt++;
            insertTaintTab(ref, i);
            return;
        }
    }

    // New Key
    if ( key_tab->count >= MT_KEY_SZ )
        initMemTrace(INIT_EXPAND);

    key_tab->keys[i].key = key;
    key_tab->keys[i].ref_cnt++;
    key_tab->count++;
    insertTaintTab(ref, i);
}

void rmAddrTaint(size_t reg)
{
    size_t idx = 0;
    size_t fix, end, ref;
    size_t fidx = 0;
    for (; idx < taint_tab->count; idx++ )
    {
        if ( taint_tab->taints[idx].val == reg )
        {
            end = --taint_tab->count;
            ref = taint_tab->taints[idx].ref;

            taint_tab->taints[idx].val = taint_tab->taints[end].val;
            taint_tab->taints[idx].ref = taint_tab->taints[end].ref;
            taint_tab->taints[end].val = taint_tab->taints[end].ref = 0;

            // Check If We need to remove Key
            if ( --key_tab->keys[ref].ref_cnt == 0 )
            {
                end = --key_tab->count;

                key_tab->keys[ref].key = key_tab->keys[end].key;
                key_tab->keys[ref].ref_cnt = key_tab->keys[end].ref_cnt;

                fix = key_tab->keys[end].key;

                key_tab->keys[end].key = key_tab->keys[end].ref_cnt = 0;

                // Fix Refs
                fidx = 0;
                for (; fidx < taint_tab->count; fidx++ )
                {
                    if ( taint_tab->taints[fidx].val == fix )
                    {
                        taint_tab->taints[fidx].ref = ref;
                    }
                }
            }

            return;
        }
    }
}

void populateAddrTaint(reg_t src, reg_t ref)
{
    size_t i = 0, idx;
    for (; i < taint_tab->count; i++ )
    {
        if ( taint_tab->taints[i].val == src )
        {
            idx = taint_tab->taints[i].ref;
            break;
        }
    }

    key_tab->keys[idx].ref_cnt++;
    insertTaintTab(ref, idx);
}

size_t mtIterStart()
{
    return 0;
}

size_t mtIterEnd()
{
    return taint_tab->count;
}

size_t mtIterGetTaint( size_t iter )
{
    return taint_tab->taints[iter].val;
}

void mtIterUpdateKey( size_t iter, size_t nkey )
{
    size_t ref = taint_tab->taints[iter].ref;
    key_tab->keys[ref].key = nkey;
}

void mtIterUpdateTaint( size_t iter, size_t ntaint )
{
    taint_tab->taints[iter].val = ntaint;
}

bool isMemTraceInit()
{
    return ( taint_tab != 0 && key_tab != 0 );
}

void trimTaints( bool (*trim_func)(size_t taint, void *_args), void *args )
{
    if ( !taint_tab->count )    return;

    size_t i = taint_tab->count;
    size_t val;
    for (; i > 0; i-- )
    {
        val = taint_tab->taints[i - 1].val;
        if ( (*trim_func)(val, args) )
            rmAddrTaint(val);
    }
}

instr_t *mtMovInst(void *drcontext, void *op1, void *op2, size_t index)
{
    struct _instr_t *instr = tlsGetMovInst(drcontext, index);
    opnd_t *dst = tlsDstOpndAddr(drcontext, index);

    memset(instr, 0, sizeof(instr_t));
    memset(dst, 0, sizeof(opnd_t));

    instr_set_isa_mode(instr, DR_ISA_AMD64);

    //
    // # Build Instr In-place
    //
    // Set Num of Opnds
    instr->num_srcs = 1;
    instr->num_dsts = 1;
    // Set Dst ( Src0 is static )
    instr->dsts = dst;
    //instr_being_modified(instr, false);
    //instr_set_operands_valid(instr, true);

    // We could only meet 3 cases:
    // r  <-  m     :   mov_ld : A1 / 8B
    // r  <-  r     :   mov_ld : 8B
    // m  <-  r     :   mov_st : A3 / 89
    if ( ISREG((reg_t)op1) )
    {
        // Set Opcode
        instr_set_opcode(instr, OP_mov_ld);

        // Set Opnd
        instr_set_dst(instr, 0, opnd_create_reg(TODRREG((int)op1)));

        if ( ISREG((reg_t)op2) )
            instr_set_src(instr, 0, opnd_create_reg(TODRREG((int)op2)));
        else
            instr_set_src(instr, 0, opnd_create_abs_addr(op2, OPSZ_PTR));

    } else {

        // Set Opcode
        instr_set_opcode(instr, OP_mov_st);

        // Set Opnd
        instr_set_src(instr, 0, opnd_create_reg(TODRREG((int)op2)));
        instr_set_dst(instr, 0, opnd_create_abs_addr(op1, OPSZ_PTR));
    }

    return instr;
}

// lea op1, op2
// op1 : Reg
instr_t *mtLeaInst(void *drcontext, size_t op1, opnd_t op2)
{
    struct _instr_t *instr = tlsGetLeaInst(drcontext);
    opnd_t *dst = tlsDstOpndAddr(drcontext, 4);

    memset(instr, 0, sizeof(instr_t));
    memset(dst, 0, sizeof(opnd_t));

    instr_set_isa_mode(instr, DR_ISA_AMD64);

    instr->num_srcs = 1;
    instr->num_dsts = 1;
    instr->dsts = dst;

    instr_set_dst(instr, 0, opnd_create_reg(TODRREG((int)op1)));

    if ( opnd_is_base_disp(op2) )
    {
        opnd_set_size(&op2, OPSZ_0);
        instr_set_opcode(instr, OP_lea);
        instr_set_src(instr, 0, op2);

    } else if ( opnd_is_rel_addr(op2) || opnd_is_abs_addr(op2) ) {

        instr_set_opcode(instr, OP_mov_imm);
        instr_set_src(instr, 0, OPND_CREATE_INTPTR(opnd_get_addr(op2)));
    }

    return instr;
}


