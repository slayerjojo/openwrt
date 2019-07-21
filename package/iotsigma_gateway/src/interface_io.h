#ifndef __INTERFACE_IO_H__
#define __INTERFACE_IO_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "env.h"

#if defined(PLATFORM_LINUX)

#define io_init linux_io_init

#define io_direction_get linux_io_direction_get
#define io_direction_set linux_io_direction_set
#define io_get linux_io_get
#define io_set linux_io_set

#endif

#ifdef __cplusplus
}
#endif

#endif
