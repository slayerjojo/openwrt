#include "sigma_sublayer_device_zigbee.h"
#include "sigma_layer_device.h"
#include "sigma_layer_entity.h"
#include "sigma_mission.h"
#include "interface_os.h"
#include "entity_type.h"
#include "entity.h"
#include "zigbee.h"
#include "fsm.h"

#define TIMEOUT_SIMPLE_DESC 10000
#define TIMEOUT_ONOFF 10000
#define TIMEOUT_LEVEL 10000

enum
{
    STATE_ZIGBEE_DEVICE_SIMPLE_DESC = 0,
    STATE_ZIGBEE_DEVICE_SIMPLE_DESC_WAIT,
    STATE_ZIGBEE_DEVICE_VALUE_ONOFF,
    STATE_ZIGBEE_DEVICE_VALUE_ONOFF_WAIT,
    STATE_ZIGBEE_DEVICE_VALUE_LEVEL,
    STATE_ZIGBEE_DEVICE_VALUE_LEVEL_WAIT,
    STATE_ZIGBEE_DEVICE_FINISHED
};

typedef struct _zigbee_device_sync_queue
{
    struct _zigbee_device_sync_queue *next;

    uint16_t device;
}QueueZigbeeDeviceSync;

static struct
{
    SigmaMission *mission;
    QueueZigbeeDeviceSync *queue;
    FSMState state;
} _zigbee_device_sync = {0};

static void response_value(int ret, 
    uint16_t sht, uint8_t ep,
    uint16_t cluster, uint16_t attribute,
    uint8_t *attrs, uint8_t count)
{
    SLEEntity ed = {0};
    uint32_t session = 0;
    uint32_t addr = 0;

    if (ret < 0)
        return;

    if (STATE_ZIGBEE_DEVICE_VALUE_ONOFF_WAIT == fsm_state(&_zigbee_device_sync.state))
        fsm_update(&_zigbee_device_sync.state, STATE_ZIGBEE_DEVICE_VALUE_LEVEL);
    else if (STATE_ZIGBEE_DEVICE_VALUE_LEVEL_WAIT == fsm_state(&_zigbee_device_sync.state))
        fsm_update(&_zigbee_device_sync.state, STATE_ZIGBEE_DEVICE_FINISHED);

    session = sle_iterator_create(0, ENTITY_TYPE_NODE, 1);
    {
        SLE_VALUE_DEF_16(value, sht);
        sle_iterator_filter(session, SLE_FILTER_EQUAL, PROPERTY_NODE_SHORT, (SLEValue *)&value);
    }
    addr = 0;
    if (sle_iterator_execute(session, &addr, &ed, 0, 0) > 0)
    {
        sle_iterator_release(session);
        return;
    }
    sle_iterator_release(session);

    {
        SLE_VALUE_DEF_BIT(value, 1);
        sle_write(&ed, PROPERTY_NODE_ONLINE, (SLEValue *)&value, 0, 0);
    }

    session = sle_iterator_create(0, ENTITY_TYPE_DEVICE, 1);
    {
        SLE_VALUE_DEF_16(value, ed.entity);
        sle_iterator_filter(session, SLE_FILTER_EQUAL, PROPERTY_DEVICE_NODE, (SLEValue *)&value);
    }
    {
        SLE_VALUE_DEF_8(value, ep);
        sle_iterator_filter(session, SLE_FILTER_EQUAL, PROPERTY_DEVICE_ZIGBEE_ENDPOINT, (SLEValue *)&value);
    }
    addr = 0;
    if (sle_iterator_execute(session, &addr, &ed, 0, 0) <= 0 || !addr)
    {
        sle_iterator_release(session);
        return;
    }
    sle_iterator_release(session);
    
    //attr = (((uint16_t)attrs[0]) << 8) | attrs[1];

    if (attrs[2])
        return;

    switch(attrs[3])
    {
        case ZCL_DATATYPE_BOOLEAN:
        {
            SLE_VALUE_DEF_BIT(value, attrs[4]);
            sld_value_update(&ed, (SLEValue *)&value);
            break;
        }
        case ZCL_DATATYPE_UINT8:
        case ZCL_DATATYPE_DATA8:
        case ZCL_DATATYPE_BITMAP8:
        case ZCL_DATATYPE_INT8:
        case ZCL_DATATYPE_ENUM8:
        {
            SLE_VALUE_DEF_8(value, attrs[4]);
            sld_value_update(&ed, (SLEValue *)&value);
            break;
        }
        case ZCL_DATATYPE_UINT16:
        case ZCL_DATATYPE_DATA16:
        case ZCL_DATATYPE_BITMAP16:
        case ZCL_DATATYPE_INT16:
        case ZCL_DATATYPE_SEMI_PREC:
        case ZCL_DATATYPE_CLUSTER_ID:
        case ZCL_DATATYPE_ATTR_ID:
        {
            SLE_VALUE_DEF_16(value, *(uint16_t *)(attrs + 4));
            sld_value_update(&ed, (SLEValue *)&value);
            break;
        }
        case ZCL_DATATYPE_UINT24:
        case ZCL_DATATYPE_DATA24:
        case ZCL_DATATYPE_BITMAP24:
        case ZCL_DATATYPE_INT24:
        {
            SLE_VALUE_DEF_32(value, (((uint32_t)attrs[4]) << 16) | (((uint32_t)attrs[5]) << 8) | attrs[6]);
            sld_value_update(&ed, (SLEValue *)&value);
            break;
        }
        case ZCL_DATATYPE_DATA32:
        case ZCL_DATATYPE_BITMAP32:
        case ZCL_DATATYPE_UINT32:
        case ZCL_DATATYPE_INT32:
        case ZCL_DATATYPE_SINGLE_PREC:
        case ZCL_DATATYPE_TOD:
        case ZCL_DATATYPE_DATE:
        case ZCL_DATATYPE_UTC:
        case ZCL_DATATYPE_BAC_OID:
        {
            SLE_VALUE_DEF_32(value, *(uint32_t *)(attrs + 4));
            sld_value_update(&ed, (SLEValue *)&value);
            break;
        }
        case ZCL_DATATYPE_UINT40:
        case ZCL_DATATYPE_INT40:
        {
            SLE_VALUE_DEF_48(value, attrs + 4);
            value.value[5] = 0;
            sld_value_update(&ed, (SLEValue *)&value);
            break;
        }
        case ZCL_DATATYPE_UINT48:
        case ZCL_DATATYPE_INT48:
        {
            SLE_VALUE_DEF_48(value, attrs + 4);
            sld_value_update(&ed, (SLEValue *)&value);
            break;
        }
        case ZCL_DATATYPE_UINT56:
        case ZCL_DATATYPE_INT56:
        {
            SLE_VALUE_DEF_64(value, attrs + 4);
            value.value[7] = 0;
            sld_value_update(&ed, (SLEValue *)&value);
            break;
        }
        case ZCL_DATATYPE_DOUBLE_PREC:
        case ZCL_DATATYPE_IEEE_ADDR:
        case ZCL_DATATYPE_UINT64:
        case ZCL_DATATYPE_INT64:
        {
            SLE_VALUE_DEF_64(value, attrs + 4);
            sld_value_update(&ed, (SLEValue *)&value);
            break;
        }
        case ZCL_DATATYPE_128_BIT_SEC_KEY:
        {
            SLE_VALUE_DEF_BUF_STATIC(value, attrs + 4, SEC_KEY_LEN);
            sld_value_update(&ed, (SLEValue *)&value);
            break;
        }
    }
}

static void response_simple_desc(int ret, 
    uint16_t sht, uint8_t ep, 
    uint16_t profile, uint16_t type, 
    uint8_t version, 
    uint8_t *ins, uint8_t in_count, 
    uint8_t *outs, uint8_t out_count)
{
    SLEEntity en = {0};
    uint32_t addr = 0;
    uint32_t session = 0;
    LogAction("ret:%d short:0x%04x ep:%u profile:0x%04x type:%04x", ret, sht, ep, profile, type);

    session = sle_iterator_create(0, ENTITY_TYPE_NODE, 1);
    {
        SLE_VALUE_DEF_16(value, sht);
        sle_iterator_filter(session, SLE_FILTER_EQUAL, PROPERTY_NODE_SHORT, (SLEValue *)&value);
    }
    addr = 0;
    if (sle_iterator_execute(session, &addr, &en, 0, 0) < 0)
    {
        sle_iterator_release(session);
        return;
    }
    sle_iterator_release(session);

    if (!addr)
        return;

    if (ret < 0)
        return;

    session = sle_iterator_create(0, ENTITY_TYPE_DEVICE, 1);
    {
        SLE_VALUE_DEF_16(value, en.entity);
        sle_iterator_filter(session, SLE_FILTER_EQUAL, PROPERTY_DEVICE_NODE, (SLEValue *)&value);
    }
    {
        SLE_VALUE_DEF_8(value, ep);
        sle_iterator_filter(session, SLE_FILTER_EQUAL, PROPERTY_DEVICE_ZIGBEE_ENDPOINT, (SLEValue *)&value);
    }
    addr = 0;
    if (sle_iterator_execute(session, &addr, &en, 0, 0) > 0)
    {
        sle_iterator_release(session);
        return;
    }
    sle_iterator_release(session);
    {
        SLE_VALUE_DEF_16(value, profile);
        sle_write(&en, PROPERTY_DEVICE_ZIGBEE_PROFILE, (SLEValue *)&value, 0, 0);
    }
    {
        SLE_VALUE_DEF_16(value, type);
        sle_write(&en, PROPERTY_DEVICE_ZIGBEE_DEVICE, (SLEValue *)&value, 0, 0);
    }
    {
        SLE_VALUE_DEF_BUF(value, ins, in_count > 16 ? 16 * 2 : in_count * 2);
        if (value)
        {
            sle_write(&en, PROPERTY_DEVICE_ZIGBEE_CLUSTERS_IN, (SLEValue *)value, 0, 0);
            os_free(value);
        }
    }
    {
        SLE_VALUE_DEF_BUF(value, outs, out_count > 16 ? 16 * 2 : out_count * 2);
        if (value)
        {
            sle_write(&en, PROPERTY_DEVICE_ZIGBEE_CLUSTERS_OUT, (SLEValue *)value, 0, 0);
            os_free(value);
        }
    }
    fsm_update(&(_zigbee_device_sync.state), STATE_ZIGBEE_DEVICE_VALUE_ONOFF);
}

static int mission_zigbee_device_sync(SigmaMission *mission, void *ctx)
{
    if (STATE_ZIGBEE_DEVICE_SIMPLE_DESC == fsm_state(&_zigbee_device_sync.state))
    {
        if (!zigbee_idle())
            return 0;

        QueueZigbeeDeviceSync *q = _zigbee_device_sync.queue;
        while (q && q->next)
            q = q->next;
        if (!q)
        {
            _zigbee_device_sync.queue = 0;
            return 1;
        }

        SLEEntity en = {0};
        en.entity = q->device;

        SLE_VALUE_DEF_8(ep, 0);
        if (sle_read(&en, PROPERTY_DEVICE_ZIGBEE_ENDPOINT, (SLEValue *)&ep, 0, 0) < 0)
        {
            fsm_update(&_zigbee_device_sync.state, STATE_ZIGBEE_DEVICE_FINISHED);
            return 0;
        }

        SLE_VALUE_DEF_16(node, 0);
        if (sle_read(&en, PROPERTY_DEVICE_NODE, (SLEValue *)&node, 0, 0) < 0)
        {
            fsm_update(&_zigbee_device_sync.state, STATE_ZIGBEE_DEVICE_FINISHED);
            return 0;
        }

        en.entity = node.value;

        SLE_VALUE_DEF_16(sht, 0)
        if (sle_read(&en, PROPERTY_NODE_SHORT, (SLEValue *)&sht, 0, 0) < 0)
        {
            fsm_update(&_zigbee_device_sync.state, STATE_ZIGBEE_DEVICE_FINISHED);
            return 0;
        }

        fsm_update(&_zigbee_device_sync.state, STATE_ZIGBEE_DEVICE_SIMPLE_DESC_WAIT);
        zigbee_simple_desc(sht.value, ep.value, response_simple_desc);
    }
    else if (STATE_ZIGBEE_DEVICE_SIMPLE_DESC_WAIT == fsm_state(&_zigbee_device_sync.state))
    {
        if (fsm_time(&_zigbee_device_sync.state) > TIMEOUT_SIMPLE_DESC)
            fsm_update(&_zigbee_device_sync.state, STATE_ZIGBEE_DEVICE_FINISHED);
    }
    else if (STATE_ZIGBEE_DEVICE_VALUE_ONOFF == fsm_state(&_zigbee_device_sync.state))
    {
        if (!zigbee_idle())
            return 0;

        QueueZigbeeDeviceSync *q = _zigbee_device_sync.queue;
        while (q && q->next)
            q = q->next;
        if (!q)
        {
            _zigbee_device_sync.queue = 0;
            return 1;
        }

        SLEEntity en = {0};
        en.entity = q->device;

        SLE_VALUE_DEF_BUF_STATIC(value, 0, 16 * 2);
        if (sle_read(&en, PROPERTY_DEVICE_ZIGBEE_CLUSTERS_IN, (SLEValue *)value, 0, 0) < 0)
        {
            fsm_update(&_zigbee_device_sync.state, STATE_ZIGBEE_DEVICE_FINISHED);
            return 0;
        }

        size_t i = 0;
        for (i = 0; i < value->size / 2; i++)
        {
            uint16_t cluster = *(((uint16_t *)value->value) + i);
            if (ZCL_CLUSTER_ID_GEN_ON_OFF == cluster)
                break;
        }
        if (i < value->size / 2)
        {
            SLE_VALUE_DEF_8(ep, 0);
            sle_read(&en, PROPERTY_DEVICE_ZIGBEE_ENDPOINT, (SLEValue *)&ep, 0, 0);

            SLE_VALUE_DEF_16(node, 0);
            sle_read(&en, PROPERTY_DEVICE_NODE, (SLEValue *)&node, 0, 0);

            en.entity = node.value;
            SLE_VALUE_DEF_16(sht, 0);
            sle_read(&en, PROPERTY_NODE_SHORT, (SLEValue *)&sht, 0, 0);

            fsm_update(&_zigbee_device_sync.state, STATE_ZIGBEE_DEVICE_VALUE_ONOFF_WAIT);
            zigbee_attribute_read(sht.value, ep.value, ZCL_CLUSTER_ID_GEN_ON_OFF, ATTRID_ON_OFF, response_value);
        }
        else
        {
            fsm_update(&_zigbee_device_sync.state, STATE_ZIGBEE_DEVICE_VALUE_LEVEL);
        }
    }
    else if (STATE_ZIGBEE_DEVICE_VALUE_ONOFF_WAIT == fsm_state(&_zigbee_device_sync.state))
    {
        if (fsm_time(&_zigbee_device_sync.state) > TIMEOUT_ONOFF)
            fsm_update(&_zigbee_device_sync.state, STATE_ZIGBEE_DEVICE_FINISHED);
    }
    else if (STATE_ZIGBEE_DEVICE_VALUE_LEVEL == fsm_state(&_zigbee_device_sync.state))
    {
        if (!zigbee_idle())
            return 0;

        QueueZigbeeDeviceSync *q = _zigbee_device_sync.queue;
        while (q && q->next)
            q = q->next;
        if (!q)
        {
            _zigbee_device_sync.queue = 0;
            return 1;
        }

        SLEEntity en = {0};
        en.entity = q->device;

        uint8_t buffer[sizeof(SLEValueBuffer) + 16 * 2] = {0};
        SLEValueBuffer *value = (SLEValueBuffer *)buffer;
        value->type = PROPERTY_TYPE_BUFFER;
        value->size = 16 * 2;
        if (sle_read(&en, PROPERTY_DEVICE_ZIGBEE_CLUSTERS_IN, (SLEValue *)value, 0, 0) < 0)
        {
            fsm_update(&_zigbee_device_sync.state, STATE_ZIGBEE_DEVICE_FINISHED);
            return 0;
        }

        size_t i = 0;
        for (i = 0; i < value->size / 2; i++)
        {
            uint16_t cluster = *(((uint16_t *)value->value) + i);
            if (ZCL_CLUSTER_ID_GEN_LEVEL_CONTROL == cluster)
                break;
        }
        if (i < value->size / 2)
        {
            SLE_VALUE_DEF_8(ep, 0);
            sle_read(&en, PROPERTY_DEVICE_ZIGBEE_ENDPOINT, (SLEValue *)&ep, 0, 0);

            SLE_VALUE_DEF_16(node, 0);
            sle_read(&en, PROPERTY_DEVICE_NODE, (SLEValue *)&node, 0, 0);

            en.entity = node.value;
            SLE_VALUE_DEF_16(sht, 0);
            sle_read(&en, PROPERTY_NODE_SHORT, (SLEValue *)&sht, 0, 0);

            zigbee_attribute_read(sht.value, ep.value, ZCL_CLUSTER_ID_GEN_LEVEL_CONTROL, ATTRID_LEVEL_CURRENT_LEVEL, response_value);
            fsm_update(&_zigbee_device_sync.state, STATE_ZIGBEE_DEVICE_VALUE_LEVEL_WAIT);
        }
        else
        {
            fsm_update(&_zigbee_device_sync.state, STATE_ZIGBEE_DEVICE_FINISHED);
        }
    }
    else if (STATE_ZIGBEE_DEVICE_VALUE_LEVEL_WAIT == fsm_state(&_zigbee_device_sync.state))
    {
        if (fsm_time(&_zigbee_device_sync.state) > TIMEOUT_LEVEL)
            fsm_update(&_zigbee_device_sync.state, STATE_ZIGBEE_DEVICE_FINISHED);
    }
    else if (STATE_ZIGBEE_DEVICE_FINISHED == fsm_state(&_zigbee_device_sync.state))
    {
        QueueZigbeeDeviceSync *q = _zigbee_device_sync.queue, *prev = 0;
        while (q && q->next)
        {
            prev = q;
            q = q->next;
        }
        if (prev)
            prev->next = 0;
        else
            _zigbee_device_sync.queue = 0;
        free(q);
        fsm_update(&_zigbee_device_sync.state, STATE_ZIGBEE_DEVICE_SIMPLE_DESC);
    }
    return 0;
}

static void ssdz_zigbee_device_sync(uint16_t device)
{
    QueueZigbeeDeviceSync *q = _zigbee_device_sync.queue;
    while (q && q->device != device)
        q = q->next;
    if (q)
        return;
    q = (QueueZigbeeDeviceSync *)os_malloc(sizeof(QueueZigbeeDeviceSync));
    if (!q)
        return;
    q->next = _zigbee_device_sync.queue;
    _zigbee_device_sync.queue = q;

    q->device = device;

    if (!_zigbee_device_sync.mission)
        _zigbee_device_sync.mission = sigma_mission_create(mission_zigbee_device_sync, 0, 0);
}

static void response_active_ep(int ret, uint16_t node_addr, uint8_t *eps, uint8_t count)
{
    uint8_t i;
    uint16_t session = 0;
    uint32_t addr = 0;
    SLEEntity en = {0};
    SLEEntity ed = {0};

    LogAction("ret:%d addr:0x%04x ep count:%u", ret, node_addr, count);

    if (ret < 0)
        return;

    session = sle_iterator_create(0, ENTITY_TYPE_NODE, 1);
    {
        SLE_VALUE_DEF_16(value, node_addr);
        sle_iterator_filter(session, SLE_FILTER_EQUAL, PROPERTY_NODE_SHORT, (SLEValue *)&value);
    }
    addr = 0;
    if (sle_iterator_execute(session, &addr, &en, 0, 0) < 0)
    {
        sle_iterator_release(session);
        return;
    }
    sle_iterator_release(session);

    session = sle_iterator_create(0, ENTITY_TYPE_DEVICE, 1);
    {
        SLE_VALUE_DEF_16(value, en.entity);
        sle_iterator_filter(session, SLE_FILTER_EQUAL, PROPERTY_DEVICE_NODE, (SLEValue *)&value);
    }
    addr = 0;
    while (sle_iterator_execute(session, &addr, &ed, 0, 0) >= 0 && addr)
    {
        SLE_VALUE_DEF_8(value, 0);
        if (sle_read(&ed, PROPERTY_DEVICE_ZIGBEE_ENDPOINT, (SLEValue *)&value, 0, 0) < 0)
            continue;
        
        for (i = 0; i < count; i++)
        {
            if (eps[i] == value.value)
                break;
        }
        if (i >= count)
        {
            sle_release(&ed, 0, 0);
            continue;
        }
        ssdz_zigbee_device_sync(ed.entity);
        eps[i] = 0;
    }
    sle_iterator_release(session);

    for (i = 0; i < count; i++)
    {
        if (!eps[i])
            continue;

        if (sld_device_create(&en, &ed) < 0)
            continue;

        SLE_VALUE_DEF_8(value, eps[i]);
        sle_write(&ed, PROPERTY_DEVICE_ZIGBEE_ENDPOINT, (SLEValue *)&value, 0, 0);

        ssdz_zigbee_device_sync(ed.entity);
    }
}

static void zigbee_device_annce(ZigbeeZDOEvent *e)
{
    int ret = 0;
    uint8_t v;
    SLEEntity en = {0};
    uint16_t session = 0;
    uint32_t addr = 0;

    session = sle_iterator_create(0, ENTITY_TYPE_NODE, 1);
    {
        SLEValue64 value;
        value.type = PROPERTY_TYPE_INT64;
        os_memcpy(value.value, e->payload.device_annouce.ieee, Z_EXTADDR_LEN);
        sle_iterator_filter(session, SLE_FILTER_EQUAL, PROPERTY_NODE_MAC, (SLEValue *)&value);
    }
    addr = 0;
    if (sle_iterator_execute(session, &addr, &en, 0, 0) < 0)
    {
        sle_iterator_release(session);
        return;
    }
    if (!addr || sld_node_create(&en) < 0)
        return;
    {
        SLE_VALUE_DEF_8(value, NODE_PROFILE_ZIGBEE);
        sle_write(&en, PROPERTY_NODE_PROFILE, (SLEValue *)&value, 0, 0);
    }
    {
        SLE_VALUE_DEF_16(value, e->node_addr);
        sle_write(&en, PROPERTY_NODE_SHORT, (SLEValue *)&value, 0, 0);
    }
    {
        SLE_VALUE_DEF_64(value, e->payload.device_annouce.ieee);
        sle_write(&en, PROPERTY_NODE_MAC, (SLEValue *)&value, 0, 0);
    }
    {
        //zigbee specification, page 92
        SLE_VALUE_DEF_8(value, e->payload.device_annouce.capabilities);
        sle_write(&en, PROPERTY_NODE_PROFILE, (SLEValue *)&value, 0, 0);
    }
    {
        SLE_VALUE_DEF_BIT(value, 1);
        sle_write(&en, PROPERTY_NODE_ONLINE, (SLEValue *)&value, 0, 0);
    }

    zigbee_active_ep(e->node_addr, response_active_ep);
}

static void zigbee_attribute_report(ZigbeeZCLEvent *e)
{
    uint8_t i = 0;
    uint16_t device = 0;
    uint32_t addr = 0;
    uint32_t session = 0;
    SLEEntity ed = {0};

    addr = 0;
    if (e->mode == afAddr16Bit)
    {
        session = sle_iterator_create(0, ENTITY_TYPE_NODE, 1);

        SLE_VALUE_DEF_16(v, e->addr.sht);
        sle_iterator_filter(session, SLE_FILTER_EQUAL, PROPERTY_NODE_SHORT, (SLEValue *)&v);
    }
    else if (e->mode == afAddr64Bit)
    {
        session = sle_iterator_create(0, ENTITY_TYPE_NODE, 1);

        SLE_VALUE_DEF_64(v, e->addr.ieee);
        sle_iterator_filter(session, SLE_FILTER_EQUAL, PROPERTY_NODE_MAC, (SLEValue *)&v);
    }
    addr = 0;
    if (sle_iterator_execute(session, &addr, &ed, 0, 0) < 0 || !addr)
    {
        sle_iterator_release(session);
        return;
    }
    sle_iterator_release(session);

    session = sle_iterator_create(0, ENTITY_TYPE_DEVICE, 1);
    if (session)
        return;
    {
        SLE_VALUE_DEF_16(value, ed.entity);
        sle_iterator_filter(session, SLE_FILTER_EQUAL, PROPERTY_DEVICE_NODE, (SLEValue *)&value);
    }
    {
        SLE_VALUE_DEF_8(value, e->ep);
        sle_iterator_filter(session, SLE_FILTER_EQUAL, PROPERTY_DEVICE_ZIGBEE_ENDPOINT, (SLEValue *)&value);
    }
    addr = 0;
    if (sle_iterator_execute(session, &addr, &ed, 0, 0) < 0 || !addr)
    {
        sle_iterator_release(session);
        return;
    }
    sle_iterator_release(session);

    SLE_VALUE_DEF_BUF_STATIC(value, 0, 16 * 2);
    if (sle_read(&ed, PROPERTY_DEVICE_ZIGBEE_CLUSTERS_IN, (SLEValue *)value, 0, 0) >= 0)
    {
        for (i = 0; i < value->size / 2; i++)
        {
            uint16_t *cluster = ((uint16_t *)value->value) + i;
            if (ZCL_CLUSTER_ID_GEN_ON_OFF == *cluster || ZCL_CLUSTER_ID_GEN_LEVEL_CONTROL == *cluster)
                break;
        }
        if (i < value->size / 2)
        {
            uint8_t *attrs = e->attribute.report.attrs;

            for (i = 0; i < e->attribute.report.count; i++)
            {
                uint16_t attr = (((uint16_t)attrs[0]) << 8) | attrs[1];

                if (ZCL_CLUSTER_ID_GEN_ON_OFF == e->cluster && ATTRID_ON_OFF == attr && ZCL_DATATYPE_BOOLEAN == attrs[2])
                {
                    LogAction("device:%d value:%d", ed.entity, attrs[3]);

                    SLE_VALUE_DEF_BIT(value, attrs[3]);
                    sld_value_update(&ed, (SLEValue *)&value);
                }
                else if (ZCL_CLUSTER_ID_GEN_LEVEL_CONTROL == e->cluster && ATTRID_LEVEL_CURRENT_LEVEL == attr && ZCL_DATATYPE_UINT8 == attrs[2])
                {
                    LogAction("device:%d value:%d", ed.entity, attrs[3]);
                    
                    SLE_VALUE_DEF_BIT(value, attrs[3]);
                    sld_value_update(&ed, (SLEValue *)&value);
                }

                attrs += 2 + 1 + zclGetAttrDataLength(attrs[2], attrs + 3);
            }
        }
    }
}

static void zigbee_handler(uint8_t event, void *data, uint8_t size)
{
    if (OPCODE_ZIGBEE_EVENT_NETWORK_START == event)
    {
    }
    else if (EVENT_DEVICE_ANNCE == event)
    {
        zigbee_device_annce((ZigbeeZDOEvent *)data);
    }
    else if (EVENT_ATTRIBUTE_REPORT == event)
    {
        zigbee_attribute_report((ZigbeeZCLEvent *)data);
    }
}

void ssdz_init(void)
{
    zigbee_init(zigbee_handler);
}

void ssdz_update(void)
{
    zigbee_update();
}
