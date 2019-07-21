#include "command.h"
#include "zigbee.h"
#include "block.h"
#include "entity.h"
#include "entity_type.h"
#include "mission.h"
#include "interface_os.h"
#include "log.h"

static void device_command_do(uint16_t entity, const uint8_t *command, uint32_t size, const uint8_t *link)
{
    uint8_t ep;
    uint16_t i;
    uint16_t count;
    uint16_t sht;
    uint8_t dl[MAX_LINK] = {0};
    uint32_t av = 0;
    uint32_t ap = entity_get(entity);
    property_get(ap, PROPERTY_DEVICE_ZIGBEE_ENDPOINT, 0, &ep, 1UL);
    property_get(ap, PROPERTY_DEVICE_NODE, 0, &sht, 2UL);
    av = entity_get(sht);
    if (!av)
    {
        LogError("node %d not found.");
        return;
    }
    property_get(av, PROPERTY_NODE_SHORT, 0, &sht, 2UL);

    av = 0;
    while ((av = entity_find(ENTITY_TYPE_VALUE, av)))
    {
        if (!property_compare(av, PROPERTY_VALUE_DEVICE, 0, &entity, 2ul))
            break;
    }
    if (!av)
    {
        av = entity_create(ENTITY_TYPE_VALUE, (uint32_t)(1 + 1 + 2 + 1 + 1 + 1));
        if (av)
            property_set(&av, PROPERTY_VALUE_DEVICE, PROPERTY_TYPE_INT16, &entity, 2ul);
    }

    count = property_size(ap, PROPERTY_DEVICE_ZIGBEE_CLUSTERS_IN) / 2;
    for (i = 0; i < count; i++)
    {
        uint16_t cluster;
        property_get(ap, PROPERTY_DEVICE_ZIGBEE_CLUSTERS_IN, i * 2, &cluster, 2ul);

        if (cluster == ZCL_CLUSTER_ID_GEN_ON_OFF && ENTITY_COMMAND_DEVICE_OPERATE_ONOFF == command[0])
        {
            zigbee_zcl_command(sht, ep, ZCL_CLUSTER_ID_GEN_ON_OFF, command[1] ? COMMAND_ON : COMMAND_OFF, 1, 0, 0, 0, 0);
            if (property_set(&av, PROPERTY_VALUE_ON_OFF, PROPERTY_TYPE_INT8, command + 1, 1ul))
                build_mission_entity_dirty(&av);
            break;
        }
        else if (cluster == ZCL_CLUSTER_ID_GEN_LEVEL_CONTROL && ENTITY_COMMAND_DEVICE_OPERATE_LEVEL == command[0])
        {
            uint8_t buffer[3] = {0};
            buffer[0] = command[1];
            zigbee_zcl_command(sht, ep, ZCL_CLUSTER_ID_GEN_LEVEL_CONTROL, COMMAND_LEVEL_MOVE_TO_LEVEL, 1, buffer, 3, 0, 0);
            if (property_set(&av, PROPERTY_VALUE_LEVEL, PROPERTY_TYPE_INT8, command + 1, 1ul))
                build_mission_entity_dirty(&av);
            break;
        }
    }
    if (i < count)
        build_mission_device_related(entity);
    
    if (link && !property_compare(ap, PROPERTY_DEVICE_LINK, 0, link, MAX_LINK))
        return;
    property_get(ap, PROPERTY_DEVICE_LINK, 0, dl, MAX_LINK);

    build_mission_link_execute(dl, entity, command, size);
}

static void master_command_do(uint16_t entity, const uint8_t *command, uint16_t size, const uint8_t *link)
{
    uint32_t addr = entity_get(entity);
    if (!addr)
        return;
    if (ENTITY_COMMAND_MASTER_SCENE_APPLY == command[0])
    {
        if (!property_exist(addr, PROPERTY_MASTER_SCENE_ON))
        {
            uint8_t scene = 0;
            uint32_t ap = 0;
            while ((ap = entity_find(ENTITY_TYPE_MASTER, ap)))
            {
                if (property_compare(ap, PROPERTY_MASTER_SCENE_ON, 0, &scene, 1ul) > 0)
                    property_get(ap, PROPERTY_MASTER_SCENE_ON, 0, &scene, 1ul);

                if (property_compare(ap, PROPERTY_MASTER_SCENE_OFF, 0, &scene, 1ul) > 0)
                    property_get(ap, PROPERTY_MASTER_SCENE_OFF, 0, &scene, 1ul);
            }
            scene++;
            if (property_set(&addr, PROPERTY_MASTER_SCENE_ON, PROPERTY_TYPE_INT8, &scene, 1ul))
                build_mission_entity_dirty(&addr);
        }
        property_release(&addr, PROPERTY_MASTER_SCENE_OFF);
    }
    else if (ENTITY_COMMAND_MASTER_ROOM_APPLY == command[0])
    {
        if (!property_exist(addr, PROPERTY_MASTER_SCENE_ON))
        {
            uint8_t scene = 0;
            uint32_t ap = 0;
            while ((ap = entity_find(ENTITY_TYPE_MASTER, ap)))
            {
                if (property_compare(ap, PROPERTY_MASTER_SCENE_ON, 0, &scene, 1ul) > 0)
                    property_get(ap, PROPERTY_MASTER_SCENE_ON, 0, &scene, 1ul);

                if (property_compare(ap, PROPERTY_MASTER_SCENE_OFF, 0, &scene, 1ul) > 0)
                    property_get(ap, PROPERTY_MASTER_SCENE_OFF, 0, &scene, 1ul);
            }
            scene++;
            if (property_set(&addr, PROPERTY_MASTER_SCENE_ON, PROPERTY_TYPE_INT8, &scene, 1ul))
                build_mission_entity_dirty(&addr);
        }
        if (!property_exist(addr, PROPERTY_MASTER_SCENE_OFF))
        {
            uint8_t scene = 0;
            uint32_t ap = 0;
            while ((ap = entity_find(ENTITY_TYPE_MASTER, ap)))
            {
                if (property_compare(ap, PROPERTY_MASTER_SCENE_ON, 0, &scene, 1ul) > 0)
                    property_get(ap, PROPERTY_MASTER_SCENE_ON, 0, &scene, 1ul);

                if (property_compare(ap, PROPERTY_MASTER_SCENE_OFF, 0, &scene, 1ul) > 0)
                    property_get(ap, PROPERTY_MASTER_SCENE_OFF, 0, &scene, 1ul);
            }
            scene++;
            if (property_set(&addr, PROPERTY_MASTER_SCENE_OFF, PROPERTY_TYPE_INT8, &scene, 1ul))
                build_mission_entity_dirty(&addr);
        }
    }
    else if (ENTITY_COMMAND_MASTER_OPERATE == command[0])
    {
        uint16_t slave;
        uint8_t buffer[3] = {0};
        *(uint16_t *)buffer = entity;
        property_get(addr, command[1] ? PROPERTY_MASTER_SCENE_ON : PROPERTY_MASTER_SCENE_OFF, 0, buffer + 2, 1ul);
        zigbee_zcl_command_mcast(entity, 0xff, ZCL_CLUSTER_ID_GEN_SCENES, COMMAND_SCENE_RECALL, 1, buffer, 3, 0, 0);

        build_mission_master_manual(entity, command[1], link);

        if (property_exist(addr, PROPERTY_MASTER_SCENE_OFF))
            build_mission_master_status(addr);

        addr = 0;
        while ((addr = entity_find(ENTITY_TYPE_SLAVE, addr)))
        {
            uint8_t *cmd = 0;
            uint16_t length = 0;
            uint8_t dl[MAX_LINK] = {0};
            uint16_t device;
            uint32_t ap = 0;
            if (property_compare(addr, PROPERTY_SLAVE_MASTER, 0, &entity, 2ul))
                continue;
            property_get(addr, PROPERTY_SLAVE_DEVICE, 0, &device, 2ul);

            ap = entity_get(device);
            if (!ap)
                continue;

            length = property_size(addr, PROPERTY_SLAVE_COMMAND);
            cmd = (uint8_t *)os_malloc(length);
            if (!cmd)
                continue;
            property_get(addr, PROPERTY_SLAVE_COMMAND, 0, cmd, length);

            if (ENTITY_COMMAND_DEVICE_OPERATE_ONOFF == cmd[0])
            {
                if (property_set(&ap, PROPERTY_VALUE_ON_OFF, PROPERTY_TYPE_INT8, cmd + 1, 1ul))
                {
                    build_mission_device_related(device);
                    build_mission_entity_dirty(&ap);
                }
            }
            else if (ENTITY_COMMAND_DEVICE_OPERATE_LEVEL == cmd[0])
            {
                if (property_set(&ap, PROPERTY_VALUE_LEVEL, PROPERTY_TYPE_INT8, cmd + 1, 1ul))
                {
                    build_mission_device_related(device);
                    build_mission_entity_dirty(&ap);
                }
            }

            if (property_exist(ap, PROPERTY_DEVICE_LINK))
            {
                if (!link || property_compare(ap, PROPERTY_DEVICE_LINK, 0, link, MAX_LINK))
                {
                    property_get(ap, PROPERTY_DEVICE_LINK, 0, dl, MAX_LINK);

                    build_mission_link_execute(dl, device, cmd, length);
                }
            }

            os_free(cmd);
        }
    }
}

static void slave_command_do(uint16_t entity, const uint8_t *command, uint16_t size, const uint8_t *link)
{
    uint32_t addr = entity_get(entity);
    if (!addr)
        return;

    if (ENTITY_COMMAND_DELETE == command[0])
    {
        uint8_t ep;
        uint16_t group;
        uint16_t value;
        uint16_t sht;
        
        property_get(addr, PROPERTY_SLAVE_MASTER, 0, &group, 2ul);
        property_get(addr, PROPERTY_SLAVE_DEVICE, 0, &value, 2ul);

        addr = entity_get(value);
        if (!addr)
            return;
        property_get(addr, PROPERTY_DEVICE_ZIGBEE_ENDPOINT, 0, &ep, 1ul);
        property_get(addr, PROPERTY_DEVICE_NODE, 0, &value, 2ul);

        addr = entity_get(value);
        if (!addr)
            return;
        property_get(addr, PROPERTY_NODE_SHORT, 0, &value, 2ul);
        
        zigbee_zcl_command(value, ep, ZCL_CLUSTER_ID_GEN_GROUPS, COMMAND_GROUP_REMOVE, 1, &group, 2, 0, 0);
    }
    else if (ENTITY_COMMAND_SLAVE_APPLY == command[0])
    {
        build_mission_slave_apply(entity);
    }
}

static void gateway_command_do(uint16_t entity, const uint8_t *command, uint16_t size, const uint8_t *link)
{
    uint32_t addr = entity_get(entity);
    if (!addr)
        return;

    if (ENTITY_COMMAND_GATEWAY_DISCOVER_START == command[0])
    {
        zigbee_discover_start();
    }
    else if (ENTITY_COMMAND_GATEWAY_DISCOVER_STOP == command[0])
    {
        zigbee_discover_stop();
    }
}

void command_execute(uint16_t entity, uint8_t type, const uint8_t *command, uint16_t size, const uint8_t *link)
{
    switch (type)
    {
        case ENTITY_TYPE_GATEWAY:
            gateway_command_do(entity, command, size, link);
            break;
        case ENTITY_TYPE_DEVICE:
            device_command_do(entity, command, size, link);
            break;
        case ENTITY_TYPE_MASTER:
            master_command_do(entity, command, size, link);
            break;
        case ENTITY_TYPE_SLAVE:
            slave_command_do(entity, command, size, link);
            break;
    }
}
