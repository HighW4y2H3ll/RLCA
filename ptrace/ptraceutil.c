#include "ptraceutil.h"

void ptTraceMe()
{
    ptrace(PTRACE_TRACEME, 0, 0, 0);
    //raise(SIGSTOP);
}

void ptInit( pid_t pid )
{
    ptrace(PTRACE_SETOPTIONS, pid, 0, PTRACE_O_TRACEFORK|
                                      PTRACE_O_TRACECLONE|
                                      PTRACE_O_TRACEVFORK   );
    ptrace(PTRACE_SYSCALL, pid, 0, 0);
}

int ptWait( pid_t *pid )
{
    int status;

    if ( unlikely((*pid = waitpid(-1, &status, __WALL)) == -1) )
        ERROR("PT Wait Error.");

    if ( WIFEXITED(status) )
        return ISNORMEXIT;
    if ( WIFSIGNALED(status) )
        return ISSIGTERM;
    if ( !WIFSTOPPED(status) )
        ERROR("PT Child Unknown Sig.");
    if ( WSTOPSIG(status) != SIGTRAP )
        ERROR("PT Child stopped by Signal %d.", WSTOPSIG(status));

    // This canbe fork, vfork, clone
    int flag = (status >> 16) & 0xffff;
    if ( flag == PTRACE_EVENT_CLONE
       || flag == PTRACE_EVENT_FORK
       || flag == PTRACE_EVENT_VFORK )
        return ISFORK;

    // This canbe
    // Syscall (enter/leave)
    // execve
    return ISSYSCALL;
}

unsigned long ptGetCurrPid( pid_t pid )
{
    unsigned long data;
    ptrace(PTRACE_GETEVENTMSG, pid, 0, &data);
    return data;
}

long ptSyscall( pid_t pid )
{
    return ptrace(PTRACE_SYSCALL, pid, 0, 0);
}

long ptGetReg( REGS *reg, pid_t pid )
{
    return ptrace(PTRACE_GETREGS, pid, 0, reg);
}

#if defined(__arm__)

void ptGetSyscallNum_arm( REGS *reg, REG *sc )
{
    *sc = reg->uregs[7];
}

void ptGetSP_arm( REGS *reg, REG *sp )
{
    *sp = reg->uregs[13];
}

void ptMmap_arm( pid_t pid, REGS *reg, REG *res )
{
    int flag = reg->r3;
    size_t addr = reg->r0;
    size_t len = reg->r1;

    *res = rmmap(pid, addr, len, flag);

    if ( flag & MAP_FIXED || *res == DIRECT_PASS )
        *res = DIRECT_PASS;
}

void ptFixMmap_arm( pid_t pid, REGS *reg, REG addr )
{
    reg->r3 |= MAP_FIXED;
    reg->r0 = addr;
    ptrace(PTRACE_SETREGS, pid, 0, reg);
}

void ptMunmap_arm( pid_t pid, REGS *reg )
{
    void *addr = reg->r0;
    size_t len = reg->r1;
    rmunmap(pid, addr, len);
}

void ptMremap_arm( pid_t pid, REGS *reg, REG *res )
{
    void *oaddr = reg->r0;
    size_t osz = reg->r1;
    size_t nsz = reg->r2;
    int flag = reg->r3;
    void *naddr = reg->r4;

    *res = rmremap(pid, oaddr, osz, nsz, flag, naddr);

    size_t rosz = (osz + PAGEMASK) & (~PAGEMASK);
    size_t rnsz = (nsz + PAGEMASK) & (~PAGEMASK);

    if ( rosz >= rnsz || flag & MREMAP_FIXED
       || !flag || *res == DIRECT_PASS )
    {
        *res = PT_PASS;
    }
}

void ptFixMremap_arm( pid_t pid, REGS *reg, REG addr )
{
    reg->r3 |= (MREMAP_FIXED|MREMAP_MAYMOVE);
    reg->r4 = addr;
    ptrace(PTRACE_SETREGS, pid, 0, reg);
}

#elif defined(__x86_64__)

void ptGetSyscallNum_x86_64( REGS *reg, REG *sc )
{
    *sc = reg->orig_rax;
}

void ptGetSP_x86_64( REGS *reg, REG *sp )
{
    *sp = reg->rsp;
}

void ptMmap_x86_64( pid_t pid, REGS *reg, REG *res )
{
    int flag = reg->r10;
    size_t addr = reg->rdi;
    size_t len = reg->rsi;

    *res = rmmap(pid, addr, len, flag);

    if ( flag & MAP_FIXED || *res == DIRECT_PASS )
        *res = DIRECT_PASS;
}

void ptFixMmap_x86_64( pid_t pid, REGS *reg, REG addr )
{
    reg->r10 |= MAP_FIXED;
    reg->rdi = addr;
    ptrace(PTRACE_SETREGS, pid, 0, reg);
}

void ptMunmap_x86_64( pid_t pid, REGS *reg )
{
    void *addr = reg->rdi;
    size_t len = reg->rsi;
    rmunmap(pid, addr, len);
}

void ptMremap_x86_64( pid_t pid, REGS *reg, REG *res )
{
    void *oaddr = reg->rdi;
    size_t osz = reg->rsi;
    size_t nsz = reg->rdx;
    int flag = reg->r10;
    void *naddr = reg->r8;

    *res = rmremap(pid, oaddr, osz, nsz, flag, naddr);

    size_t rosz = (osz + PAGEMASK) & (~PAGEMASK);
    size_t rnsz = (nsz + PAGEMASK) & (~PAGEMASK);

    if ( rosz >= rnsz || flag & MREMAP_FIXED
       || !flag || *res == DIRECT_PASS )
    {
        *res = PT_PASS;
    }
}

void ptFixMremap_x86_64( pid_t pid, REGS *reg, REG addr )
{
    reg->r10 |= (MREMAP_FIXED|MREMAP_MAYMOVE);
    reg->r8 = addr;
    ptrace(PTRACE_SETREGS, pid, 0, reg);
}

#elif defined(__i386__)

void ptGetSyscallNum_i386( REGS *reg, REG *sc )
{
    *sc = reg->orig_eax;
}

void ptGetSP_i386( REGS *reg, REG *sp )
{
    *sp = reg->esp;
}

void ptMmap_i386( pid_t pid, REGS *reg, REG *res )
{
    int flag = reg->esi;
    size_t addr = reg->ebx;
    size_t len = reg->ecx;

    *res = rmmap(pid, addr, len, flag);

    if ( flag & MAP_FIXED || *res == DIRECT_PASS )
        *res = DIRECT_PASS;
}

void ptFixMmap_i386( pid_t pid, REGS *reg, REG addr )
{
    reg->esi |= MAP_FIXED;
    reg->ebx = addr;
    ptrace(PTRACE_SETREGS, pid, 0, reg);
}

void ptMunmap_i386( pid_t pid, REGS *reg )
{
    void *addr = reg->ebx;
    size_t len = reg->ecx;
    rmunmap(pid, addr, len);
}

void ptMremap_i386( pid_t pid, REGS *reg, REG *res )
{
    void *oaddr = reg->ebx;
    size_t osz = reg->ecx;
    size_t nsz = reg->edx;
    int flag = reg->esi;
    void *naddr = reg->edi;

    *res = rmremap(pid, oaddr, osz, nsz, flag, naddr);

    size_t rosz = (osz + PAGEMASK) & (~PAGEMASK);
    size_t rnsz = (nsz + PAGEMASK) & (~PAGEMASK);

    if ( rosz >= rnsz || flag & MREMAP_FIXED
       || !flag || *res == DIRECT_PASS )
    {
        *res = PT_PASS;
    }
}

void ptFixMremap_i386( pid_t pid, REGS *reg, REG addr )
{
    reg->esi |= (MREMAP_FIXED|MREMAP_MAYMOVE);
    reg->edi = addr;
    ptrace(PTRACE_SETREGS, pid, 0, reg);
}

#endif



struct SC_WAITLIST *scNewList()
{
    struct SC_WAITLIST *wl = malloc(sizeof(struct SC_WAITLIST));
    wl->count = 0;
    size_t i = 0;
    for (; i < SC_MAXCACHEENTRY; i++ )
    {
        wl->ent[i].pid = (~((pid_t)0));
        LIST_INIT(&wl->ent[i].rec);
    }
    return wl;
}

void scDelList( struct SC_WAITLIST *wl )
{
    size_t i = 0;
    for (; i < wl->count; i++ )
    {
        struct SC_WAITENTRY *p = &wl->ent[i];
        struct list_head *h = p->rec.next;
        while ( h != &p->rec )
        {
            h->prev->next = h->next;
            h->next->prev = h->prev;
            free(h);
            h = p->rec.next;
        }
    }
    free(wl);

    // Free Rmmap Info
    rmexit();
}

void scEraseEntry( struct SC_WAITLIST **pwl, pid_t pid )
{
    size_t i = 0;
    struct SC_WAITLIST *wl = *pwl;

    while ( i < wl->count && pid != wl->ent[i].pid )
        i++;

    // pid not in record
    if ( i == wl->count )
        return;

    struct SC_WAITENTRY *p = &wl->ent[i];
    struct list_head *h = p->rec.next;

    // Remove the pending syscall entry
    while ( h != &p->rec )
    {
        h->prev->next = h->next;
        h->next->prev = h->prev;
        free(h);
        h = p->rec.next;
    }

    // Move the rest list forward
    for (; i < wl->count - 1; i++ )
    {
        struct SC_WAITENTRY *dst = &wl->ent[i];
        struct SC_WAITENTRY *src = &wl->ent[i + 1];
        dst->pid = src->pid;
        dst->rec.next = src->rec.next;
        dst->rec.prev = src->rec.prev;
        src->rec.next->prev = dst;
        src->rec.prev->next = dst;

        src->pid = (~((pid_t)0));
        LIST_INIT(&src->rec);
    }

    // Shrink if necessary
    if ( wl->count-- > SC_MAXCACHEENTRY )
        *pwl = realloc(wl, sizeof(size_t) + sizeof(struct SC_WAITENTRY)*wl->count);
    if ( *pwl == 0 )
        ERROR("Realloc WaitList Error.");
}

int scInsertEntry( struct SC_WAITLIST **pwl, pid_t pid, REG sc, REG sp )
{
    size_t i = 0;
    struct SC_WAITLIST *wl = *pwl;

    while ( i < wl->count && pid != wl->ent[i].pid )
        i++;

    if ( i == wl->count )
    {
        // Create a new entry
        if ( wl->count++ >= SC_MAXCACHEENTRY )
            *pwl = realloc(wl, sizeof(size_t) + sizeof(struct SC_WAITENTRY)*wl->count);
        if ( (wl = *pwl) == 0 )
            ERROR("Parameter Error Or Ralloc WaitList Error.");

        struct SC_WAITENTRY *p = &wl->ent[i];
        struct SC_RECORD *r = malloc(sizeof(struct SC_RECORD));

        p->pid = pid;
        LIST_INITWITH(&p->rec, (struct list_head*)r);
        r->sc = sc;
        r->sp = sp;

    } else {
        // Check Existing entry
        struct SC_WAITENTRY *p = &wl->ent[i];
        struct list_head *h = p->rec.next;
        while ( h != &p->rec )
        {
            if ( ((struct SC_RECORD*)h)->sc == sc
               && ((struct SC_RECORD*)h)->sp == sp )
            {
                LIST_DEL(h);
                free(h);
                return SCRETURN;
            }

            h = h->next;
        }

        // New Syscall or Thread
        struct SC_RECORD *r = malloc(sizeof(struct SC_RECORD));
        
        r->sc = sc;
        r->sp = sp;
        LIST_INS(h, (struct list_head*)r);
    }
    return SCNEW;
}


