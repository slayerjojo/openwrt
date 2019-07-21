#ifndef __INTERFACE_OS_H__
#define __INTERFACE_OS_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "env.h"

#if defined(PLATFORM_LINUX)

#define os_init()
#define os_malloc malloc
#define os_free free
#define os_realloc realloc
#define os_memset memset
#define os_memcpy memcpy
#define os_memcmp memcmp
#define os_strlen strlen
#define os_strcpy strcpy
#define os_strcmp strcmp
#define os_ticks linux_ticks
#define os_sleep usleep
#define os_rand rand

#define os_ticks_ms(ms) (ms)
#define os_ticks_from linux_ticks_from

#endif

#ifdef __cplusplus
}
#endif

#endif
