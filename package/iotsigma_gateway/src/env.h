#ifndef __ENV_H__
#define __ENV_H__

#ifdef __cplusplus
extern "C"
{
#endif

#define PLATFORM_OPENWRT

#ifdef PLATFORM_OPENWRT
#define PLATFORM_LINUX
#endif

#ifdef PLATFORM_LINUX

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "log.h"
#include "net.h"
#include "netio_epoll.h"
#include "console.h"

#endif

#define STRUCT_REFERENCE(type, inst, struction) ((type *)(((char *)(inst)) - (uint64_t)(&((type *)0)->struction)))

#define UNUSED(x) (void)(x)

#ifdef __cplusplus
}
#endif

#endif
