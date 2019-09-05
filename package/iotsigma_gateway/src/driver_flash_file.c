#include "driver_flash_file.h"
#include <stdio.h>
#include "hex.h"
#include "log.h"

#if defined(PLATFORM_LINUX)

#define FLASH_FILE_PREFIX "./"

static FILE *_fp = 0;

void flash_file_init(void)
{
    flash_file_space((const uint8_t *)"\0\0\0\0\0\0\0\0\0\0");
}

void flash_file_space(const uint8_t *space)
{
    char file[sizeof(FLASH_FILE_PREFIX) + 6 + 4 + 1] = {0};
    strcat(file, FLASH_FILE_PREFIX);
    bin2hex(file + strlen(FLASH_FILE_PREFIX), space, 10);
    if (_fp)
        fclose(_fp);
    _fp = fopen(file, "r+b");
    if (!_fp)
    {
        _fp = fopen(file, "w+b");
        if (!_fp)
        {
            LogError("open flash file (%s) failed", file);
            return;
        }
    }
}

uint32_t flash_file_bulk(void)
{
    return 2 * 1024 * 1024;
}

uint32_t flash_file_unit(void)
{
    return 4 * 1024;
}

int flash_file_read(uint32_t addr, void *buffer, uint32_t size)
{
    if (addr + size > flash_file_bulk())
        size = flash_file_bulk() - addr;

    if (!_fp)
    {
        LogError("flash file not ready");
        return 0;
    }
    fseek(_fp, 0, SEEK_END);

    int ret = size;
    uint32_t pos = ftell(_fp);
    if (addr >= pos)
    {
        memset(buffer, 0, size);
    }
    else if (addr + size >= pos)
    {
        fseek(_fp, addr, SEEK_SET);
        fread(buffer, pos - addr, 1, _fp);
        memset((uint8_t *)buffer + pos - addr, 0, addr + size - pos);
    }
    else
    {
        fseek(_fp, addr, SEEK_SET);
        ret = fread(buffer, size, 1, _fp);
    }
    return size;
}

void flash_file_write(uint32_t addr, const void *buffer, uint32_t size)
{
    if (addr + size > flash_file_bulk())
        size = flash_file_bulk() - addr;
    if (!_fp)
    {
        LogError("flash file not ready");
        return;
    }
    fseek(_fp, 0, SEEK_END);
    uint32_t pos = ftell(_fp);
    if (addr < pos)
    {
        fseek(_fp, addr, SEEK_SET);
        fwrite(buffer, size, 1, _fp);
    }
    else
    {
        while (pos < addr)
        {
            uint8_t data = 0x00;
            pos += fwrite(&data, 1, 1, _fp);
        }
        fwrite(buffer, size, 1, _fp);
    }
    fflush(_fp);
}

int flash_file_compare(uint32_t addr, const void *buffer, size_t size)
{
    if (addr + size > flash_file_bulk())
        size = flash_file_bulk() - addr;

    int ret = 0;

    if (!_fp)
    {
        LogError("flash file not ready");
        return -1;
    }
    fseek(_fp, 0, SEEK_END);

    uint32_t pos = ftell(_fp);
    size_t i = 0;
    while (i < size)
    {
        uint8_t data = 0;
        if (addr + i < pos)
        {
            fseek(_fp, addr + i, SEEK_SET);
            fread(&data, 1, 1, _fp);
        }
        if (data > *((uint8_t *)buffer + i))
        {
            ret = 1;
            break;
        }
        else if (data < *((uint8_t *)buffer + i))
        {
            ret = -1;
            break;
        }
        i++;
    }
    fflush(_fp);
    return ret;
}

int flash_file_conflict(uint32_t addr, const void *buffer, uint32_t size)
{
    if (addr + size > flash_file_bulk())
        size = flash_file_bulk() - addr;

    int ret = 0;

    if (!_fp)
    {
        LogError("flash file not ready");
        return -1;
    }
    fseek(_fp, 0, SEEK_END);

    uint32_t pos = ftell(_fp);
    size_t i = 0;
    while (i < size)
    {
        uint8_t data = 0;
        if (addr + i < pos)
        {
            fseek(_fp, addr + i, SEEK_SET);
            fread(&data, 1, 1, _fp);
        }
        if (data & (data ^ *((uint8_t *)buffer + i)))
        {
            ret = 1;
            break;
        }
        i++;
    }

    fflush(_fp);

    return ret;
}

void flash_file_erase(uint32_t addr)
{
    addr /= flash_file_unit();
    addr *= flash_file_unit();

    if (!_fp)
    {
        LogError("flash file not ready");
        return;
    }
    fseek(_fp, 0, SEEK_END);

    uint32_t pos = ftell(_fp);
    if (addr + flash_file_unit() < pos)
        pos = addr + flash_file_unit();
    fseek(_fp, addr, SEEK_SET);
    while (addr < pos)
    {
        uint8_t data = 0x00;
        addr += fwrite(&data, 1, 1, _fp);
    }
    fflush(_fp);
}

#endif
