#include "entity.h"
#include "block.h"
#include "endian.h"
#include "log.h"
#include "bit.h"

static uint8_t MAX_PROPERTY_SIZE[] = {1, 1, 2, 4, 6, 8, 2, 2};

static uint16_t property_skip(uint32_t addr, uint32_t offset, uint8_t type)
{
    if (type <= PROPERTY_TYPE_INT64)
        return 1 + 1 + MAX_PROPERTY_SIZE[type];
    else if (PROPERTY_TYPE_BUFFER == type ||
        PROPERTY_TYPE_STRING == type)
    {
        uint16_t size;
        block_read(addr, offset + 2, &size, 2ul);
        return 1 + 1 + 2 + size;
    }
    else
    {
        LogHalt("unknown property type %u", type);
    }
    return 0;
}

uint32_t entity_create(uint8_t type, uint32_t size)
{
    uint16_t entity = 0;
    uint32_t addr = 0;
    while ((addr = entity_iterator(addr)))
    {
        if (property_compare(addr, PROPERTY_ENTITY_ID, 0, &entity, 2ul) > 0)
            property_get(addr, PROPERTY_ENTITY_ID, 0, &entity, 2ul);
    }
    entity++;
    LogAction("type:%u entity:%u", type, entity);

    return entity_init(entity, type, size);
}

uint32_t entity_init(uint16_t entity, uint8_t type, uint32_t size)
{
    uint32_t addr = 0;
    size += 1 + 2;//type + entity
    addr = block_alloc(size);
    if (!addr)
        return 0;

    block_write(addr, 0, &type, 1ul);
    block_write(addr, 1, &entity, 2ul);
    return addr;
}

uint32_t entity_find(uint8_t type, uint32_t last)
{
    while ((last = entity_iterator(last)) && block_compare(last, 0, &type, 1ul));
    return last;
}

uint32_t entity_get(uint16_t entity)
{
    uint32_t addr = 0;
    while ((addr = entity_iterator(addr)) && block_compare(addr, 1, &entity, 2ul));
    return addr;
}

uint32_t entity_size(uint32_t addr)
{
    uint32_t size = block_size(addr);
    uint32_t pos = 3;
    while (pos < size)
    {
        uint8_t property;
        uint8_t type;
        block_read(addr, pos + 0, &property, 1ul);
        if (!property)
            break;
        block_read(addr, pos + 1, &type, 1ul);

        pos += property_skip(addr, pos, type);
    }
    return pos;
}

uint32_t entity_pack(uint32_t addr, uint8_t *data, uint32_t size)
{
	uint32_t offset = 3;
    uint32_t length = block_size(addr);
    if (length > size)
        length = size;
    block_read(addr, 0, data, 3ul);
    *(uint16_t *)(data + 1) = endian16(*(uint16_t *)(data + 1));
    while (offset < length)
    {
        block_read(addr, offset, data + offset, 1ul);
        if (!data[offset])
            break;
        block_read(addr, offset + 1, data + offset + 1, 1ul);
        block_read(addr, offset + 2, data + offset + 2, (uint32_t)MAX_PROPERTY_SIZE[data[offset + 1]]);
        if (PROPERTY_TYPE_BIT == data[offset + 1])
            data[offset + 2] = byte_bit_count(data[offset + 2]) % 2;
        else if (PROPERTY_TYPE_INT16 == data[offset + 1])
            *(uint16_t *)(data + offset + 2) = endian16(*(uint16_t *)(data + offset + 2));
        else if (PROPERTY_TYPE_INT32 == data[offset + 1])
            *(uint32_t *)(data + offset + 2) = endian32(*(uint32_t *)(data + offset + 2));
        else if (PROPERTY_TYPE_INT64 == data[offset + 1])
            *(uint64_t *)(data + offset + 2) = endian64(*(uint64_t *)(data + offset + 2));
        else if (PROPERTY_TYPE_BUFFER == data[offset + 1] || PROPERTY_TYPE_STRING == data[offset + 1])
        {
            block_read(addr, offset + 2 + 2, data + offset + 2 + 2, *(uint16_t *)(data + offset + 2));
            *(uint16_t *)(data + offset + 2) = endian16(*(uint16_t *)(data + offset + 2));
        }
        else
        {
            LogHalt("unknown property type %d", data[offset + 1]);
            break;
        }
        offset += property_skip(addr, offset, data[offset + 1]);
    }
    return offset;
}

int entity_unpack(uint32_t *addr, uint8_t *data, uint32_t size)
{
    int ret = 0;
    uint32_t offset = 0;
    while (offset < size)
    {
        if (!data[offset])
            break;
        if (PROPERTY_TYPE_BIT == data[offset + 1])
            data[offset + 2] = !!data[offset + 2];
        else if (PROPERTY_TYPE_INT16 == data[offset + 1])
            *(uint16_t *)(data + offset + 1 + 1) = endian16(*(uint16_t *)(data + offset + 1 + 1));
        else if (PROPERTY_TYPE_INT32 == data[offset + 1])
            *(uint32_t *)(data + offset + 1 + 1) = endian32(*(uint32_t *)(data + offset + 1 + 1));
        else if (PROPERTY_TYPE_INT64 == data[offset + 1])
            *(uint64_t *)(data + offset + 1 + 1) = endian64(*(uint64_t *)(data + offset + 1 + 1));
        else if (PROPERTY_TYPE_STRING == data[offset + 1] || PROPERTY_TYPE_BUFFER == data[offset + 1])
            *(uint16_t *)(data + offset + 1 + 1) = endian16(*(uint16_t *)(data + offset + 1 + 1));

        ret += property_set(addr, data[offset + 0], data[offset + 1], data + offset + 2);

        offset += property_skip(*addr, offset, data[offset + 1]);
    }
    return !!ret;
}

uint32_t property_iterator(uint32_t addr, uint32_t offset, uint8_t *property, uint8_t *type)
{
    uint32_t length = block_size(addr);
    if (!offset)
        offset = 3;

    if (offset >= length)
        return 0;
    block_read(addr, offset, property, 1ul);
    if (!*property)
        return 0;

    block_read(addr, offset + 1, type, 1ul);

    return offset + property_skip(addr, offset, *type);
}

uint8_t property_type(uint32_t addr, uint8_t property)
{
    uint32_t length = block_size(addr);
    uint32_t offset = 3;
    if (property == PROPERTY_ENTITY_ID)
        return PROPERTY_TYPE_INT16;
    else if (property == PROPERTY_ENTITY_TYPE)
        return PROPERTY_TYPE_INT8;
    while (offset < length)
    {
        uint8_t p;
        uint8_t t;
        block_read(addr, offset + 0, &p, 1ul);
        if (!p)
            break;
        block_read(addr, offset + 1, &t, 1ul);
        if (p == property)
            return t;
        offset += property_skip(addr, offset, t);
    }
    return 0xff;
}

uint32_t property_length(uint8_t type, const void *value)
{
    uint32_t size = MAX_PROPERTY_SIZE[type];
    if (type == PROPERTY_TYPE_BUFFER || type == PROPERTY_TYPE_STRING)
        return size + *(uint16_t *)value;
    return size;
}

uint32_t property_size(uint32_t addr, uint8_t property)
{
    uint32_t length = block_size(addr);
    uint32_t offset = 3;
    if (PROPERTY_ENTITY_ID == property)
        return 2;
    if (PROPERTY_ENTITY_TYPE == property)
        return 1;
    while (offset < length)
    {
        uint8_t p;
        uint8_t t;
        block_read(addr, offset + 0, &p, 1ul);
        if (!p)
            break;
        block_read(addr, offset + 1, &t, 1ul);
        if (p == property)
        {
            if (t <= PROPERTY_TYPE_INT64)
            {
                return MAX_PROPERTY_SIZE[t];
            }
            else if (t == PROPERTY_TYPE_BUFFER || t == PROPERTY_TYPE_STRING)
            {
                uint16_t size;
                block_read(addr, offset + 2, &size, 2ul);
                return 2 + size;
            }
            else
            {
                LogHalt("unknown property type %u", t);
            }
        }
        offset += property_skip(addr, offset, t);
    }
    return 0;
}

void property_pack(uint8_t type, void *value)
{
    if (type == PROPERTY_TYPE_INT16 || type == PROPERTY_TYPE_BUFFER || type == PROPERTY_TYPE_STRING)
        *(uint16_t *)value = endian16(*(uint16_t *)value);
    else if (type == PROPERTY_TYPE_INT32)
        *(uint32_t *)value = endian32(*(uint32_t *)value);
    else if (type == PROPERTY_TYPE_INT64)
        *(uint64_t *)value = endian64(*(uint64_t *)value);
}

void property_unpack(uint8_t type, void *value)
{
    if (type == PROPERTY_TYPE_INT16 || type == PROPERTY_TYPE_BUFFER || type == PROPERTY_TYPE_STRING)
        *(uint16_t *)value = endian16(*(uint16_t *)value);
    else if (type == PROPERTY_TYPE_INT32)
        *(uint32_t *)value = endian32(*(uint32_t *)value);
    else if (type == PROPERTY_TYPE_INT64)
        *(uint64_t *)value = endian64(*(uint64_t *)value);
}

int property_release(uint32_t *addr, uint8_t property)
{
    uint8_t ret = 0;
    uint32_t length = block_size(*addr);
    uint32_t pos = 3;
    uint32_t offset = 3;
    uint32_t ap = 0;
    if (property == PROPERTY_ENTITY_ID || property == PROPERTY_ENTITY_TYPE)
    {
        block_free(*addr);
        *addr = 0;
        return 1;
    }
    ap = block_alloc(length);
    if (!ap)
    {
        *addr = ap;
        return 1;
    }
    block_copy(ap, 0ul, *addr, 0ul, 3ul);
    while (pos < length)
    {
        uint8_t p;
        uint8_t t;
        block_read(*addr, pos + 0, &p, 1ul);
        if (!p)
            break;
        block_read(*addr, pos + 1, &t, 1ul);
        if (p != property)
        {
            block_write(ap, offset + 0, &p, 1ul);
            block_write(ap, offset + 1, &t, 1ul);

            if (PROPERTY_TYPE_BIT == t)
            {
                block_read(*addr, pos + 2, &p, 1ul);
                p = byte_bit_count(p) % 2;
                block_write(ap, offset + 2, &p, 1ul);
            }
            else if (t <= PROPERTY_TYPE_INT64)
            {
                block_copy(ap, offset + 2, *addr, pos + 2, (uint32_t)MAX_PROPERTY_SIZE[t]);
            }
            else if (PROPERTY_TYPE_BUFFER == t || PROPERTY_TYPE_STRING == t)
            {
                uint16_t size = 0;
                block_read(*addr, pos + 2, &size, 2ul);

                block_write(ap, offset + 2, &size, 2ul);
                block_copy(ap, offset + 2 + 2, *addr, pos + 2 + 2, size);
            }
            offset += property_skip(*addr, pos, t);
        }
        else
        {
            ret = 1;
        }
        pos += property_skip(*addr, pos, t);
    }
    block_free(*addr);
    *addr = ap;
    return ret;
}

int property_set(uint32_t *addr, uint8_t property, uint8_t type, const void *value)
{
    uint8_t alloc = 0;
    uint32_t length = block_size(*addr);
    uint32_t ap = 0;
    uint32_t pos = 3;
    uint32_t offset = 3;
    uint32_t size = property_length(type, value);

    if (PROPERTY_ENTITY_TYPE == property)
    {
        if (!block_conflict(*addr, 0ul, value, 1ul))
        {
            if (!block_compare(*addr, 0ul, value, 1ul))
                return 0;
            block_write(*addr, 0ul, value, 1ul);
            return 1;
        }
        else
        {
            alloc = 1;
        }
    }
    else if (PROPERTY_ENTITY_ID == property)
    {
        if (!block_conflict(*addr, 1ul, value, 2ul))
        {
            if (!block_compare(*addr, 1ul, value, 2ul))
                return 0;
            block_write(*addr, 1ul, value, 2ul);
            return 1;
        }
        else
        {
            alloc = 1;
        }
    }
    while (offset < length)
    {
        uint8_t t;
        block_read(*addr, offset + 0, &t, 1ul);
        if (!t)
            break;
        if (t == property)
        {
            alloc = 1;
            if (!block_compare(*addr, offset + 1, &type, 1ul) &&
                !block_conflict(*addr, offset + 1 + 1, value, size))
            {
                block_write(*addr, offset + 1 + 1, value, size);
                return 1;
            }
        }
        block_read(*addr, offset + 1, &t, 1ul);
        offset += property_skip(*addr, offset, t);
    }
    if (!alloc)
    {
        switch (type)
        {
            case PROPERTY_TYPE_BIT:
                *(uint8_t *)value = byte_bit_count((*(uint8_t *)value)) % 2;
            case PROPERTY_TYPE_INT8: 
            case PROPERTY_TYPE_INT16:
            case PROPERTY_TYPE_INT32:
            case PROPERTY_TYPE_INT48:
            case PROPERTY_TYPE_INT64:
            case PROPERTY_TYPE_STRING:
            case PROPERTY_TYPE_BUFFER:
            {
                if (offset + 1 + 1 + size < length)
                {
                    block_write(*addr, offset + 0, &property, 1ul);
                    block_write(*addr, offset + 1, &type, 1ul);
                    block_write(*addr, offset + 2, value, size);
                    return 1;
                }
                break;
            }
            default:
            {
                LogError("unknown type(%d)", type);
                return 0;
            }
        }
    }

    if (PROPERTY_ENTITY_ID == property)
    {
        ap = block_alloc(offset);
        block_copy(ap, 0ul, *addr, 0ul, 1ul);
        block_write(ap, 1ul, value, 2ul);
    }
    else if (PROPERTY_ENTITY_TYPE == property)
    {
        ap = block_alloc(offset);
        block_write(ap, 0ul, value, 1ul);
        block_copy(ap, 1ul, *addr, 1ul, 2ul);
    }
    else
    {
        ap = block_alloc(offset + 1 + 1 + size);
        block_copy(ap, 0ul, *addr, 0ul, 3ul);
    }

    length = offset;
    offset = 3;
    pos = 3;
    while (pos < length)
    {
        uint8_t p;
        uint8_t t;
        block_read(*addr, pos + 0, &p, 1ul);
        if (!p)
            break;
        block_read(*addr, pos + 1, &t, 1ul);
        if (p != property)
        {
            block_write(ap, offset + 0, &p, 1ul);
            block_write(ap, offset + 1, &t, 1ul);

            if (PROPERTY_TYPE_BIT == t)
            {
                block_read(*addr, pos + 2, &p, 1ul);
                p = byte_bit_count(p) % 2;
                block_write(ap, offset + 2, &p, 1ul);
            }
            else if (t <= PROPERTY_TYPE_INT64)
            {
                block_copy(ap, offset + 2, *addr, pos + 2, (uint32_t)MAX_PROPERTY_SIZE[t]);
            }
            else if (PROPERTY_TYPE_STRING == t || PROPERTY_TYPE_BUFFER == t)
            {
                uint16_t l;
                block_read(*addr, pos + 2, &l, 2ul);

                block_write(ap, offset + 2, &l, 2ul);
                block_copy(ap, offset + 2 + 2, *addr, pos + 2 + 2, (uint32_t)l);
            }
            offset += property_skip(*addr, pos, t);
        }
        pos += property_skip(*addr, pos, t);
    }
    if (property != PROPERTY_ENTITY_TYPE && property != PROPERTY_ENTITY_ID)
    {
        block_write(ap, offset + 0, &property, 1ul);
        block_write(ap, offset + 1, &type, 1ul);
        block_write(ap, offset + 2, value, size);
    }
    block_free(*addr);
    *addr = ap;
    return 1;
}

int property_get(uint32_t addr, uint8_t property, uint16_t offset, void *value, uint32_t size)
{
    uint32_t pos = 3;
    uint32_t length = block_size(addr);
    if (property == PROPERTY_ENTITY_TYPE)
    {
        if (offset > 1)
            offset = 1;
        if (offset + size > 1)
            size = 1 - offset;
        if (size)
            block_read(addr, offset, value, size);
        return size;
    }
    else if (property == PROPERTY_ENTITY_ID)
    {
        if (offset > 2)
            offset = 2;
        if (offset + size > 2)
            size = 2 - offset;
        if (size)
            block_read(addr, 1 + offset, value, size);
        return size;
    }
    while (pos < length)
    {
        uint8_t p;
        uint8_t type;
        block_read(addr, pos + 0, &p, 1ul);
        if (!p)
            break;
        block_read(addr, pos + 1, &type, 1ul);
        if (p == property)
        {
            if (type <= PROPERTY_TYPE_INT64)
            {
                if (offset > MAX_PROPERTY_SIZE[type])
                    return 0;
                if (offset + size > MAX_PROPERTY_SIZE[type])
                    size = MAX_PROPERTY_SIZE[type] - offset;
                block_read(addr, pos + 2 + offset, value, size);
                return size;
            }
            else
            {
                uint16_t l;
                block_read(addr, pos + 2, &l, 2ul);
                if (offset > 2 + l)
                    return 0;
                if (offset + size > 2 + l)
                    size = 2 + l - offset;
                block_read(addr, pos + 2 + offset, value, size);
                return size;
            }
        }
        pos += property_skip(addr, pos, type);
    }
    return -1;
}

bool property_exist(uint32_t addr, uint8_t property)
{
    uint32_t pos = 3;
    uint32_t length = block_size(addr);
    if (PROPERTY_ENTITY_ID == property || PROPERTY_ENTITY_TYPE == property)
        return 1;
    while (pos < length)
    {
        uint8_t p;
        uint8_t type;
        block_read(addr, pos + 0, &p, 1ul);
        if (!p)
            return 0;
        if (p == property)
            return 1;
        block_read(addr, pos + 1, &type, 1ul);

        pos += property_skip(addr, pos, type);
    }
    return 0;
}

int property_compare(uint32_t addr, uint8_t property, uint16_t offset, const void *value, uint32_t size)
{
    uint32_t pos = 3;
    uint32_t length = block_size(addr);
    if (PROPERTY_ENTITY_TYPE == property)
        return block_compare(addr, 0 + offset, value, size);
    if (PROPERTY_ENTITY_ID == property)
        return block_compare(addr, 1 + offset, value, size);
    while (pos < length)
    {
        uint8_t p;
        uint8_t type;
        block_read(addr, pos + 0, &p, 1ul);
        if (!p)
            break;
        block_read(addr, pos + 1, &type, 1ul);
        if (p == property)
        {
            if (PROPERTY_TYPE_BIT == type)
            {
                uint8_t v1, v2 = *(uint8_t *)value;
                if (offset || !size)
                    return 1;
                block_read(addr, pos + 2, &v1, 1ul);
                
                v1 = byte_bit_count(v1) % 2;
                v2 = byte_bit_count(v2) % 2;
                
                if (v1 == v2)
                    return 0;
                else if (v1 > v2)
                    return 1;
                return -1;
            }
            else
            {
                return block_compare(addr, pos + 2 + offset, value, size);
            }
        }
        pos += property_skip(addr, pos, type);
    }
    return -1;
}

int property_bit_get(uint32_t addr, uint8_t property)
{
    uint8_t v = 0;
    property_get(addr, property, 0, &v, 1ul);
    return byte_bit_count(v) % 2;
}

int property_bit_set(uint32_t *addr, uint8_t property)
{
    uint8_t value = 0;
    uint8_t v = 0;
    property_get(*addr, property, 0, &v, 1ul);
    
    value = v;

    v = byte_bit_count(v) % 2;

    if (v)
        return 0;

    if (0xff == value)
        value = 0;
    value = (value << 1) | 1;
    return property_set(addr, property, PROPERTY_TYPE_BIT, &value);
}

int property_bit_clear(uint32_t *addr, uint8_t property)
{
    uint8_t value = 0;
    uint8_t v = 0;
    property_get(*addr, property, 0, &v, 1ul);
    
    value = v;

    v = byte_bit_count(v) % 2;

    if (!v)
        return 0;
    
    value = (value << 1) | 1;
    return property_set(addr, property, PROPERTY_TYPE_BIT, &value);
}
