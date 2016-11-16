#include "error.h"
#include <unistd.h>
#include <fcntl.h>

inline size_t randNumMod( size_t max )
{
#ifdef RAND_OFF
    return 0;
#else
    int fd = open("/dev/urandom", O_RDONLY);
    size_t rand;
    if ( read(fd, &rand, sizeof(rand)) == -1 )
        ERROR("RAND ERROR");
    close(fd);
    return rand%max;
#endif
}
