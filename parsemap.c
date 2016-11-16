
#include <fcntl.h>
#include <stdlib.h>
#include "parsemap.h"
#include "mmap.h"


void setMinAddr()
{
    // Init MMAP_MINADDR Only Once
    if ( MMAP_MINADDR ) return;

    char buf[128] = {0};

    int fd = open("/proc/sys/vm/mmap_min_addr", O_RDONLY);
    if ( fd == -1 ) ERROR("/proc/sys/vm/mmap_min_addr file Open Failed.");
    int err = read(fd, buf, 128);
    if ( err == -1 ) ERROR("/proc/sys/vm/mmap_min_addr file Read Failed.");
    sscanf(buf, "%ud", &MMAP_MINADDR);
    close(fd);
}

/*
 * Faster Than USEing dr_query_memory_ex()
 */
void parseMaps( pid_t pid )
{
    char buf[1025] = {0};
    sprintf(buf, "/proc/%d/maps", pid );

    int fd = open(buf, O_RDONLY);
    if ( fd == -1 ) ERROR("Maps file Open Failed.");

    int err = read(fd, buf, 1024);
    size_t sz = 0;

    size_t prev_addr = MMAP_MINADDR;

    while(err)
    {
        if ( err == -1 )    ERROR("Maps file Read Error.");

        sz += err;
        buf[sz] = 0;

        size_t start, end;
        char *prev_off, *off;
        prev_off = off = buf;

        while( off = strchr(off + 1, '\n') )
        {
            sscanf(prev_off, "%p-%p ", &start, &end);
            size_t gap = start - prev_addr;

            if ( likely( start < MMAP_MAXADDR )
                    && gap != 0 )
                insertEntry( prev_addr, start );

            // Most likely is gap == 0
            else if ( unlikely( start >= MMAP_MAXADDR ) )
            {
                insertEntry( prev_addr, MMAP_MAXADDR );
                goto end_of_parse;
            }

            prev_addr = end;
            prev_off = off;
        }

        if ( unlikely( sz < 1024 ) )    break;

        sprintf( buf, "%s", prev_off + 1 );
        sz = sz - (size_t)prev_off - 1 + (size_t)buf;
        err = read(fd, buf + sz, 1024 - sz);
    }

end_of_parse:
    close(fd);
}
