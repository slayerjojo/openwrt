#include "driver_flash_file.h"
#include <stdio.h>
#include "log.h"

#if defined(PLATFORM_OPENWRT)

static FILE *_fp = 0;
static char _file[255] = {0};
static int _size = 0;

void flash_file_init(const char *path)
{
    if (path)
    {
        strcpy(_file, path);
        _size = strlen(path);
    }
    flash_file_space(0);
}

void flash_file_space(const HalFlashSpace *space)
{
    const uint8_t *dir = "\0\0\0\0\0\0";
    const uint8_t *file = "\0\0\0\0";
    if (space)
    {
        if (space->dir)
            dir = space->dir;
        if (space->file)
            file = space->file;
    }
    sprintf(_file + _size, "%02x%02x%02x%02x%02x%02x.%02x%02x%02x%02x", 
        dir[0],
        dir[1],
        dir[2],
        dir[3],
        dir[4],
        dir[5],
        file[0],
        file[1],
        file[2],
        file[3]);
    if (_fp)
        fclose(_fp);
    _fp = fopen(_file, "r+b");
    if (!_fp)
    {
        _fp = fopen(_file, "w+b");
        if (!_fp)
        {
            LogError("open flash file (%s) failed", _file);
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
        LogError("open flash file %s failed", _file);
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
        memset(buffer + pos - addr, 0, addr + size - pos);
    }
    else
    {
        fseek(_fp, addr, SEEK_SET);
        ret = fread(buffer, size, 1, _fp);
    }
    fflush(_fp);
    return size;
}

void flash_file_write(uint32_t addr, const void *buffer, uint32_t size)
{
    if (addr + size > flash_file_bulk())
        size = flash_file_bulk() - addr;
    if (!_fp)
    {
        LogError("open flash file %s failed", _file);
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

int flash_file_compare(uint32_t addr, const void *buffer, uint32_t size)
{
    if (addr + size > flash_file_bulk())
        size = flash_file_bulk() - addr;

    int ret = 0;

    if (!_fp)
    {
        LogError("open flash file %s failed", _file);
        return -1;
    }
    fseek(_fp, 0, SEEK_END);

    uint32_t pos = ftell(_fp);
    int i = 0;
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
        LogError("open flash file %s failed", _file);
        return -1;
    }
    fseek(_fp, 0, SEEK_END);

    uint32_t pos = ftell(_fp);
    int i = 0;
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
    addr /= flash_file_bulk();
    addr *= flash_file_bulk();

    if (!_fp)
    {
        LogError("open flash file %s failed", _file);
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
