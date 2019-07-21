#ifndef __BUDDHA_HEAP_H__
#define __BUDDHA_HEAP_H__

#include "env.h"
#include "heap_mark.h"

#ifdef __cplusplus
extern "C"
{
#endif

void buddha_heap_init(uint8_t *heap, size_t size);
void *buddha_heap_alloc(uint32_t size, uint8_t mark);
void *buddha_heap_talloc(uint32_t size, uint8_t mark);
void *buddha_heap_malloc(size_t size);
void *buddha_heap_reuse(void *addr);
void *buddha_heap_realloc(void *addr, uint32_t size);
void buddha_heap_mark_set(void *addr, uint8_t mark);
void *buddha_heap_flesh(void *addr);
void buddha_heap_free(void *addr);
void buddha_heap_destory(void *addr);
void *buddha_heap_find(void *addr, uint8_t mark);
void buddha_heap_check(void);

#ifdef __cplusplus
}
#endif

#endif
