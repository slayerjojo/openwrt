#include "endian.h"

typedef union _endian_64
{
    struct
    {
        uint32_t first;
        uint32_t second;
    }order;
    uint64_t v;
}Endian64;

typedef union _endian_32
{
    struct
    {
        uint16_t first;
        uint16_t second;
    }order;
    uint32_t v;
}Endian32;

typedef union _endian_16
{
    struct
    {
        uint8_t first;
        uint8_t second;
    }order;
    uint16_t v;
}Endian16;

static uint8_t big_endian(void)
{
    uint16_t i = 0x1234;
    uint8_t *c = (uint8_t *)(&i);
    return (c[0] == 0x12);
}

uint16_t endian16(uint16_t v)
{
	Endian16 ret;
    uint8_t i;

    if (big_endian())
        return v;

    ret.v = v;
    i = ret.order.first;
    ret.order.first = ret.order.second;
    ret.order.second = i;
    return ret.v;
}

uint32_t endian32(uint32_t v)
{
	Endian32 ret;
    uint16_t i;

    if (big_endian())
        return v;

    ret.v = v;
    i = endian16(ret.order.first);
    ret.order.first = endian16(ret.order.second);
    ret.order.second = i;
    return ret.v;
}

uint64_t endian64(uint64_t v)
{
	Endian64 ret;
    uint32_t i;

    if (big_endian())
        return v;

    ret.v = v;
    i = endian32(ret.order.first);
    ret.order.first = endian32(ret.order.second);
    ret.order.second = i;
    return ret.v;
}
