#include "dr_api.h"
#include "drmgr.h"
#include "drutil.h"
#include <sys/syscall.h>

#include "mmap.h"
#include "brk.h"
#include "threadlocal.h"
#include "memtrace.h"

#ifndef __NR_mmap
#define __NR_mmap   90
#endif
#ifndef __NR_mmap2
#define __NR_mmap2   192
#endif
#ifndef __NR_munmap
#define __NR_munmap   91
#endif
#ifndef __NR_mremap
#define __NR_mremap   163
#endif

#ifdef DEBUG_MODE
#include <signal.h>
#include <fcntl.h>
#endif

typedef struct {
    size_t base;
    size_t sp;
} STF_TYPE;

static void *mutex;

static int start_trace;

static bool stackTaintFix( size_t taint, void *args )
{
    STF_TYPE *p = args;
    return ( taint >= p->base && taint < p->sp );
}

static void funcChkStk()
{
    //dr_printf("funcChkStk\n");
    void *drcontext = dr_get_current_drcontext();

    struct Brk bi;
    getBrkInfo(&bi);

    reg_t _sp;
    tlsGetReg(drcontext, _RSP, &_sp);
    tlsSetReg(drcontext, _RSP, 0);

    // Check Only Current SP Page
    reg_t _base = _sp & (~(reg_t)PAGEMASK);

    STF_TYPE arg = { _base, _sp };

    // Check Taint
    trimTaints( stackTaintFix, &arg );
    //dr_printf("funcChkStk DEBUG\n");
}

static void funcChkTaint()
{
    //dr_printf("funcChkTaint\n");
    void *drcontext = dr_get_current_drcontext();

    struct Brk bi;
    getBrkInfo(&bi);
    
    // Check Taint
    size_t ptr;
    reg_t td;
    tlsGetDst(drcontext, &td);

    // San Check
    if ( td > MMAP_MAXADDR || td < MMAP_MINADDR )
        return;

    //dr_printf("Taint %p\n", td);

    if ( dr_safe_read(td, sizeof(size_t), &ptr, NULL) )
    {
        if ( bi.base <= (size_t)ptr
           && bi.end >= (size_t)ptr )
        {
            if ( isAddrTainted(td) )
                rmAddrTaint(td);

            setAddrTaint(td, ptr);

        } else {

            if ( isAddrTainted(td) )
                rmAddrTaint(td);
        }
    }
    //dr_printf("funcChkTaint DEBUG\n");
}

static dr_emit_flags_t
app2appFunc( void *drcontext, void *tag, instrlist_t *bb,
             bool for_trace, bool translating )
{
    drutil_expand_rep_string(drcontext, bb);
    return DR_EMIT_DEFAULT;
}

static void onExit( void )
{
    rbrkexit();
    rmexit();
    destroyTLS();
    dr_mutex_destroy(mutex);
    drmgr_unregister_bb_app2app_event(app2appFunc);
    drmgr_exit();
}

static void funcSyscall()
{
    process_id_t pid;
    thread_id_t tid;
    reg_t hint, len, prot, flag;
    reg_t addr;
    reg_t naddr, oaddr, osz, nsz, sz, obase;
    reg_t end;
    int err;
    struct Brk bi;
    size_t iter, ptr, taint, tmp;
    size_t sysnum;
    size_t r;
    void *drcontext;
    dr_mcontext_t mc = {sizeof(mc), DR_MC_ALL};

    drcontext = dr_get_current_drcontext();
    dr_get_mcontext(drcontext, &mc);

    sysnum = mc.xax;

    dr_mutex_lock(mutex);

    if ( sysnum == __NR_mmap
#if defined(__NR_mmap2)
       || sysnum == __NR_mmap2
#endif
       )
    {
        pid = dr_get_process_id();
        tid = dr_get_thread_id(drcontext);
        hint = mc.xdi;
        len = mc.xsi;
        prot = mc.xdx;
        flag = mc.r10;

        addr = rmmap(pid, hint, len, flag, 1);

        if ( addr == DIRECT_PASS )
            goto out;

        flag |= MAP_FIXED;
        mc.xdi = addr;
        mc.r10 = flag;

        dr_set_mcontext(drcontext, &mc);
        //dr_printf("MMAP DEBUG\n");

    } else if ( sysnum == __NR_munmap ) {

        pid = dr_get_process_id();
        hint = mc.xdi;
        len = mc.xsi;
        //dr_printf("MUNMAP %p, %p\n", hint, len);

        rmunmap(pid, hint, len);

    } else if ( sysnum == __NR_mremap ) {

        pid = dr_get_process_id();
        oaddr = mc.xdi;
        osz = mc.xsi;
        nsz = mc.xdx;
        flag = mc.r10;
        naddr = mc.r8;
        //dr_printf("MREMAP %p, %p, %p, %p\n", oaddr, naddr, osz, nsz);

        addr = rmremap(pid, oaddr, osz, nsz, flag, naddr);

        if ( addr == DIRECT_PASS )
            goto out;

        flag |= MREMAP_FIXED;
        mc.r10 = flag;
        mc.r8 = addr;

        dr_set_mcontext(drcontext, &mc);

#ifdef BRK_ON
    } else if ( sysnum == __NR_brk ) {

        // Handled
        pid = dr_get_process_id();
        end = mc.xdi;
        err = rbrk(pid, end, &addr, &sz);

        //dr_printf("DEBUG BRK: %d, %p, %p, %p, %x\n", err, mc.xdi, end, addr, sz);

        if ( err == RBRK_INIT || err == RBRK_MOVE || err == RBRK_EXPAND )
        {
            // Backup Registers
#define BRK_BACKUP_ALL_REG(F, G)    \
            tlsSetReg(drcontext, G, mc.F);
            REG_BACKUP_LIST_ACCESSOR(BRK_BACKUP_ALL_REG)
#undef  BRK_BACKUP_ALL_REG

            // Change Syscall to MMAP
            mmap(addr, sz, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE|MAP_FIXED, -1, 0);

            if ( err == RBRK_MOVE )
            {
                getBrkInfo(&bi);

                //dr_printf("BRK_MOVE: %p, %p, %p, %p\n", bi.base, bi.end, addr, mtIterEnd());

                // Fix Pointer Refs
                iter = mtIterStart();
                while ( iter < mtIterEnd() )
                {
                    // -- Duo Check mem Value
                    taint = mtIterGetTaint(iter);

                    if ( !dr_safe_read(taint, sizeof(size_t), &ptr, NULL) )
                    {
                        //dr_printf("read failed: %p\n", taint);
                        //rmAddrTaint(taint);
                        // Since this taint is replaced by the last one
                        iter++;
                        continue;
                    }

                    //dr_printf("Taint Brk: %p, %p\n", taint, ptr);

                    if ( (size_t)bi.base <= ptr && (size_t)bi.end > ptr )
                    {
                        tmp = ptr - (size_t)bi.base;
                        tmp += (size_t)addr;

                        // Fix Memory Ref
                        if ( !dr_safe_write(taint, sizeof(size_t), &tmp, NULL) )
                            ERROR("Taint Write Error!");

                        // Fix Taint Tab
                        mtIterUpdateKey(iter, tmp);
                        // Update taint val for in-Brk ref
                        if ( (size_t)bi.base <= taint && (size_t)bi.end > taint )
                        {
                            tmp = taint - (size_t)bi.base;
                            tmp += (size_t)addr;

                            mtIterUpdateTaint(iter, tmp);
                        }

                    } else {

                        rmAddrTaint(taint);
                        continue;
                    }

                    iter++;
                }

                // Move Brk Data
                memcpy(addr, bi.base, bi.sz);

                // Update Brk Infomation
                obase = bi.base;
                osz = bi.sz;

                // Invoke Another Munmap
                rmunmap(pid, obase, osz);
                munmap(obase, osz);
                //dr_printf("MUNMAP BRK: %p, %p\n", obase, osz);

                // Restore Regs First
#define BRK_RESTORE_ALL_REG(F, G)   \
                tlsGetReg(drcontext, G, &mc.F);
#undef  BRK_RESTORE_ALL_REG
                tlsReset(drcontext);

                // Then, Fix Reg Values
#define BRK_FIX_REG_VALUES(F)   \
                r = mc.F;    \
                if ( (size_t)bi.base <= r && (size_t)bi.end > r )   \
                    mc.F = r - (size_t)bi.base + addr;
                REG_LIST_ACCESSOR(BRK_FIX_REG_VALUES)
#undef  BRK_FIX_REG_VALUES

                // Fix Brk Info
                end = ((size_t)end + PAGEMASK) & (~PAGEMASK);
                bi.end = end - (size_t)bi.base + addr;
                bi.base = addr;
                bi.sz = sz;
                updateBrkInfo(&bi);

                //mc.xax = bi.end;
                mc.xdi = bi.end;

                // HACK to Skip The Original syscall
                // syscall : 0F 05
                //mc.xip += 2;
                mc.xax = 0xffffffff;

                dr_set_mcontext(drcontext, &mc);

            } else if ( err == RBRK_EXPAND ) {

                //dr_syscall_set_result(drcontext, end);
                //mc.xax = end;
#define BRK_RESTORE_ALL_REG(F, G)   \
                tlsGetReg(drcontext, G, &mc.F);
#undef  BRK_RESTORE_ALL_REG
                tlsReset(drcontext);

                mc.xdi = end;

                //mc.xip += 2;
                mc.xax = 0xffffffff;

                dr_set_mcontext(drcontext, &mc);

            } else {

#define BRK_RESTORE_ALL_REG(F, G)   \
                tlsGetReg(drcontext, G, &mc.F);
#undef  BRK_RESTORE_ALL_REG

                tlsReset(drcontext);

                //dr_syscall_set_result(drcontext, addr);
                //mc.xax = addr;
                mc.xdi = addr;

                //mc.xip += 2;
                mc.xax = 0xffffffff;

                dr_set_mcontext(drcontext, &mc);

                start_trace = 1;

            }

        } else {

            // Normal Case
            
            //dr_syscall_set_result(drcontext, addr);
            //mc.xax = addr;
            mc.xdi = addr;

            //mc.xip += 2;
            mc.xax = 0xffffffff;

            dr_set_mcontext(drcontext, &mc);
        }
#endif
    }

out:
    dr_mutex_unlock(mutex);
}

#ifdef BRK_ON
static bool filterSyscall( void *drcontext, int sysnum )
{
    if ( sysnum == -1 )
        return true;
    return false;
}

static bool preSyscall( void *drcontext, int sysnum )
{
    if ( sysnum != -1 ) return true;
    //dr_printf("Sysnum : %d\n", sysnum);
    dr_mcontext_t mc = {sizeof(mc), DR_MC_ALL};
    dr_get_mcontext(drcontext, &mc);
    mc.xax = __NR_brk;
    dr_set_mcontext(drcontext, &mc);
    dr_syscall_set_result(drcontext, mc.xdi);
    return false;
}
#endif

static dr_emit_flags_t
analysisFunc(void *drcontext, void *tag, instrlist_t *bb,
             bool for_trace, bool translating,
             void *user_data)
{
    return DR_EMIT_DEFAULT;
}

static dr_emit_flags_t
insertFunc(void *drcontext, void *tag, instrlist_t *bb,
           instr_t *instr, bool for_trace, bool translating,
           void *user_data)
{
    int i;
    int src_mem = 0;
    app_pc ref;

    if ( instr_is_meta(instr) ) return DR_EMIT_DEFAULT;

    //dr_print_instr(drcontext, 1, instr, "");
    //dr_mcontext_t mc = {sizeof(mc), DR_MC_ALL};
    //dr_get_mcontext(drcontext, &mc);

    // Syscall Handling : Maybe conflict between Syscall Event & BB_Instr Event
    if ( instr_is_syscall(instr) )
    {
        dr_insert_clean_call(drcontext, bb, instr, funcSyscall, false, 0);

        return DR_EMIT_DEFAULT;
    }

#ifdef BRK_ON
    if ( !start_trace ) return DR_EMIT_DEFAULT;

#if 0
    // Check RSP for Every Instr writes to RSP reg
    // Removes The Un-needed Taints
    // This Procedure Split up into Two Iterations(Stages)
    //
    // Stage 2: Trim Taint
    if ( isTlsChkStk(drcontext) )
    {
        tlsUnChkStk(drcontext);

        dr_insert_clean_call(drcontext, bb, instr, funcChkStk, false, 0);
    }

    // Stage 1: Set Flag & Instrument to Save RSP
    //if ( instr_writes_to_reg(instr, DR_REG_RSP, DR_QUERY_INCLUDE_ALL) )
    do {
    if ( instr_writes_to_exact_reg(instr, DR_REG_RSP, DR_QUERY_INCLUDE_ALL) )
    {
        if ( instr_is_call(instr) )
            break;

        tlsChkStk(drcontext);
        /*
        instrlist_meta_preinsert( bb, instr,
                mtMovInst(drcontext, tlsRegSlotAddr(drcontext, _RSP), _RSP, 0) );
                */
        instr_t *ii = mtMovInst(drcontext, tlsRegSlotAddr(drcontext, _RSP), _RSP, 0);
        ii = XINST_CREATE_store(drcontext, opnd_create_abs_addr(tlsRegSlotAddr(drcontext, _RSP), OPSZ_PTR), opnd_create_reg(DR_REG_RSP));
        instrlist_meta_preinsert( bb, instr, ii
                 );
    }
    } while(0);
#endif

    //dr_printf("brk, %p\n", mtIterEnd());

    // Instrument Every Write-Memory Instr
    // 
    // Stage 2: Check Src & Add/Remove Taint
    if ( isTlsMemWrite(drcontext) )
    {
        tlsClrMemWrite(drcontext);

        dr_insert_clean_call(drcontext, bb, instr, funcChkTaint, false, 0);
    }

    // Stage 1: Instrument
    do {
    if ( instr_writes_memory(instr) )
    {
        if ( instr_is_call(instr) )
            break;

        opnd_t dst;

        // Assume Only One Dest Memory Address
        for ( i = 0; i < instr_num_dsts(instr); i++ )
        {
            dst = instr_get_dst(instr, i);

            if ( opnd_is_memory_reference(dst) )
                break;
        }

        // Clobber RAX
        /*
        instrlist_meta_preinsert( bb, instr,
                mtMovInst(drcontext, tlsRegSlotAddr(drcontext, _RAX), _RAX, 1) );
        instrlist_meta_preinsert( bb, instr,
                mtLeaInst(drcontext, _RAX, dst) );
        instrlist_meta_preinsert( bb, instr,
                mtMovInst(drcontext, tlsDstSlotAddr(drcontext), _RAX, 2) );
        instrlist_meta_preinsert( bb, instr,
                mtMovInst(drcontext, _RAX, tlsRegSlotAddr(drcontext, _RAX), 3) );
                */

        // Mov
        instr_t *ii = XINST_CREATE_store(drcontext, opnd_create_abs_addr(tlsRegSlotAddr(drcontext, _RAX), OPSZ_PTR), opnd_create_reg(DR_REG_RAX));
        instrlist_meta_preinsert( bb, instr, ii);

        // Lea
        if ( opnd_is_base_disp(dst) )
        {
            opnd_set_size(&dst, OPSZ_lea);
            ii = INSTR_CREATE_lea(drcontext, opnd_create_reg(DR_REG_RAX), dst);
        }
        else if (opnd_is_rel_addr(dst) || opnd_is_abs_addr(dst))
            ii = INSTR_CREATE_mov_imm(drcontext, opnd_create_reg(DR_REG_RAX), OPND_CREATE_INTPTR(opnd_get_addr(dst)));

        instrlist_meta_preinsert( bb, instr, ii );

        // Mov
        ii = XINST_CREATE_store(drcontext, opnd_create_abs_addr(tlsDstSlotAddr(drcontext), OPSZ_PTR), opnd_create_reg(DR_REG_RAX));
        instrlist_meta_preinsert( bb, instr, ii);

        // Mov
        ii = XINST_CREATE_load(drcontext, opnd_create_reg(DR_REG_RAX), opnd_create_abs_addr(tlsRegSlotAddr(drcontext, _RAX), OPSZ_PTR));
        instrlist_meta_preinsert( bb, instr, ii);

        // Set memwrite Flag
        tlsSetMemWrite(drcontext);

        /*{{{*/
        /*  Depreciated  */

        /* For Instructions have both Src & Dst as Memory Address
         * They are Ignored Here
         * e.g. add, sub, xor
         * e.g. cmpxchg
         */

        /*
        // Special Case for CMPXCHG
        if ( instr_get_opcode(instr) == OP_cmpxchg )
        {
            if ( isAddrTainted(ref) )
                // Skip Taint Now
                // Check Later at Brk_Move Stage
                return DR_EMIT_DEFAULT;
        }

        // Assume Only One Src is Interested, if there are Any
        // Duo Check Later at Brk_Move Stage
        int notaint = 1;

        for ( i = 0; i < instr_num_srcs(instr); i++ )
        {
            opnd_t opnd = instr_get_src(instr, i);

            if ( opnd_is_reg(opnd) )
            {
                reg_id_t idx = opnd_get_reg(opnd);
                reg_t reg = reg_get_value(idx, &mcontext);
                if ( bi.base <= (size_t)reg
                   && bi.end >= (size_t)reg )
                {
                    dr_printf("Add Taint: %p, %p: %p, %p\n", ref, reg, bi.base, bi.end);

                    if ( isAddrTainted(ref) )
                        rmAddrTaint(ref);

                    notaint = 0;

                    setAddrTaint(ref, reg);

                    break;
                }
            }
        }

        app_pc src;
        bool wf;
        uint index = 0;

        while ( notaint
              && instr_compute_address_ex(instr, &mcontext,
                                          index++, &src, &wf) )
        {
            // Skip Write Address
            if ( wf )   continue;

            // If Src Memory Ref is Tainted
            // Populate Taint
            if ( isAddrTainted(src) )
            {
                // Skip self Modify Instr
                // e.g. inc, dec
                if ( src == ref )
                {
                    notaint = 0;
                    break;
                }

                if ( isAddrTainted(ref) )
                    rmAddrTaint(ref);

                notaint = 0;
                dr_printf("Populate Taint: %p, %p\n", src, ref);
                dr_print_instr(drcontext, 1, instr, "");

                populateAddrTaint(src, ref);

                break;
            }
        }

        if ( notaint )
            rmAddrTaint(ref);
        */
        /*}}}*/
    }
    } while(0);
#endif

    return DR_EMIT_DEFAULT;
}

DR_EXPORT void
dr_client_main( client_id_t id, int argc, const char *argv[] )
{
#ifndef NUL_RIO
    start_trace = 0;

    drmgr_init();

    mutex = dr_mutex_create();

    initTLS();

    dr_register_exit_event(onExit);
#ifdef BRK_ON
    dr_register_filter_syscall_event(filterSyscall);
    drmgr_register_pre_syscall_event(preSyscall);
#endif
    //drmgr_register_post_syscall_event(postSyscall);

    drmgr_register_bb_app2app_event(app2appFunc, NULL);
    drmgr_register_bb_instrumentation_event( analysisFunc,
                                             insertFunc,
                                             NULL );
#endif
}

