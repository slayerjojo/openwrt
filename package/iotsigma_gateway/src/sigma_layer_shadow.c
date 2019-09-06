#include "sigma_layer_shadow.h"
#include "interface_flash.h"
#include "entity.h"

static int sls_monitor(void *ctx, int event, const SLEEntity *entity, uint8_t property, const SLEValue *value)
{
    UNUSED(ctx);

    if (!os_memcmp(entity->domain->terminal, sll_terminal_local(), MAX_TERMINAL_ID) ||
        !os_memcmp(entity->domain->terminal, sll_src(0), MAX_TERMINAL_ID))
        return 0;

    if (flash_space(entity->domain) < 0)
        return 0;
    
    uint32_t addr = entity_get(entity->entity);
    if (event == EVENT_ENTITY_CREATE)
    {
        if (addr)
            property_set(&addr, PROPERTY_ENTITY_TYPE, PROPERTY_TYPE_INT8, value->value, 1);
        else
            addr = entity_init(entity->entity, *(uint8_t *)value->value, 0);
    }
    else if (event == EVENT_ENTITY_UPDATE)
    {
        if (!addr)
            return 0;
        uint8_t length = property_value_size(value->type, value->value);
        const uint8_t *v = value->value;
        if (value->type == PROPERTY_TYPE_BUFFER || value->type == PROPERTY_TYPE_STRING)
        {
            length -= 2;
            v += 2;
        }
        property_set(&addr, property, value->type, v, length);
    }
    else if (event == EVENT_ENTITY_RELEASE)
    {
        if (!addr)
            return 0;
        entity_release(addr);
    }
    return 0;
}

void sls_init(void)
{
    sle_listen(sls_monitor, 0);
}

void sls_update(void)
{
}

int sls_read(const SLEEntity *entity, uint8_t property, SLEValue *value, SigmaEntityListener resp, void *ctx)
{
    if (flash_space(entity->domain) < 0)
        return sle_read(entity, property, value, resp, ctx);

    uint32_t addr = entity_get(entity->entity);
    if (!addr)
        return sle_read(entity, property, value, resp, ctx);

    uint8_t type = property_type(addr, property);
    uint8_t v[2];
    property_get(addr, property, 0, v, 2);
    uint16_t length = property_value_size(type, value);

    SLEValue *val = value;
    if (!val)
        val = (SLEValue *)os_malloc(sizeof(SLEValue) + length);
    property_get(addr, property, 0, val, length);
    if (resp)
        resp(ctx, EVENT_ENTITY_RESULT, entity, property, val);

    if (!value)
        os_free(val);
    return 0;
}

void sls_iterator(const SigmaDomain *domain, uint8_t type, SigmaEntityListener resp, void *ctx)
{
    if (flash_space(domain) < 0)
        return sls_iterator(domain, type, resp, ctx);

    SLEEntity entity;
    entity.domain = domain;

    uint8_t buffer[sizeof(SLEValue) + sizeof(uint8_t)];
    SLEValue *value = (SLEValue *)buffer;
    value->type = PROPERTY_TYPE_INT8;

    uint32_t addr = 0;
    while ((addr = (type == 0xff ? entity_iterator(addr) : entity_find(type, addr))))
    {
        property_get(addr, PROPERTY_ENTITY_ID, 0, &entity.entity, 2);
        property_get(addr, PROPERTY_ENTITY_TYPE, 0, value->value, 1);

        int ret = 0;
        if (resp)
            ret = resp(ctx, EVENT_ENTITY_RESULT_ITERATOR, &entity, PROPERTY_ENTITY_TYPE, value);
        if (ret)
            break;
    }
}
