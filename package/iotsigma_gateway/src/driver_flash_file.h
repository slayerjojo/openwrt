#ifndef __DRIVER_FLASH_FILE_H__
#define __DRIVER_FLASH_FILE_H__

#include "env.h"
#include "interface_flash.h"

#ifdef __cplusplus
extern "C"
{
#endif

#if defined(PLATFORM_OPENWRT)

void flash_file_init(const char *space);
void flash_file_space(const HalFlashSpace *space);

uint32_t flash_file_bulk(void);
uint32_t flash_file_unit(void);

int flash_file_read(uint32_t addr, void *buffer, uint32_t size);
void flash_file_write(uint32_t addr, const void *buffer, uint32_t size);
int flash_file_compare(uint32_t addr, const void *buffer, uint32_t size);
int flash_file_conflict(uint32_t addr, const void *buffer, uint32_t size);
void flash_file_erase(uint32_t addr);

#endif

#ifdef __cplusplus
}
#endif

#endif
