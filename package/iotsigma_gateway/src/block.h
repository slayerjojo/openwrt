#ifndef __BLOCK_H__
#define __BLOCK_H__

#include "env.h"

#ifdef __cplusplus
extern "C"
{
#endif

void block_init(uint16_t size);
void block_clear(void);
uint32_t block_alloc(uint32_t size);
uint32_t block_size(uint32_t addr);
void block_copy(uint32_t dst, uint32_t dst_off, uint32_t src, uint32_t src_off, uint32_t size);
void block_write(uint32_t addr, uint32_t offset, const void *buffer, uint32_t size);
uint32_t block_read(uint32_t addr, uint32_t offset, void *buffer, uint32_t size);
int block_compare(uint32_t addr, uint32_t offset, const void *buffer, uint32_t size);
int block_conflict(uint32_t addr, uint32_t offset, const void *buffer, uint32_t size);
void block_free(uint32_t addr);
uint32_t block_iterator(uint32_t last);

void block_defrag(void);

#ifdef __cplusplus
}
#endif

#endif
