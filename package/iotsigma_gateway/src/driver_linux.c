#include "driver_linux.h"

#if defined(PLATFORM_LINUX)

#include <sys/time.h>

uint32_t linux_ticks(void)
{
    struct timeval tv;
    gettimeofday(&tv, 0);
    return (uint64_t)tv.tv_sec * 1000 + (uint64_t)tv.tv_usec / 1000;
}

uint32_t linux_ticks_from(uint32_t start)
{
	uint32_t now;

    if (!start)
        return 0;
    
    now = os_ticks();
    if (now >= start)
        return now - start;
    return 0xffffffffL - start + now;
}

#endif
