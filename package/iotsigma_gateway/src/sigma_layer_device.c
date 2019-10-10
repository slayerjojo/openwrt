#include "sigma_layer_device.h"
#include "sigma_layer_rpc.h"
#include "entity_type.h"
#include "sigma_opcode.h"

//----------------------------------

typedef struct
{
    uint16_t entity;
    uint8_t link[MAX_LINK_ID];
}__attribute__((packed)) ParameterDeviceLink;

static int rpc_device_link(const SigmaDomain *domain, uint16_t opcode, uint32_t session, uint8_t response, const void *parameter, uint16_t size)
{
}

void sld_init(void)
{
    slr_register(OPCODE_RPC_DEVICE_LINK, rpc_device_link);
}

void sld_update(void)
{
}

int sld_node_create(SLEEntity *node)
{
    return sle_create(0, ENTITY_TYPE_NODE, 0, node, 0, 0);
}

int sld_device_create(SLEEntity *node, SLEEntity *device)
{
    if (sle_create(0, ENTITY_TYPE_DEVICE, 0, device, 0, 0) < 0)
        return -1;
    SLE_VALUE_DEF_16(value, node->entity);
    return sle_write(device, PROPERTY_DEVICE_NODE, (SLEValue *)&value, 0, 0);
}

int sld_value_update(const SLEEntity *device, const SLEValue *value)
{
    uint32_t session = sle_iterator_create(0, ENTITY_TYPE_VALUE, 1);
    SLE_VALUE_DEF_16(dv, device->entity);
    if (sle_iterator_filter(session, SLE_FILTER_EQUAL, PROPERTY_VALUE_DEVICE, (SLEValue *)&dv) < 0)
    {
        sle_iterator_release(session);
        return -1;
    }
    uint32_t addr = 0;
    SLEEntity ev = {0};
    if (sle_iterator_execute(session, &addr, &ev, 0, 0) < 0)
    {
        sle_iterator_release(session);
        return -1;
    }
    sle_iterator_release(session);
    if (!addr || sle_create(0, ENTITY_TYPE_VALUE, 2 + 2, &ev, 0, 0) < 0)
        return -1;

    if (PROPERTY_TYPE_BIT == value->type)
        return sle_write(&ev, PROPERTY_VALUE_BOOL, value, 0, 0);
    else if (PROPERTY_TYPE_INT8 == value->type)
        return sle_write(&ev, PROPERTY_VALUE_INT8, value, 0, 0);
    else if (PROPERTY_TYPE_INT16 == value->type)
        return sle_write(&ev, PROPERTY_VALUE_INT16, value, 0, 0);
    else if (PROPERTY_TYPE_INT32 == value->type)
        return sle_write(&ev, PROPERTY_VALUE_INT32, value, 0, 0);
    else if (PROPERTY_TYPE_INT48 == value->type)
        return sle_write(&ev, PROPERTY_VALUE_INT48, value, 0, 0);
    else if (PROPERTY_TYPE_INT64 == value->type)
        return sle_write(&ev, PROPERTY_VALUE_INT64, value, 0, 0);
    else if (PROPERTY_TYPE_BUFFER == value->type)
        return sle_write(&ev, PROPERTY_VALUE_BUFFER, value, 0, 0);
    else if (PROPERTY_TYPE_STRING == value->type)
        return sle_write(&ev, PROPERTY_VALUE_STRING, value, 0, 0);
    return 0;
}

int sld_link(const SLEEntity *device, const uint8_t *link, SigmaEntityListener resp, void *ctx)
{
    SigmaRPCCall *caller = slr_create(device->domain, OPCODE_RPC_DEVICE_LINK, sizeof(ParameterDeviceLink) + MAX_LINK_ID, resp ? sizeof(EntityListenerContext) : 0);
    if (!caller)
        return -1;
    ParameterDeviceLink *p = (ParameterDeviceLink *)slr_parameter(caller);
    p->entity = network_htons(device->entity);
    os_memcpy(p->link, link, MAX_LINK_ID);

    EntityListenerContext *extend = 0;
    if (resp)
    {
        extend = (EntityListenerContext *)slr_extend(caller);
        extend->imm.value = 0;
        extend->resp = resp;
        extend->ctx = ctx;
    }
    slr_call(caller, resp ? responser_write : 0, extend, TIMEOUT_ENTITY_RPC);
    return 0;
}
