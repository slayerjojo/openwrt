#include "hex.h"

char *bin2hex(char *hex, const uint8_t *bin, uint16_t size)
{
    uint16_t i;
    for (i = 0; i < size; i++)
    {
        if (bin[i] / 16 >= 10)
            hex[i * 2 + 0] = 'a' + bin[i] / 16 - 10;
        else
            hex[i * 2 + 0] = '0' + bin[i] / 16;
        
        if (bin[i] % 16 >= 10)
            hex[i * 2 + 1] = 'a' + bin[i] % 16 - 10;
        else
            hex[i * 2 + 1] = '0' + bin[i] % 16;
    }
    hex[i * 2] = 0;
    return hex;
}

uint8_t *hex2bin(uint8_t *bin, const char *hex, uint16_t size)
{
    uint16_t i;
    for (i = 0; i < size; i += 2)
    {
        bin[i / 2] = 0;

        if ('a' <= hex[i] && hex[i] <= 'f')
            bin[i / 2] |= (hex[i] - 'a' + 10) << 4;
        else if ('A' <= hex[i] && hex[i] <= 'F')
            bin[i / 2] |= (hex[i] - 'A' + 10) << 4;
        else if ('0' <= hex[i] && hex[i] <= '9')
            bin[i / 2] |= (hex[i] - '0') << 4;
        
        if ('a' <= hex[i + 1] && hex[i + 1] <= 'f')
            bin[i / 2] |= hex[i + 1] - 'a' + 10;
        else if ('A' <= hex[i + 1] && hex[i + 1] <= 'F')
            bin[i / 2] |= hex[i + 1] - 'A' + 10;
        else if ('0' <= hex[i + 1] && hex[i + 1] <= '9')
            bin[i / 2] |= hex[i + 1] - '0';
    }
    return bin;
}
