#include "block.h"
#include "interface_flash.h"
#include "driver_flash_file.h"
#include "log.h"

#define BLOCK_UNIT (flash_unit())
#define BLOCK_BULK (flash_bulk())
#define BLOCK_SIZE (_block)
#define BLOCK_MANAGER (manager_blocks())

static uint16_t _block = 0;

void block_init(uint16_t block)
{
    _block = block;
    if (BLOCK_UNIT != (BLOCK_UNIT / BLOCK_SIZE * BLOCK_SIZE))
        LogHalt("block(%d) not assign to unit(%d)", BLOCK_SIZE, BLOCK_UNIT);
}

void block_clear(void)
{
    uint32_t unit = 0UL;
    for (unit = 0UL; unit < BLOCK_BULK / BLOCK_UNIT; unit++)
        flash_erase(unit * BLOCK_UNIT);
}

static uint32_t manager_blocks(void)
{
    uint32_t manager = BLOCK_UNIT / BLOCK_SIZE / 4UL;
    return manager / BLOCK_SIZE + !!(manager % BLOCK_SIZE);
}

static uint32_t block_swap(uint32_t unit, uint32_t free)
{
    uint32_t block = 0UL;
    uint32_t offset = 0UL;
    for (block = 0UL; block < BLOCK_UNIT / BLOCK_SIZE - BLOCK_MANAGER; block++)
    {
        uint8_t manager;
        flash_read(unit * BLOCK_UNIT + block / 4UL, &manager, 1UL);
        if ((manager & (0x01 << (2 * (block % 4)))) && 
            !(manager & (0x02 << (2 * (block % 4)))))
        {
            uint16_t i = 0;

            flash_read(free * BLOCK_UNIT + offset / 4, &manager, 1UL);
            manager |= 0x01 << (2 * (offset % 4));
            flash_write(free * BLOCK_UNIT + offset / 4, (const void *)&manager, 1UL);
            
            for (i = 0; i < BLOCK_SIZE; i++)
            {
                uint8_t data;
                flash_read(unit * BLOCK_UNIT + (BLOCK_MANAGER + block) * BLOCK_SIZE + i, &data, 1UL);
                flash_write(free * BLOCK_UNIT + (BLOCK_MANAGER + offset) * BLOCK_SIZE + i, (const void *)&data, 1UL);
            }
            offset++;
        }
    }
    flash_erase(free * BLOCK_UNIT);
    return offset;
}

uint32_t block_alloc(uint32_t size)
{
    uint8_t manager;
    uint32_t unit_free = 0xffffUL;
    uint32_t unit = 0UL;
    uint32_t block = 0UL;
    uint8_t blocks = (1UL + size) / BLOCK_SIZE + !!((1UL + size) % BLOCK_SIZE);
    for (unit = 0; unit < (BLOCK_BULK / BLOCK_UNIT); unit++)
    {
        flash_read(unit * BLOCK_UNIT, &manager, 1UL);
        if ((0 == manager) && (0xffffUL == unit_free))
        {
            unit_free = unit;
            continue;
        }
        for (block = 0; block < BLOCK_UNIT / BLOCK_SIZE - BLOCK_MANAGER - blocks; block++)
        {
            uint32_t offset = 0UL;
            for (offset = 0; offset < blocks; offset++)
            {
                flash_read(unit * BLOCK_UNIT + (block + offset) / 4, &manager, 1UL);
                if (manager & (0x01 << (2 * ((block + offset) % 4))))
                    break;
            }
            if (offset >= blocks)
            {
				uint32_t addr;
                for (offset = 0; offset < blocks; offset++)
                {
                    flash_read(unit * BLOCK_UNIT + (block + offset) / 4, &manager, 1UL);
                    manager |= 0x01 << (2 * ((block + offset) % 4));
                    flash_write(unit * BLOCK_UNIT + (block + offset) / 4, (const void *)&manager, 1UL);
                }
                addr = unit * BLOCK_UNIT + (BLOCK_MANAGER + block) * BLOCK_SIZE;
                flash_write(addr, (const void *)&blocks, 1UL);
                return addr;
            }
        }
    }
    if (0xffff == unit_free)
        return 0;
    for (unit = 0; unit < (BLOCK_BULK / BLOCK_UNIT); unit++)
    {
		uint32_t erased = 0;
        if (unit == unit_free)
            continue;
        for (block = 0; block < BLOCK_UNIT / BLOCK_SIZE - BLOCK_MANAGER; block++)
        {
            uint8_t manager;
            flash_read(unit * BLOCK_UNIT + block / 4, &manager, 1UL);
            if (!(manager & (0x01 << (2 * (block % 4)))))
                erased++;
            else if (manager & (0x02 << (2 * (block % 4))))
                erased++;
        }
        if (erased >= blocks)
        {
			uint8_t i = 0;
            uint32_t offset = block_swap(unit, unit_free);

            for (i = 0; i < blocks; i++)
            {
                uint8_t manager;
                flash_read(unit_free * BLOCK_UNIT + (offset + i) / 4, &manager, 1UL);
                manager |= 0x01 << (2 * ((offset + i) % 8));
                flash_write(unit_free * BLOCK_UNIT + (offset + i) / 4, (const void *)&manager, 1UL);
            }
            offset = unit * BLOCK_UNIT + (BLOCK_MANAGER + block) * BLOCK_SIZE;
            flash_write(offset, (const void *)&blocks, 1UL);
            return offset;
        }
    }
    return 0;
}

uint32_t block_size(uint32_t addr)
{
    uint32_t unit = addr / BLOCK_UNIT;
    uint32_t block = (addr % BLOCK_UNIT) / BLOCK_SIZE - BLOCK_MANAGER;
    uint8_t manager;
	uint8_t blocks;
    flash_read(unit * BLOCK_UNIT + block / 4, &manager, 1UL);
    if (manager & (0x02 << (2 * (block % 4))))
        return 0;
    flash_read(addr, &blocks, 1UL);
    return ((uint32_t)blocks) * BLOCK_SIZE - 1;
}

void block_copy(uint32_t dst, uint32_t dst_off, uint32_t src, uint32_t src_off, uint32_t size)
{
    uint32_t i = 0;
    for (i = 0; i < size; i++)
    {
        uint8_t data;
        block_read(src, src_off + i, &data, 1UL);
        block_write(dst, dst_off + i, &data, 1UL);
    }
}

void block_write(uint32_t addr, uint32_t offset, const void *buffer, uint32_t size)
{
    uint8_t manager;
	uint8_t blocks;
    uint32_t unit = (addr + offset) / BLOCK_UNIT;
    uint32_t block = ((addr + offset) % BLOCK_UNIT) / BLOCK_SIZE - BLOCK_MANAGER;
    if (!size)
        return;
    flash_read(unit * BLOCK_UNIT + block / 4, &manager, 1UL);
    if (manager & (0x02 << (2 * (block % 4))))
        return;
    flash_read(addr, &blocks, 1UL);
    if ((offset + size) > ((uint32_t)blocks) * BLOCK_SIZE)
        LogHalt("offset:%u size:%u blocks:%u", offset, size, blocks);
    flash_write(addr + 1 + offset, buffer, size);
}

uint32_t block_read(uint32_t addr, uint32_t offset, void *buffer, uint32_t size)
{
    uint8_t manager;
	uint8_t blocks;
    uint32_t unit = (addr + offset) / BLOCK_UNIT;
    uint32_t block = ((addr + offset) % BLOCK_UNIT) / BLOCK_SIZE - BLOCK_MANAGER;
    if (!size)
        return 0;
    flash_read(unit * BLOCK_UNIT + block / 4, &manager, 1UL);
    if (manager & (0x02 << (2 * (block % 4))))
        return 0;
    flash_read(addr, &blocks, 1UL);
    if (offset + size > (uint32_t)blocks * BLOCK_SIZE)
    {
        LogError("overflow");
        if (offset > (uint32_t)blocks * BLOCK_SIZE)
            size = 0;
        else
            size = (uint32_t)blocks * BLOCK_SIZE - offset;
    }
    if (!size)
        return 0;
    return flash_read(addr + 1 + offset, buffer, size);
}

int block_compare(uint32_t addr, uint32_t offset, const void *buffer, uint32_t size)
{
    if (!size)
        return 0;
    return flash_compare(addr + 1 + offset, buffer, size);
}

int block_conflict(uint32_t addr, uint32_t offset, const void *buffer, uint32_t size)
{
    return flash_conflict(addr + 1 + offset, buffer, size);
}

void block_free(uint32_t addr)
{
	uint8_t blocks = 0;
	uint32_t offset = 0;

    if ((!addr) || (addr % BLOCK_SIZE))
        return;

    flash_read(addr, &blocks, 1UL);

    for (offset = 0; offset < blocks; offset++)
    {
        uint32_t unit = (addr + offset * BLOCK_SIZE) / BLOCK_UNIT;
        uint32_t block = ((addr + offset * BLOCK_SIZE) % BLOCK_UNIT) / BLOCK_SIZE - BLOCK_MANAGER;

        uint8_t manager;
        flash_read(unit * BLOCK_UNIT + block / 4, &manager, 1UL);
        manager |= 0x02 << (2 * (block % 4));
        flash_write(unit * BLOCK_UNIT + block / 4, (const void *)&manager, 1UL);
    }
}

uint32_t block_iterator(uint32_t last)
{
    uint32_t addr = last;
    if (last)
    {
        uint8_t blocks;
        flash_read(addr, &blocks, 1UL);
        addr += (uint32_t)blocks * BLOCK_SIZE;
    }

    while (addr < BLOCK_BULK)
    {
        uint8_t manager;
        if (addr % BLOCK_UNIT)
        {
            uint32_t blocks = 0;
            uint32_t unit = addr / BLOCK_UNIT;
            uint32_t block = (addr % BLOCK_UNIT) / BLOCK_SIZE - BLOCK_MANAGER;

            flash_read(unit * BLOCK_UNIT + block / 4, &manager, 1UL);
            if (!(manager & (0x03 << (2 * (block % 4)))))
            {
                addr += BLOCK_UNIT;
                addr /= BLOCK_UNIT;
                addr *= BLOCK_UNIT;
                continue;
            }
            if (!(manager & (0x02 << (2 * (block % 4)))))
                return addr;
            flash_read(addr, &blocks, 1UL);
            addr += blocks * BLOCK_SIZE;
        }
        else
        {
            flash_read(addr, &manager, 1UL);
            if (0 == manager)
                addr += BLOCK_UNIT;
            else
                addr += BLOCK_MANAGER * BLOCK_SIZE;
        }
    }
    return 0;
}

void block_defrag(void)
{
    uint8_t manager;
    uint32_t unit;
    uint32_t unit_free = 0xffffUL;
    
    for (unit = 0; unit < (BLOCK_BULK / BLOCK_UNIT); unit++)
    {
        flash_read(unit * BLOCK_UNIT, &manager, 1UL);
        if ((!manager) && (0xffff == unit_free))
        {
            unit_free = unit;
            break;
        }
    }
    if (0xffff == unit_free)
        return;
    for (unit = 0; unit < (BLOCK_BULK / BLOCK_UNIT); unit++)
    {
        uint32_t block;

        if (unit == unit_free)
            continue;
        flash_read(unit * BLOCK_UNIT, &manager, 1UL);
        if (!manager)
            continue;
        for (block = 0; block < BLOCK_UNIT / BLOCK_SIZE - BLOCK_MANAGER; block += 4)
        {
            uint8_t i = 0;

            flash_read(unit * BLOCK_UNIT + block / 4, &manager, 1UL);
            for (i = 0; i < 4; i++)
            {
                if (manager & (0x02 << (i * 2)))
                {
                    break;
                }
                else if (!(manager & (0x01 << (i * 2))))
                {
                    block = BLOCK_UNIT / BLOCK_SIZE - BLOCK_MANAGER;
                    break;
                }
            }
            if (i < 4)
                break;
        }
        if (block >= BLOCK_UNIT / BLOCK_SIZE - BLOCK_MANAGER)
            continue;

        block_swap(unit, unit_free);
        unit_free = unit;
    }
}
