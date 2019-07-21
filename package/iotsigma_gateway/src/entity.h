#ifndef __ENTITY_H__
#define __ENTITY_H__

#include "env.h"
#include "cluster.h"
#include "entity_type.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define PROPERTY_TYPE_BIT 0
#define PROPERTY_TYPE_INT8 1
#define PROPERTY_TYPE_INT16 2
#define PROPERTY_TYPE_INT32 3
#define PROPERTY_TYPE_INT48 4
#define PROPERTY_TYPE_INT64 5
#define PROPERTY_TYPE_BUFFER 6
#define PROPERTY_TYPE_STRING 7

#define PROPERTY_ENTITY_TYPE 0
#define PROPERTY_ENTITY_ID 1
#define PROPERTY_ENTITY_DIRTY 2

uint32_t entity_create(uint8_t type, uint32_t size);
uint32_t entity_init(uint16_t entity, uint8_t type, uint32_t size);
#define entity_release(addr) block_free(addr)
uint32_t entity_find(uint8_t type, uint32_t last);
uint32_t entity_get(uint16_t entity);
#define entity_iterator(last) block_iterator(last)
uint32_t entity_size(uint32_t addr);
uint32_t entity_pack(uint32_t addr, uint8_t *data, uint32_t size);
int entity_unpack(uint32_t *addr, uint8_t *data, uint32_t size);
uint32_t property_iterator(uint32_t addr, uint32_t offset, uint8_t *property, uint8_t *type);
uint8_t property_type(uint32_t addr, uint8_t property);
uint32_t property_size(uint32_t addr, uint8_t property);
int property_set(uint32_t *addr, uint8_t property, uint8_t type, const void *value, uint32_t size);
int property_release(uint32_t *addr, uint8_t property);
int property_get(uint32_t addr, uint8_t property, uint16_t offset, void *value, uint32_t size);
bool property_exist(uint32_t addr, uint8_t property);
int property_compare(uint32_t addr, uint8_t property, uint16_t offset, const void *value, uint32_t size);
int property_bit_get(uint32_t addr, uint8_t property);
int property_bit_set(uint32_t *addr, uint8_t property);
int property_bit_clear(uint32_t *addr, uint8_t property);

#ifdef __cplusplus
}
#endif

#endif
