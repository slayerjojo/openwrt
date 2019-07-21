#ifndef __DRIVER_LINUX_H__
#define __DRIVER_LINUX_H__

#include "env.h"
#include "interface_usart.h"
#include "interface_network.h"
#include "interface_os.h"

#ifdef __cplusplus
extern "C"
{
#endif

#if defined(PLATFORM_LINUX)

uint32_t linux_ticks(void);
uint32_t linux_ticks_from(uint32_t start);

#endif

#ifdef __cplusplus
}
#endif

#endif
