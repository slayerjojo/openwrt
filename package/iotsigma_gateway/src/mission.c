#include "mission.h"
#include "command.h"
#include "entity.h"
#include "entity_type.h"
#include "block.h"
#include "interface_os.h"
#include "interface_network.h"
#include "driver_linux.h"
#include "driver_network_linux.h"
#include "log.h"
#include "zigbee.h"
#include "storage.h"
#include "endian.h"
#include <time.h>

#define MISSION_TYPE_MASTER_STATUS 1
#define MISSION_TYPE_MASTER_MANUAL 2
#define MISSION_TYPE_SLAVE_APPLY 3
#define MISSION_TYPE_LINK_EXECUTE 4
#define MISSION_TYPE_LINK_STATUS 5

typedef struct _mission
{
    struct _mission *next;
    uint8_t type;
}Mission;

typedef struct
{
    Mission mission;

    uint8_t size;
    uint16_t source;
    uint32_t addr;
    uint8_t link[MAX_LINK];
    uint8_t command[];
}MissionLinkExecute;

typedef struct
{
    Mission mission;

    uint16_t master;
    uint16_t device;
    uint32_t addr;
    uint32_t ap;
    uint8_t state;
    uint8_t link[MAX_LINK + 1];
}MissionMasterStatus;

typedef struct
{
    Mission mission;

    uint16_t master;
    uint8_t command;
    uint32_t addr;
    uint8_t *link;
    uint8_t link_payload[];
}MissionMasterManual;

typedef struct
{
    Mission mission;

    uint16_t slave;
}MissionSlaveApply;

static Mission *_mission = 0;
static uint32_t _addr = 0;

static struct
{
    uint32_t sync;
    uint32_t timer;
}_schedule = {0};

static struct
{
    uint8_t state;
    uint8_t ep;
    uint16_t sht;
}_sync_value = {0};

static Mission *mission_add(uint8_t type, uint32_t size)
{
    Mission *mission = (Mission *)os_malloc(size > sizeof(Mission) ? size : sizeof(Mission));
    if (!mission)
        return 0;
    mission->type = type;
    mission->next = _mission;
    _mission = mission;
    return mission;
}

static void mission_remove(Mission *mission)
{
    if (mission == _mission)
    {
        _mission = _mission->next;
    }
    else
    {
        Mission *prev = _mission;
        while (prev && prev->next != mission)
            prev = prev->next;
        if (prev)
            prev->next = mission->next;
    }
    os_free(mission);
}

static void response_sync_value_level(int ret, 
    uint16_t sht, uint8_t ep,
    uint16_t cluster, uint16_t attribute,
    uint8_t *attrs, uint8_t count)
{
    uint16_t entity = 0;
    uint32_t addr = 0;

    while ((addr = entity_find(ENTITY_TYPE_NODE, addr)) && 
        property_compare(addr, PROPERTY_NODE_SHORT, 0, &sht, 2ul));
    if (!addr)
    {
        LogError("node(addr:0x%04x) not found!", sht);
        return;
    }

    if (ret < 0)
    {
        if (property_bit_set(&addr, PROPERTY_NODE_ONLINE))
            build_mission_entity_dirty(&addr);
        return;
    }
    
    property_get(addr, PROPERTY_ENTITY_ID, 0, &entity, 2ul);//node

    addr = 0;
    while ((addr = entity_find(ENTITY_TYPE_DEVICE, addr)) && 
        property_compare(addr, PROPERTY_DEVICE_NODE, 0, &entity, 2ul) &&
        property_compare(addr, PROPERTY_DEVICE_ZIGBEE_ENDPOINT, 0, &ep, 1ul));
    property_get(addr, PROPERTY_ENTITY_ID, 0, &entity, 2ul);//device

    //attr = (((uint16_t)attrs[0]) << 8) | attrs[1];

    if (attrs[2])
        return;

    if (ZCL_DATATYPE_BOOLEAN != attrs[3])
        return;

    addr = 0;
    while ((addr = entity_find(ENTITY_TYPE_VALUE, addr)) && 
        property_compare(addr, PROPERTY_VALUE_DEVICE, 0, &entity, 2ul));
    if (!addr)
        return;
    if (property_set(&addr, PROPERTY_VALUE_LEVEL, PROPERTY_TYPE_INT8, attrs + 4, 1ul))
        build_mission_entity_dirty(&addr);
}

static void response_sync_value_on_off(int ret, 
    uint16_t sht, uint8_t ep,
    uint16_t cluster, uint16_t attribute,
    uint8_t *attrs, uint8_t count)
{
    uint16_t entity = 0;
    uint32_t addr = 0;

    while ((addr = entity_find(ENTITY_TYPE_NODE, addr)) && 
        property_compare(addr, PROPERTY_NODE_SHORT, 0, &sht, 2ul));
    if (!addr)
    {
        LogError("node(addr:0x%04x) not found.", sht);
        return;
    }

    if (ret < 0)
    {
        if (property_bit_set(&addr, PROPERTY_NODE_ONLINE));
            build_mission_entity_dirty(&addr);
        return;
    }
    
    property_get(addr, PROPERTY_ENTITY_ID, 0, &entity, 2ul);

    addr = 0;
    while ((addr = entity_find(ENTITY_TYPE_DEVICE, addr)) &&
        property_compare(addr, PROPERTY_DEVICE_NODE, 0, &entity, 2ul) &&
        property_compare(addr, PROPERTY_DEVICE_ZIGBEE_ENDPOINT, 0, &ep, 1ul));
    property_get(addr, PROPERTY_ENTITY_ID, 0, &entity, 2ul);

    //attr = (((uint16_t)attrs[0]) << 8) | attrs[1];

    if (attrs[2])
        return;

    if (ZCL_DATATYPE_BOOLEAN != attrs[3])
        return;

    addr = 0;
    while ((addr = entity_find(ENTITY_TYPE_VALUE, addr)) && 
        property_compare(addr, PROPERTY_VALUE_DEVICE, 0, &entity, 2ul));
    if (addr)
    {
        uint8_t ret = 0;
        if (attrs[4])
            ret = property_bit_set(&addr, PROPERTY_VALUE_ON_OFF);
        else
            ret = property_bit_clear(&addr, PROPERTY_VALUE_ON_OFF);
        if (ret)
            build_mission_entity_dirty(&addr);
    }
}

static void response_sync_simple_desc(int ret, 
    uint16_t sht, uint8_t ep, 
    uint16_t profile, uint16_t type, 
    uint8_t version, 
    uint8_t *ins, uint8_t in_count, 
    uint8_t *outs, uint8_t out_count)
{
    uint16_t i = 0;
    uint16_t category;
    uint16_t entity = 0;
    uint32_t addr = 0;

    LogAction("ret:%d short:0x%04x ep:%u profile:0x%04x type:%04x", ret, sht, ep, profile, type);

    while ((addr = entity_find(ENTITY_TYPE_NODE, addr)) && 
        property_compare(addr, PROPERTY_NODE_SHORT, 0, &sht, 2ul));
    if (!addr)
    {
        LogError("node(addr:%04x) not found", sht);
        return;
    }

    if (ret < 0)
    {
        if (property_bit_set(&addr, PROPERTY_NODE_ONLINE))
            build_mission_entity_dirty(&addr);
        return;
    }

    property_get(addr, PROPERTY_ENTITY_ID, 0, &entity, 2ul);

    addr = 0;
    while ((addr = entity_find(ENTITY_TYPE_DEVICE, addr)) &&
        property_compare(addr, PROPERTY_DEVICE_NODE, 0, &entity, 2ul) &&
        property_compare(addr, PROPERTY_DEVICE_ZIGBEE_ENDPOINT, 0, &ep, 1ul));
    if (!addr)
    {
        LogError("device(node:0x%04x device:0x%02x) not found.", entity, ep);
        return;
    }
    category = 0;
    if (ZCL_HA_PROFILE_ID == profile)
    {
        if (ZCL_HA_DEVICEID_ON_OFF_LIGHT == type)
        {
            category = DEVICE_CATEGORY_LIGHT;
        }
    }
    if (category)
        ret += property_set(&addr, PROPERTY_DEVICE_CATEGORY, PROPERTY_TYPE_INT16, (void *)&category, 2ul);
    else
        LogError("unknown device category.(addr:0x%04x ep:%u profile:0x%04x device:0x%04x)", sht, ep, profile, type);

    ret += property_set(&addr, PROPERTY_DEVICE_ZIGBEE_PROFILE, PROPERTY_TYPE_INT8, (void *)&profile, 1ul);
    ret += property_set(&addr, PROPERTY_DEVICE_ZIGBEE_DEVICE, PROPERTY_TYPE_INT8, (void *)&type, 1ul);
    ret += property_set(&addr, PROPERTY_DEVICE_ZIGBEE_CLUSTERS_IN, PROPERTY_TYPE_BUFFER, ins, in_count * 2ul);

    property_get(addr, PROPERTY_ENTITY_ID, 0, &entity, 2ul);
    for (i = 0; i < in_count; i++)
    {
        if (*(((uint16_t *)ins) + i) == ZCL_CLUSTER_ID_GEN_ON_OFF)
        {
            build_mission_sync_device_value(entity);
            break;
        }
        else if (*(((uint16_t *)ins) + i) == ZCL_CLUSTER_ID_GEN_LEVEL_CONTROL)
        {
            build_mission_sync_device_value(entity);
            break;
        }
    }
    ret += property_set(&addr, PROPERTY_DEVICE_ZIGBEE_CLUSTERS_OUT, PROPERTY_TYPE_BUFFER, outs, out_count * 2);
    if (ret)
        build_mission_entity_dirty(&addr);
}

static void response_slave_apply_scene_off(int ret, void *ctx, uint16_t sht, uint8_t ep, uint16_t cluster, uint8_t command, uint8_t *data, uint8_t size)
{
    uint8_t *buffer = (uint8_t *)ctx;

    if (ret < 0)
    {
        uint32_t addr = entity_get(*(uint16_t *)(buffer + 1));
        if (addr)
            property_bit_set(&addr, PROPERTY_SLAVE_MANUAL);
    }
}

static void response_slave_apply_scene_on(int ret, void *ctx, uint16_t sht, uint8_t ep, uint16_t cluster, uint8_t command, uint8_t *data, uint8_t size)
{
    uint8_t *buffer = (uint8_t *)ctx;

    if (ret < 0)
    {
        uint32_t addr = entity_get(*(uint16_t *)(buffer + 1));
        if (addr)
            property_bit_set(&addr, PROPERTY_SLAVE_MANUAL);
        return;
    }
    
    if (buffer[0])
    {
        ctx = zigbee_zcl_command(
            sht, ep, 
            ZCL_CLUSTER_ID_GEN_SCENES, COMMAND_SCENE_ADD, 
            1,
            buffer + 1 + 2, buffer[0], 
            response_slave_apply_scene_off, 1 + 2);
        if (!ctx)
        {
            uint32_t addr = entity_get(*(uint16_t *)(buffer + 1));
            if (addr)
                property_bit_set(&addr, PROPERTY_SLAVE_MANUAL);
            return;
        }
        *(uint8_t *)ctx = 0;
        os_memcpy(((uint8_t *)ctx) + 1, buffer + 1, 2ul);
    }
    else
    {
        response_slave_apply_scene_off(ret, ctx, sht, ep, cluster, command, data, size);
    }
}

typedef struct
{
    uint16_t group;
    uint8_t scene;
}ContextMasterApplyGroup;

static void response_slave_apply_group(int ret, void *ctx, uint16_t sht, uint8_t ep, uint16_t cluster, uint8_t command, uint8_t *data, uint8_t size)
{
    uint8_t *buffer = (uint8_t *)ctx;
    if (ret < 0)
    {
        uint32_t addr = entity_get(*(uint16_t *)(buffer + 1));
        if (addr)
            property_bit_set(&addr, PROPERTY_SLAVE_MANUAL);
        return;
    }
    
    if (buffer[0])
    {
        ctx = zigbee_zcl_command(
            sht, ep, 
            ZCL_CLUSTER_ID_GEN_SCENES, COMMAND_SCENE_ADD, 
            1,
            buffer + 1 + 2, buffer[0],
            response_slave_apply_scene_on, 1 + 2 + buffer[1 + 2 + buffer[0]]);
        if (!ctx)
        {
            uint32_t addr = entity_get(*(uint16_t *)(buffer + 1));
            if (addr)
                property_bit_set(&addr, PROPERTY_SLAVE_MANUAL);
            return;
        }
        os_memcpy(ctx, buffer + 1 + 2 + buffer[0], 1 + 2 + buffer[1 + 2 + buffer[0]]);
    }
    else if (buffer[1 + 2])
    {
        buffer += 3;

        ctx = zigbee_zcl_command(
            sht, ep, 
            ZCL_CLUSTER_ID_GEN_SCENES, COMMAND_SCENE_ADD, 
            1,
            buffer + 1 + 2, buffer[0], 
            response_slave_apply_scene_off, 1 + 2);
        if (!ctx)
        {
            uint32_t addr = entity_get(*(uint16_t *)(buffer + 1));
            if (addr)
                property_bit_set(&addr, PROPERTY_SLAVE_MANUAL);
            return;
        }
        *(uint8_t *)ctx = 0;
        os_memcpy(((uint8_t *)ctx) + 1, buffer + 1, 2ul);
    }
    else
    {
        uint32_t addr = entity_get(*(uint16_t *)(buffer + 1));
        if (addr)
            property_bit_set(&addr, PROPERTY_SLAVE_MANUAL);
    }
}

static void mission_master_status(Mission *mission)
{
    MissionMasterStatus *m = (MissionMasterStatus *)mission;

    if (0 == m->state)
    {
        m->ap = 0;
        m->addr = entity_find(ENTITY_TYPE_SLAVE, m->addr);
        if (!m->addr)
        {
            m->state = 2;
            return;
        }
        if (property_compare(m->addr, PROPERTY_SLAVE_MASTER, 0, &m->master, 2ul))
            return;
        property_get(m->addr, PROPERTY_SLAVE_DEVICE, 0, &m->device, 2ul);
        m->state = 1;
    }
    else if (1 == m->state)
    {
        uint8_t value = 0;
        m->ap = entity_find(ENTITY_TYPE_VALUE, m->ap);
        if (!m->ap)
        {
            m->state = 0;
            return;
        }
        if (property_compare(m->ap, PROPERTY_VALUE_DEVICE, 0, &m->device, 2ul))
            return;
        
        if (property_compare(m->ap, PROPERTY_VALUE_ON_OFF, 0, &value, 1ul) > 0)
        {
            m->state = 2;
            return;
        }
        
        if (property_compare(m->ap, PROPERTY_VALUE_LEVEL, 0, &value, 1ul) > 0)
        {
            m->state = 2;
            return;
        }
    }
    else if (2 == m->state)
    {
        m->link[MAX_LINK] = !!m->ap;
        cluster_send(cluster_id(), 0, OPCODE_GANGER_SYNC, 0, 0, m->link, MAX_LINK + 1, 0);

        if (!m->addr)
            m->state = 3;
        else
            m->state = 4;
    }
    else if (3 == m->state)
    {
        m->addr = 0;
        m->ap = entity_find(ENTITY_TYPE_GANGER, m->ap);
        if (!m->ap)
        {
            m->state = 4;
            return;
        }
        if (!property_compare(m->ap, PROPERTY_GANGER_LINK, 0, m->link, MAX_LINK))
        {
            m->state = 4;
            return;
        }
    }
    else if (4 == m->state)
    {
        uint8_t ep;
        uint16_t i;
        uint16_t count;
        uint16_t sht;
        uint16_t entity;
        uint32_t addr;

        if (!zigbee_idle())
            return;

        m->addr = entity_find(ENTITY_TYPE_DEVICE, m->addr);
        if (!m->addr)
        {
            mission_remove(mission);
            return;
        }
        if (property_compare(m->addr, PROPERTY_DEVICE_LINK, 0, m->link, MAX_LINK))
            return;
        
        count = property_size(m->addr, PROPERTY_DEVICE_ZIGBEE_CLUSTERS_IN) / 2;
        for (i = 0; i < count; i++)
        {
            uint16_t cluster;
            property_get(m->addr, PROPERTY_DEVICE_ZIGBEE_CLUSTERS_IN, i * 2, &cluster, 2ul);

            if (ZCL_CLUSTER_ID_GEN_ON_OFF == cluster)
                break;
        }
        if (i >= count)
            return;
        
        property_get(m->addr, PROPERTY_DEVICE_NODE, 0, &entity, 2ul);
        property_get(m->addr, PROPERTY_DEVICE_ZIGBEE_ENDPOINT, 0, &ep, 1ul);

        addr = entity_get(entity);
        if (!addr)
            return;
        property_get(addr, PROPERTY_NODE_SHORT, 0, &sht, 2ul);
            
        zigbee_zcl_command(sht, ep, ZCL_CLUSTER_ID_GEN_ON_OFF, m->ap ? COMMAND_ON : COMMAND_OFF, 1, 0, 0, 0, 0);

        property_get(m->addr, PROPERTY_ENTITY_ID, 0, &entity, 2ul);

        addr = 0;
        while ((addr = entity_find(ENTITY_TYPE_VALUE, addr)) && 
            property_compare(addr, PROPERTY_VALUE_DEVICE, 0, &entity, 2ul));
        if (addr)
        {
            uint8_t ret = 0;
            if (m->ap)
                ret = property_bit_set(&addr, PROPERTY_VALUE_ON_OFF);
            else
                ret = property_bit_clear(&addr, PROPERTY_VALUE_ON_OFF);
            if (ret)
                build_mission_entity_dirty(&addr);
        }
    }
}

static void mission_master_manual(Mission *mission)
{
    uint8_t cmd;
    uint16_t length;
    uint16_t device;
    MissionMasterManual *m = (MissionMasterManual *)mission;

    if (!zigbee_idle())
        return;

    m->addr = entity_find(ENTITY_TYPE_SLAVE, m->addr);
    if (!m->addr)
    {
        mission_remove(mission);
        return;
    }
    if (property_compare(m->addr, PROPERTY_SLAVE_MASTER, 0, &m->master, 2ul))
        return;
    if (!property_bit_get(m->addr, PROPERTY_SLAVE_MANUAL))
        return;

    property_get(m->addr, PROPERTY_SLAVE_DEVICE, 0, &device, 2ul);
    
    property_get(m->addr, PROPERTY_SLAVE_COMMAND, 0, &cmd, 1ul);

    if (ENTITY_COMMAND_DEVICE_OPERATE_ONOFF == cmd)
    {
        uint8_t command[2] = {0};
        property_get(m->addr, PROPERTY_SLAVE_COMMAND, 0, command, 2ul);

        if (m->command)
        {
            command_execute(device, ENTITY_TYPE_DEVICE, command, 2, 0);
        }
        else
        {
            command[1] = 0;
            command_execute(device, ENTITY_TYPE_DEVICE, command, 2, 0);
        }
    }
    else if (ENTITY_COMMAND_DEVICE_OPERATE_LEVEL == cmd)
    {
        uint8_t command[2] = {0};
        property_get(m->addr, PROPERTY_SLAVE_COMMAND, 0, command, 2ul);

        if (m->command)
        {
            command_execute(device, ENTITY_TYPE_DEVICE, command, 2, 0);
        }
        else
        {
            command[1] = 0;
            command_execute(device, ENTITY_TYPE_DEVICE, command, 2, 0);
        }
    }
}

static void mission_link_execute(Mission *mission)
{
    uint8_t type;
    uint16_t entity;

    MissionLinkExecute *m = (MissionLinkExecute *)mission;

    if (!zigbee_idle())
        return;

    m->addr = entity_iterator(m->addr);
    if (!m->addr)
    {
        mission_remove(mission);
        return;
    }

    property_get(m->addr, PROPERTY_ENTITY_TYPE, 0, &type, 1ul);
    property_get(m->addr, PROPERTY_ENTITY_ID, 0, &entity, 2ul);

    if (entity == m->source)
        return;

    if (ENTITY_TYPE_MASTER == type)
    {
        if (property_compare(m->addr, PROPERTY_MASTER_LINK, 0, m->link, MAX_LINK))
            return;
    }
    else if (ENTITY_TYPE_DEVICE == type)
    {
        if (property_compare(m->addr, PROPERTY_DEVICE_LINK, 0, m->link, MAX_LINK))
            return;
    }
    else
    {
        return;
    }

    command_execute(entity, type, m->command, m->size, m->link);
}

static void mission_slave_apply(Mission *mission)
{
    uint16_t master;
    uint16_t slave;
    uint16_t entity;
    uint8_t scene_on = 0;
    uint8_t scene_off = 0;
    uint8_t cmd;
    uint8_t ep;
    uint16_t sht;
    uint32_t as = 0;
    uint32_t ap = 0;

    slave = ((MissionSlaveApply *)mission)->slave;
    mission_remove(mission);

    as = entity_get(slave);
    if (!as)
    {
        mission_remove(mission);
        return;
    }
    property_get(as, PROPERTY_SLAVE_DEVICE, 0, &entity, 2ul);
    property_get(as, PROPERTY_SLAVE_COMMAND, 0, &cmd, 1ul);
    property_get(as, PROPERTY_SLAVE_MASTER, 0, &master, 2ul);

    ap = entity_get(master);
    if (!ap)
    {
        mission_remove(mission);
        return;
    }
    property_get(ap, PROPERTY_MASTER_SCENE_ON, 0, &scene_on, 1ul);
    property_get(ap, PROPERTY_MASTER_SCENE_OFF, 0, &scene_off, 1ul);

    ap = entity_get(entity);//device
    if (!ap)
    {
        mission_remove(mission);
        return;
    }
    property_get(ap, PROPERTY_DEVICE_ZIGBEE_ENDPOINT, 0, &ep, 1ul);
    property_get(ap, PROPERTY_DEVICE_NODE, 0, &entity, 2ul);

    ap = entity_get(entity);//node
    if (!ap)
    {
        mission_remove(mission);
        return;
    }
    property_get(ap, PROPERTY_NODE_SHORT, 0, &sht, 2ul);

    if (ENTITY_COMMAND_DEVICE_OPERATE_ONOFF == cmd || 
        ENTITY_COMMAND_DEVICE_OPERATE_LEVEL == cmd)
    {
        uint8_t *ctx = 0;
        uint8_t buffer[3] = {0};
        *(uint16_t *)buffer = master;
        ctx = (uint8_t *)zigbee_zcl_command(
            sht, ep, 
            ZCL_CLUSTER_ID_GEN_GROUPS, COMMAND_GROUP_ADD, 
            1,
            buffer, 3, 
            response_slave_apply_group, (scene_on ? (1 + 2 + 2 + 1 + 2 + 1 + 2 + 1 + 1) : (1 + 2)) + (scene_off ? (1 + 2 + 2 + 1 + 2 + 1 + 2 + 1 + 1) : (1 + 2)));
        if (ctx)
        {
            if (!scene_on)
            {
                *ctx++ = 0;
                
                *(uint16_t *)ctx = slave;
                ctx += 2;
            }
            else
            {
                *ctx++ = 2 + 1 + 2 + 1 + 2 + 1 + 1;

                *(uint16_t *)ctx = slave;
                ctx += 2;

                *(uint16_t *)ctx = *(uint16_t *)buffer;
                ctx += 2;

                *ctx++ = scene_on;

                property_get(as, PROPERTY_SLAVE_TRANSITION, 0, ctx, 2ul);//transition time
                ctx += 2;

                *ctx++ = 0;//scene name length

                if (ENTITY_COMMAND_DEVICE_OPERATE_ONOFF == cmd)
                    *(uint16_t *)ctx = ZCL_CLUSTER_ID_GEN_ON_OFF;
                else if (ENTITY_COMMAND_DEVICE_OPERATE_LEVEL == cmd)
                    *(uint16_t *)ctx = ZCL_CLUSTER_ID_GEN_LEVEL_CONTROL;
                ctx += 2;

                *ctx++ = 1;

                property_get(as, PROPERTY_SLAVE_COMMAND, 1, ctx++, 1ul);
            }
            
            if (!scene_off)
            {
                *ctx = 0;
                
                *(uint16_t *)ctx = slave;
                ctx += 2;
            }
            else
            {
                *ctx++ = 2 + 1 + 2 + 1 + 2 + 1 + 1;

                *(uint16_t *)ctx = slave;
                ctx += 2;

                *(uint16_t *)ctx = *(uint16_t *)buffer;
                ctx += 2;
                
                *ctx++ = scene_off;

                *(uint16_t *)ctx = 0;//transition time
                ctx += 2;

                *ctx++ = 0;//scene name length

                if (ENTITY_COMMAND_DEVICE_OPERATE_ONOFF == cmd)
                    *(uint16_t *)ctx = ZCL_CLUSTER_ID_GEN_ON_OFF;
                else if (ENTITY_COMMAND_DEVICE_OPERATE_LEVEL == cmd)
                    *(uint16_t *)ctx = ZCL_CLUSTER_ID_GEN_LEVEL_CONTROL;
                ctx += 2;

                *ctx++ = 1;

                *ctx++ = 0;
            }
        }
    }
}

static void simple_desc_update(uint8_t type)
{
    uint8_t ep;
    uint16_t sht;
    uint32_t addr;

    if (!zigbee_idle())
        return;

    if (!_addr)
        return;

    if (ENTITY_TYPE_DEVICE != type)
        return;

    if (!property_bit_get(_addr, PROPERTY_DEVICE_SIMPLE_DESC))
        return;
    property_bit_clear(&_addr, PROPERTY_DEVICE_SIMPLE_DESC);

    property_get(_addr, PROPERTY_DEVICE_ZIGBEE_ENDPOINT, 0, &ep, 1ul);
    property_get(_addr, PROPERTY_DEVICE_NODE, 0, &sht, 2ul);

    addr = entity_get(sht);
    if (!addr)
        return;
    property_get(addr, PROPERTY_NODE_SHORT, 0, &sht, 2ul);
    
    zigbee_simple_desc(sht, ep, response_sync_simple_desc);
}

static void sync_value_update(uint8_t i)
{
    if (0 == _sync_value.state)
    {
        uint16_t count = 0;
        uint16_t device = 0;
        uint16_t node = 0;
        uint32_t addr = 0;

        if (!_addr)
            return;
        if (ENTITY_TYPE_VALUE != i)
            return;
        if (!property_bit_get(_addr, PROPERTY_VALUE_SYNC))
            return;
        property_bit_clear(&_addr, PROPERTY_VALUE_SYNC);

        property_get(_addr, PROPERTY_VALUE_DEVICE, 0, &device, 2ul);

        addr = entity_get(device);
        if (!addr)
            return;
        property_get(addr, PROPERTY_DEVICE_NODE, 0, &node, 2ul);
        property_get(addr, PROPERTY_DEVICE_ZIGBEE_ENDPOINT, 0, &_sync_value.ep, 1ul);

        _sync_value.state = 0;
        count = property_size(addr, PROPERTY_DEVICE_ZIGBEE_CLUSTERS_IN) / 2;
        for (i = 0; i < count; i++)
        {
            uint16_t cluster;
            property_get(_addr, PROPERTY_DEVICE_ZIGBEE_CLUSTERS_IN, i * 2, &cluster, 2ul);

            if (ZCL_CLUSTER_ID_GEN_ON_OFF == cluster)
                _sync_value.state |= 1;
            else if (ZCL_CLUSTER_ID_GEN_LEVEL_CONTROL == cluster)
                _sync_value.state |= 2;
        }
        
        addr = entity_get(node);
        if (!addr)
        {
            _sync_value.state = 0;
            return;
        }
        property_get(addr, PROPERTY_NODE_SHORT, 0, &_sync_value.sht, 2ul);
    }
    else if (1 == _sync_value.state)
    {
        if (!zigbee_idle())
            return;

        _sync_value.state = 0;

        zigbee_attribute_read(
            _sync_value.sht, 
            _sync_value.ep, 
            ZCL_CLUSTER_ID_GEN_ON_OFF, 
            ATTRID_ON_OFF, 
            response_sync_value_on_off);
    }
    else if (2 == _sync_value.state)
    {
        if (!zigbee_idle())
            return;

        _sync_value.state = 0;

        zigbee_attribute_read(
            _sync_value.sht, 
            _sync_value.ep, 
            ZCL_CLUSTER_ID_GEN_LEVEL_CONTROL, 
            ATTRID_LEVEL_CURRENT_LEVEL, 
            response_sync_value_level);
    }
    else if (3 == _sync_value.state)
    {
        if (!zigbee_idle())
            return;

        _sync_value.state = 2;

        zigbee_attribute_read(
            _sync_value.sht, 
            _sync_value.ep, 
            ZCL_CLUSTER_ID_GEN_ON_OFF, 
            ATTRID_ON_OFF, 
            response_sync_value_on_off);
    }
}

static void dirty_update(uint8_t type)
{
    uint16_t entity;

    if (!_addr)
        return;

    if (!property_bit_get(_addr, PROPERTY_ENTITY_DIRTY))
        return;
    property_bit_clear(&_addr, PROPERTY_ENTITY_DIRTY);

    property_get(_addr, PROPERTY_ENTITY_ID, 0, &entity, 2ul);

    build_mission_entity_update(cluster_id(), 0, 0, _addr, entity);
}

static void schedule_update(uint8_t t)
{
    uint8_t link[MAX_LINK] = {0};
    uint8_t *cmd = 0;
    uint16_t length = 0;
    uint16_t owner;
    uint32_t addr;
    time_t now = time(0);
    struct tm *d = localtime(&now);

    if (ENTITY_TYPE_SCHEDULE != t)
        return;

    if (!_addr)
    {
        _schedule.timer = os_ticks();
        return;
    }

    if (_schedule.timer && os_ticks_from(_schedule.timer) < os_ticks_ms(35000))
        return;

    property_get(_addr, PROPERTY_SCHEDULE_YEAR, 0, &owner, 2ul);
    if (owner && d->tm_year != owner)
        return;
    property_get(_addr, PROPERTY_SCHEDULE_MONTH, 0, &t, 1ul);
    if (t && (t != (d->tm_mon + 1)))
        return;
    property_get(_addr, PROPERTY_SCHEDULE_DAY, 0, &t, 1ul);
    if (t && (t != d->tm_mday))
        return;
    property_get(_addr, PROPERTY_SCHEDULE_WEEK, 0, &t, 1ul);
    if (t && (t & d->tm_wday))
        return;
    property_get(_addr, PROPERTY_SCHEDULE_HOUR, 0, &t, 1ul);
    if (t && (t != (d->tm_hour + 1)))
        return;
    property_get(_addr, PROPERTY_SCHEDULE_MINUTE, 0, &t, 1ul);
    if (t && (t != (d->tm_min + 1)))
        return;
    property_get(_addr, PROPERTY_SCHEDULE_LINK, 0, link, MAX_LINK);

    length = property_size(_addr, PROPERTY_SCHEDULE_COMMAND);
    cmd = (uint8_t *)os_malloc(length);
    if (!cmd)
        return;
    property_get(_addr, PROPERTY_SCHEDULE_COMMAND, 2, cmd, length);

    addr = 0;
    while ((addr = entity_iterator(addr)))
    {
        property_get(addr, PROPERTY_ENTITY_TYPE, 0, &t, 1ul);

        if ((ENTITY_TYPE_DEVICE == t && !property_compare(addr, PROPERTY_DEVICE_LINK, 0, link, MAX_LINK)) ||
            (ENTITY_TYPE_MASTER == t && !property_compare(addr, PROPERTY_MASTER_LINK, 0, link, MAX_LINK)))
        {
            property_get(addr, PROPERTY_ENTITY_ID, 0, &owner, 2ul);

            command_execute(owner, t, cmd, length, link);
        }
    }

    os_free(cmd);
}

void mission_init(void)
{
    uint8_t dhcp;
    uint32_t reset = 0;
    uint32_t addr = entity_find(ENTITY_TYPE_GATEWAY, 0);
    if (!addr)
        addr = entity_create(ENTITY_TYPE_GATEWAY, 1ul + 1ul + 1ul + (1ul + 1ul + 4ul) * 5 + 1ul + 1ul + 6ul);
    if (addr)
    {
        property_get(addr, PROPERTY_GATEWAY_RESET, 0, &reset, 1ul);
        reset = (reset << 1) | 1;
        if (0xff == reset)
            reset = 0;
        property_set(&addr, PROPERTY_GATEWAY_RESET, PROPERTY_TYPE_INT8, &reset, 1ul);

        dhcp = dhcp_enabled();
        property_set(&addr, PROPERTY_GATEWAY_DHCP, PROPERTY_TYPE_INT8, &dhcp, 1ul);
        property_set(&addr, PROPERTY_GATEWAY_IP, PROPERTY_TYPE_INT32, network_net_ip(), 4ul);
        property_set(&addr, PROPERTY_GATEWAY_MASK, PROPERTY_TYPE_INT32, network_net_mask(), 4ul);
        property_set(&addr, PROPERTY_GATEWAY_PRIDNS, PROPERTY_TYPE_INT32, network_net_dns_pri(), 4ul);
        property_set(&addr, PROPERTY_GATEWAY_SECDNS, PROPERTY_TYPE_INT32, network_net_dns_sec(), 4ul);
        property_set(&addr, PROPERTY_GATEWAY_MAC, PROPERTY_TYPE_INT48, network_net_mac(), 6ul);
    }

    addr = 0;
    while ((addr = entity_iterator(addr)))
    {
        uint8_t type;
        property_get(addr, PROPERTY_ENTITY_TYPE, 0, &type, 1ul);
        if (ENTITY_TYPE_GANGER == type)
            entity_release(addr);
    }
}

void mission_update(void)
{
    uint8_t type = 0;
    Mission *current = 0;

    current = _mission;
    while (current)
    {
        Mission *mission = current;
        current = current->next;

        if (MISSION_TYPE_MASTER_STATUS == mission->type)
            mission_master_status(mission);
        else if (MISSION_TYPE_MASTER_MANUAL == mission->type)
            mission_master_manual(mission);
        else if (MISSION_TYPE_LINK_EXECUTE == mission->type)
            mission_link_execute(mission);
        else if (MISSION_TYPE_SLAVE_APPLY == mission->type)
            mission_slave_apply(mission);
        else
            mission_remove(mission);
    }

    _addr = entity_iterator(_addr);
    if (_addr)
        property_get(_addr, PROPERTY_ENTITY_TYPE, 0, &type, 1ul);
    schedule_update(type);
    dirty_update(type);
    sync_value_update(type);
    simple_desc_update(type);
}

void build_mission_device_simple_desc(uint32_t *addr)
{
    /*
    if (!property_bit_get(*addr, PROPERTY_DEVICE_SIMPLE_DESC))
        property_bit_set(addr, PROPERTY_DEVICE_SIMPLE_DESC);
    */
}

void build_mission_sync_device_value(uint16_t device)
{
    /*
    uint32_t addr = 0;
    while ((addr = entity_find(ENTITY_TYPE_VALUE, addr)))
    {
        if (property_compare(addr, PROPERTY_VALUE_DEVICE, 0, &device, 2ul))
            continue;
        if (!property_bit_get(addr, PROPERTY_VALUE_SYNC))
            property_bit_set(&addr, PROPERTY_VALUE_SYNC);
    }
    */
}

void build_mission_link_execute(const uint8_t *link, uint16_t source, const uint8_t *command, uint16_t size)
{
    /*chenjing
    MissionLinkExecute *m = 0;

    Mission *mission = _mission;
    while (mission)
    {
        if (mission->type == MISSION_TYPE_LINK_EXECUTE && 
            !os_memcmp(((MissionLinkExecute *)mission)->link, link, MAX_LINK))
            break;
        mission = mission->next;
    }
    if (!mission)
        mission = mission_add(MISSION_TYPE_LINK_EXECUTE, sizeof(MissionLinkExecute) + size);
    if (!mission)
        return;

    m = (MissionLinkExecute *)mission;

    m->addr = 0;
    m->size = size;
    os_memcpy(m->link, link, MAX_LINK);
    os_memcpy(m->command, command, size);
    
    if (source)
    {
        uint8_t *data;
        m->source = source;

        cluster_send(cluster_id(), 0, OPCODE_ENTITY_LINK, 0, 0, m->link, MAX_LINK + size, 0);
    }
    */
}

void build_mission_master_manual(uint16_t master, uint8_t command, const uint8_t *link)
{
    /*
    Mission *mission = 0;
    while (mission)
    {
        if (mission->type == MISSION_TYPE_MASTER_MANUAL &&
            master == ((MissionMasterManual *)mission)->master)
            break;
        mission = mission->next;
    }
    if (!mission)
        mission = mission_add(MISSION_TYPE_MASTER_MANUAL, sizeof(MissionMasterManual) + MAX_LINK);
    if (!mission)
        return;

    ((MissionMasterManual *)mission)->master = master;
    ((MissionMasterManual *)mission)->command = command;
    ((MissionMasterManual *)mission)->addr = 0;
    ((MissionMasterManual *)mission)->link = 0;
    if (link)
    {
        ((MissionMasterManual *)mission)->link = ((MissionMasterManual *)mission)->link_payload;
        os_memcpy(((MissionMasterManual *)mission)->link_payload, link, MAX_LINK);
    }
    */
}

void build_mission_master_status(uint32_t addr)
{
    /*
    Mission *mission = 0;
    MissionMasterStatus *m = 0;
    uint16_t master;
    property_get(addr, PROPERTY_ENTITY_ID, 0, &master, 2ul);

    mission = _mission;
    while (mission)
    {
        if (mission->type == MISSION_TYPE_MASTER_STATUS && 
            ((MissionMasterStatus *)mission)->master == master)
            break;
        mission = mission->next;
    }
    if (!mission)
        mission = mission_add(MISSION_TYPE_MASTER_STATUS, sizeof(MissionMasterStatus));
    if (!mission)
        return;
    m = (MissionMasterStatus *)mission;

    m->master = master;
    property_get(addr, PROPERTY_MASTER_LINK, 0, m->link, MAX_LINK);
    m->addr = 0;
    m->device = 0;
    m->state = 0;
    m->ap = 0;
    */
}

void build_mission_device_related(uint16_t device)
{
    /* chenjing
    uint32_t addr = 0;
    while ((addr = entity_find(ENTITY_TYPE_SLAVE, addr)))
    {
        uint16_t master;
        uint32_t ap;
        if (property_compare(addr, PROPERTY_SLAVE_DEVICE, 0, &device, 2ul))
            continue;
        property_get(addr, PROPERTY_SLAVE_MASTER, 0, &master, 2ul);

        ap = entity_get(master);
        if (!ap)
            continue;
        if (property_exist(ap, PROPERTY_MASTER_SCENE_OFF))
            build_mission_master_status(ap);
    }
    */
}

void build_mission_terminal_update(void)
{
    /* chenjing
    uint16_t entity = 0;
    uint32_t addr = 0;
    uint8_t data[1 + 4] = {0};
    data[0] = TERMINAL_TYPE_GATEWAY;

    if (!os_memcmp(cluster_id(), local_addr(), MAX_CLUSTER_ID))
        return;

    addr = entity_find(ENTITY_TYPE_GATEWAY, 0ul);
    if (addr)
        property_get(addr, PROPERTY_GATEWAY_RESET, 0, data + 1, 4ul);
    *(uint32_t *)(data + 1) = network_htonl(*(uint32_t *)(data + 1));
    
    cluster_send(cluster_id(), 0, OPCODE_TERMINAL_UPDATE, 0, 0, data, 1 + 4, 0);
    */
}

void build_mission_entity_update(const uint8_t *cluster, const uint8_t *terminal, uint8_t session, uint32_t addr, uint8_t entity)
{
    /* chenjing
    uint8_t opt[MAX_TERMINAL_ID + 1] = {0};
    uint8_t *data = 0;

    if (terminal)
        os_memcpy(opt, terminal, MAX_TERMINAL_ID);
    else
        os_memset(opt, 0xff, MAX_TERMINAL_ID);
    opt[MAX_TERMINAL_ID] = session;

    if (!addr)
    {
        data = (uint8_t *)os_malloc((uint32_t)(4 + 1 + 2));
        if (!data)
            return;
        *(uint32_t *)data = network_htonl((uint32_t)0);
        data[4] = 0;
        *(uint16_t *)(data + 5) = network_htons(entity);
        cluster_send(cluster, 0, OPCODE_ENTITY_UPDATE, opt, MAX_TERMINAL_ID + 1, data, 4 + 1 + 2, 0);
    }
    else
    {
        uint32_t size = entity_size(addr);
        data = (uint8_t *)os_malloc(4ul + size);
        if (!data)
            return;
        *(uint32_t *)data = network_htonl(addr);
        entity_pack(addr, data + 4, size);
        cluster_send(cluster, 0, OPCODE_ENTITY_UPDATE, opt, MAX_TERMINAL_ID + 1, data, 4 + size, 0);
    }
    if (data)
        os_free(data);
    */
}

void build_mission_entity_dirty(uint32_t *addr)
{
    /* chenjing
    Mission *mission = 0;
    if (property_bit_get(*addr, PROPERTY_ENTITY_DIRTY))
        return;
    property_bit_set(addr, PROPERTY_ENTITY_DIRTY);
    */
}

void build_mission_slave_apply(uint16_t entity)
{
    /* chenjing
    Mission *mission = _mission;
    while (mission)
    {
        if (mission->type == MISSION_TYPE_SLAVE_APPLY && ((MissionSlaveApply *)mission)->slave == entity)
            break;
        mission = mission->next;
    }
    if (!mission)
        mission = mission_add(MISSION_TYPE_SLAVE_APPLY, sizeof(MissionSlaveApply));
    if (!mission)
        return;
    ((MissionSlaveApply *)mission)->slave = entity;
    */
}
