#ifndef __ENV_H__
#define __ENV_H__

#define PLATFORM_OPENWRT

#ifdef PLATFORM_OPENWRT
#define PLATFORM_LINUX
#endif

#ifdef PLATFORM_OPENWRT

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "net.h"
#include "netio_epoll.h"
#include "console.h"

#endif

#define STRUCT_REFERENCE(type, inst, struction) ((type *)(((char *)(inst)) - (uint32_t)(&((type *)0)->struction)))

#endif
