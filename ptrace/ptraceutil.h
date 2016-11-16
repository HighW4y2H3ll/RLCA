#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/wait.h>
#include "error.h"
#include "mmap.h"

#ifndef _PTRACEUTIL
#define _PTRACEUTIL

#define ISFORK  0
#define ISSYSCALL   1
#define ISNORMEXIT  2
#define ISSIGTERM   3

#if defined(__arm__)

#define REGS struct user_regs
#define REG unsigned long
#define ptGetSyscallNum ptGetSyscallNum_arm
#define ptGetSP ptGetSP_arm
#define ptMmap  ptMmap_arm
#define ptFixMmap   ptFixMmap_arm
#define ptMunmap    ptMunmap_arm
#define ptMremap    ptMremap_arm
#define ptFixMremap    ptFixMremap_arm

#elif defined(__x86_64__)

#define REGS struct user_regs_struct
#define REG unsigned long long
#define ptGetSyscallNum ptGetSyscallNum_x86_64
#define ptGetSP ptGetSP_x86_64
#define ptMmap  ptMmap_x86_64
#define ptFixMmap   ptFixMmap_x86_64
#define ptMunmap    ptMunmap_x86_64
#define ptMremap    ptMremap_x86_64
#define ptFixMremap    ptFixMremap_x86_64

#elif defined(__i386__)

#define REGS struct user_regs_struct
#define REG unsigned long
#define ptGetSyscallNum ptGetSyscallNum_i386
#define ptGetSP ptGetSP_i386
#define ptMmap  ptMmap_i386
#define ptFixMmap   ptFixMmap_i386
#define ptMunmap    ptMunmap_i386
#define ptMremap    ptMremap_i386
#define ptFixMremap    ptFixMremap_i386

#endif

#define SC_MAXCACHEENTRY    8
#define SCRETURN    0
#define SCNEW   1

#define PT_PASS     -1

#define LIST_INIT(ent)  {   \
    (ent)->prev = (ent)->next = (ent);  \
}

#define LIST_INITWITH(ent, node)    {   \
    (ent)->prev = (ent)->next = (node); \
    (node)->prev = (node)->next = (ent);    \
}

#define LIST_DEL(node)  {   \
    (node)->prev->next = (node)->next;  \
    (node)->next->prev = (node)->prev;  \
}

#define LIST_INS(ent, node) {   \
    (node)->next = (ent)->next; \
    (node)->prev = (ent);   \
    (ent)->next->prev = (node); \
    (ent)->next = (node);   \
}

struct list_head {
    struct list_head *next, *prev;
};

struct SC_RECORD {
    struct list_head list;
    REG sc;     // Syscall Number
    REG sp;     // Stack Pointer : ID for thread
};

struct SC_WAITENTRY {
    pid_t pid;
    struct list_head rec;
};

struct SC_WAITLIST {
    size_t count;
    struct SC_WAITENTRY ent[SC_MAXCACHEENTRY];
};


void ptTraceMe();
void ptInit( pid_t );
int ptWait( pid_t * );
unsigned long ptGetCurrPid( pid_t );
long ptSyscall( pid_t );
long ptGetReg( REGS *, pid_t );

struct SC_WAITLIST *scNewList();
void scDelList( struct SC_WAITLIST * );
void scEraseEntry( struct SC_WAITLIST **, pid_t );
int scInsertEntry( struct SC_WAITLIST **, pid_t, REG, REG );

// arm
void ptGetSyscallNum_arm( REGS *, REG * );
void ptGetSP_arm( REGS *, REG * );
void ptMmap_arm( pid_t, REGS *, REG * );
void ptFixMmap_arm( pid_t, REGS *, REG );
void ptMunmap_arm( pid_t, REGS * );
void ptMremap_arm( pid_t, REGS *, REG * );
void ptFixMremap_arm( pid_t, REGS *, REG );

// x86_64
void ptGetSyscallNum_x86_64( REGS *, REG * );
void ptGetSP_x86_64( REGS *, REG * );
void ptMmap_x86_64( pid_t, REGS *, REG * );
void ptFixMmap_x86_64( pid_t, REGS *, REG );
void ptMunmap_x86_64( pid_t, REGS * );
void ptMremap_x86_64( pid_t, REGS *, REG * );
void ptFixMremap_x86_64( pid_t, REGS *, REG );

// i386
void ptGetSyscallNum_i386( REGS *, REG * );
void ptGetSP_i386( REGS *, REG * );
void ptMmap_i386( pid_t, REGS *, REG * );
void ptFixMmap_i386( pid_t, REGS *, REG );
void ptMunmap_i386( pid_t, REGS * );
void ptMremap_i386( pid_t, REGS *, REG * );
void ptFixMremap_i386( pid_t, REGS *, REG );


#endif
