#ifndef __DRIVER_IO_LINUX_H__
#define __DRIVER_IO_LINUX_H__

#include "env.h"

#ifdef __cplusplus
extern "C"
{
#endif

#if defined(PLATFORM_OPENWRT)

#define IO_LED_APP 0
#define IO_LED_ZIGBEE 1
#define IO_LED_WIFI 2

void linux_io_init(void);

uint8_t linux_io_direction_get(uint8_t io);
void linux_io_direction_set(uint8_t io, uint8_t v);

uint8_t linux_io_get(uint8_t io);
void linux_io_set(uint8_t io, uint8_t v);

#endif

#ifdef __cplusplus
}
#endif

#endif
