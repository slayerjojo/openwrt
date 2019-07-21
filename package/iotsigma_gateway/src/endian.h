#ifndef __ENDIAN_H__
#define __ENDIAN_H__

#include "env.h"

#ifdef __cplusplus
extern "C"
{
#endif

uint16_t endian16(uint16_t v);
uint32_t endian32(uint32_t v);
uint64_t endian64(uint64_t v);

#ifdef __cplusplus
}
#endif

#endif
