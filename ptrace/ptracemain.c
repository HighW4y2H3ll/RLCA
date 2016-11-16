#include "ptraceutil.h"
#include "error.h"
#include <sys/syscall.h>

int main( int argc, char **argv )
{
    pid_t pid;
    REGS reg;
    struct SC_WAITLIST *waitlist = scNewList();

    if ( (pid = fork()) == 0 )
    {
        ptTraceMe();
        printf("In child\n");
        char *env[] = {"LD_PRELOAD=../malloc/phk.so", NULL};
        execle("./t2", "t2", NULL, env);
    }
    else
    {
        // Simply waitpid here
        // it is execve entry on wakeup
        pid_t curr_pid;
        ptWait(&curr_pid);
        ptInit(pid);
        while(1)
        {
            int perr = ptWait(&curr_pid);
            if ( perr == ISFORK )
            {
                pid_t fpid = ptGetCurrPid(curr_pid);
                ptInit(fpid);
                continue;
            } else if ( perr == ISNORMEXIT ) {
                if ( curr_pid != pid )
                {
                    scEraseEntry(&waitlist, curr_pid);
                    continue;
                }
                else
                    EXIT("PT EXIT.");
            } else if ( perr == ISSIGTERM ) {
                if ( curr_pid != pid )
                {
                    scEraseEntry(&waitlist, curr_pid);
                    continue;
                }
                else
                    EXIT("PT SIGNALED.");
            }

            if ( ptGetReg(&reg, curr_pid) == -1 )
                ERROR("PTrace Get Register Failed");

            REG sc, sp;

            ptGetSyscallNum(&reg, &sc);
            ptGetSP(&reg, &sp);

            // I Choose to Neglect __NR_brk
            // & Stack Overflow
            // Just crash if you like
            if ( sc == __NR_mmap 
#if defined(__NR_mmap2)
                || sc == __NR_mmap2 )
#else
                )
#endif
            {
                // Mmap & Mmap2
                int sc_err = scInsertEntry( &waitlist,
                        curr_pid,
                        sc,
                        sp   );

                if ( sc_err == SCNEW )
                {
                    // Enter Syscall
                    //printf("MMAP %d:%llx\n", curr_pid, reg.rsi );

                    REG addr;
                    ptMmap(curr_pid, &reg, &addr);
                    if ( addr != PT_PASS )
                    {
                        ptFixMmap(curr_pid, &reg, addr);
                    }
                } else {
                    // Leave Syscall
                    // TODO Logging Syscall Exit?
                    //printf("Ret :%llx\n", reg.rdi );
                }

            } else if ( sc == __NR_munmap ) {

                // Munmap
                int sc_err = scInsertEntry( &waitlist,
                        curr_pid,
                        sc,
                        sp   );

                if ( sc_err == SCNEW )
                {
                    // Enter Syscall
                    ptMunmap(curr_pid, &reg);
                    //printf("UNMAP %d:%llx\n", curr_pid, reg.rdi);
                } else {
                    // Leave Syscall
                    // TODO Logging Syscall Exit?
                }

            } else if ( sc == __NR_mremap ) {

                // Mremap
                int sc_err = scInsertEntry( &waitlist,
                        curr_pid,
                        sc,
                        sp   );

                if ( sc_err == SCNEW )
                {
                    // Enter Syscall
                    REG addr;
                    ptMremap(curr_pid, &reg, &addr);
                    if ( addr != PT_PASS )
                    {
                        ptFixMremap(curr_pid, &reg, addr);
                    }
                } else {
                    // Leave Syscall
                    // TODO Logging Syscall Exit?
                }

            }

            ptSyscall(curr_pid);
        }
    }

    scDelList(waitlist);
}
