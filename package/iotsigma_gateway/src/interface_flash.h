#ifndef __INTERFACE_FLASH_H__
#define __INTERFACE_FLASH_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "env.h"

#if defined(PLATFORM_LINUX)

#include "driver_flash_file.h"

#define flash_init flash_file_init
#define flash_space flash_file_space

#define flash_bulk() ((uint32_t)(2ul * 1024ul * 1024ul / 8ul))
#define flash_unit() (256ul)
#define flash_compare flash_file_compare
#define flash_conflict flash_file_conflict
#define flash_read flash_file_read
#define flash_write flash_file_write
#define flash_erase flash_file_erase

#endif

#ifdef __cplusplus
}
#endif

#endif
