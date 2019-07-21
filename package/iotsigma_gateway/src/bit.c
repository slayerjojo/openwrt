#include "bit.h"

uint8_t byte_bit_count(uint8_t v)
{
    v = (v & 0x55) + ((v >> 1) & 0x55);
    v = (v & 0x33) + ((v >> 2) & 0x33);
    v = (v & 0x0f) + ((v >> 4) & 0x0f);
    return v;
}
