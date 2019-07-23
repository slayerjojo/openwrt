#include "core.h"
#include "endian.h"
#include "buddha_heap.h"
#include "mission.h"
#include "entity.h"
#include "zigbee.h"
#include "cluster.h"
#include "block.h"
#include "storage.h"
#include "log.h"
#include "command.h"
#include "opcode.h"
#include "interface_network.h"
#include "interface_os.h"
#include "driver_led.h"

#define DEVICE_SYNC_SIMPLE_DESC 0x01
#define DEVICE_SYNC_VALUE_ON_OFF 0x02
#define DEVICE_SYNC_VALUE_LEVEL 0x04
#define DEVICE_SYNC_FINISHED 0x07

static uint32_t _mission_timer = 0;
static uint32_t _mission_addr = 0;

static void response_active_ep(int ret, uint16_t node_addr, uint8_t *eps, uint8_t count)
{
    uint8_t i;
    uint16_t node = 0;
    uint32_t addr = 0;

    LogAction("ret:%d addr:0x%04x ep count:%u", ret, node_addr, count);

    if (ret < 0)
        return;
    addr = 0;
    while ((addr = entity_find(ENTITY_TYPE_NODE, addr)) && property_compare(addr, PROPERTY_NODE_SHORT, 0, &node_addr, 2ul));
    if (!addr)
    {
        LogError("node(addr:%04x) not found", node_addr);
        return;
    }
    property_get(addr, PROPERTY_ENTITY_ID, 0, &node, 2ul);

    addr = 0;
    while ((addr = entity_find(ENTITY_TYPE_DEVICE, addr)))
    {
        uint8_t ep;
        if (property_compare(addr, PROPERTY_DEVICE_NODE, 0, &node, 2ul))
            continue;
        property_get(addr, PROPERTY_DEVICE_ZIGBEE_ENDPOINT, 0, &ep, 1ul);
        for (i = 0; i < count; i++)
        {
            if (eps[i] == ep)
                break;
        }
        if (i >= count)
        {
            entity_release(addr);
            continue;
        }
        build_mission_device_simple_desc(&addr);
        eps[i] = 0;
    }
    for (i = 0; i < count; i++)
    {
        if (!eps[i])
            continue;
        addr = entity_create(ENTITY_TYPE_DEVICE, (uint32_t)(1 + 1 + 2 + 1 + 1 + 1));
        if (!addr)
            continue;
        ret = property_set(&addr, PROPERTY_DEVICE_NODE, PROPERTY_TYPE_INT16, &node, 2ul);
        ret += property_set(&addr, PROPERTY_DEVICE_ZIGBEE_ENDPOINT, PROPERTY_TYPE_INT8, eps + i, 1ul);
        if (ret)
            build_mission_entity_dirty(&addr);

        build_mission_device_simple_desc(&addr);
    }
}

static void zigbee_device_annce(ZigbeeZDOEvent *e)
{
    int ret = 0;
    uint8_t v;
    uint16_t entity = 0;
    uint32_t addr = 0;

    while ((addr = entity_find(ENTITY_TYPE_NODE, addr)) && property_compare(addr, PROPERTY_NODE_MAC, 0, e->payload.device_annouce.ieee, Z_EXTADDR_LEN));
    if (!addr)
    {
        addr = entity_create(ENTITY_TYPE_NODE, (uint32_t)(1 + 1 + 1 + 1 + 1 + 2 + 1 + 1 + 8 + 1 + 1 + 1 + 1 + 1 + 1));
        ret = 1;
    }
    v = NODE_PROFILE_ZIGBEE;
    ret += property_set(&addr, PROPERTY_NODE_PROFILE, PROPERTY_TYPE_INT8, &v, 1ul);
    ret += property_set(&addr, PROPERTY_NODE_SHORT, PROPERTY_TYPE_INT16, &e->node_addr, 2ul);
    ret += property_set(&addr, PROPERTY_NODE_MAC, PROPERTY_TYPE_INT64, e->payload.device_annouce.ieee, Z_EXTADDR_LEN);
    ret += property_set(&addr, PROPERTY_NODE_CAPABILITY, PROPERTY_TYPE_INT8, &e->payload.device_annouce.capabilities, 1ul);//zigbee specification, page 92
    ret += property_bit_set(&addr, PROPERTY_NODE_ONLINE);
    if (ret)
        build_mission_entity_dirty(&addr);

    zigbee_active_ep(e->node_addr, response_active_ep);
}

static void zigbee_attribute_report(ZigbeeZCLEvent *e)
{
    uint8_t i = 0;
    uint16_t entity;
    uint16_t count = 0;
    uint32_t addr = 0;
    uint8_t *attrs = 0;

    addr = 0;
    if (e->mode == afAddr16Bit)
    {
        while ((addr = entity_find(ENTITY_TYPE_NODE, addr)) && property_compare(addr, PROPERTY_NODE_SHORT, 0, &(e->addr.sht), 2ul));
        if (!addr)
        {
            LogError("report node 0x%04x not found.", e->addr.sht);
            return;
        }
    }
    else if (e->mode == afAddr64Bit)
    {
        while ((addr = entity_find(ENTITY_TYPE_NODE, addr)) && property_compare(addr, PROPERTY_NODE_MAC, 0, e->addr.ieee, Z_EXTADDR_LEN));
        if (!addr)
        {
            LogError("report node %02x%02x%02x%02x%02x%02x not found.", 
                e->addr.ieee[0],
                e->addr.ieee[1],
                e->addr.ieee[2],
                e->addr.ieee[3],
                e->addr.ieee[4],
                e->addr.ieee[5]);
            return;
        }
    }
    property_get(addr, PROPERTY_ENTITY_ID, 0, &entity, 2ul);

    addr = 0;
    while ((addr = entity_find(ENTITY_TYPE_DEVICE, addr)))
    {
        if (property_compare(addr, PROPERTY_DEVICE_NODE, 0, &entity, 2ul))
            continue;
        if (property_compare(addr, PROPERTY_DEVICE_ZIGBEE_ENDPOINT, 0, &e->ep, 1ul))
            continue;
        break;
    }
    if (!addr)
    {
        LogError("device(addr:%d ep:%d) not found.", entity, e->ep);
        return;
    }
    property_get(addr, PROPERTY_ENTITY_ID, 0, &entity, 2ul);

    count = property_size(addr, PROPERTY_DEVICE_ZIGBEE_CLUSTERS_IN) / 2;
    if (count)
    {
        for (i = 0; i < count; i++)
        {
            uint16_t cluster;
            property_get(addr, PROPERTY_DEVICE_ZIGBEE_CLUSTERS_IN, i * 2, &cluster, 2ul);
            if (ZCL_CLUSTER_ID_GEN_ON_OFF == cluster || ZCL_CLUSTER_ID_GEN_LEVEL_CONTROL == cluster)
                break;
        }
        if (i < count)
        {
            uint8_t *attrs = e->attribute.report.attrs;
            uint32_t ap = 0;
            while ((ap = entity_find(ENTITY_TYPE_VALUE, ap)) && property_compare(ap, PROPERTY_VALUE_DEVICE, 0, &entity, 2ul));
            for (i = 0; i < e->attribute.report.count; i++)
            {
                int ret = 0;
                uint16_t ev = 0;
                uint16_t attr = (((uint16_t)attrs[0]) << 8) | attrs[1];

                if (ZCL_CLUSTER_ID_GEN_ON_OFF == e->cluster && ATTRID_ON_OFF == attr && ZCL_DATATYPE_BOOLEAN == attrs[2])
                {
                    if (!ap)
                    {
                        ap = entity_create(ENTITY_TYPE_VALUE, (uint32_t)(1 + 1 + 1 + 1 + 1 + 1));
                        if (ap)
                            property_set(&ap, PROPERTY_VALUE_DEVICE, PROPERTY_TYPE_INT16, &entity, 2ul);
                        ret = 1;
                    }
                    if (ap)
                    {
                        LogAction("device:%d value:%d", entity, attrs[3]);
                        ret += property_set(&ap, PROPERTY_VALUE_ON_OFF, PROPERTY_TYPE_INT8, attrs + 3, 1ul);
                    }
                }
                else if (ZCL_CLUSTER_ID_GEN_LEVEL_CONTROL == e->cluster && ATTRID_LEVEL_CURRENT_LEVEL == attr && ZCL_DATATYPE_UINT8 == attrs[2])
                {
                    if (!ap)
                    {
                        ap = entity_create(ENTITY_TYPE_VALUE, (uint32_t)(1 + 1 + 1 + 1 + 1 + 1));
                        if (ap)
                            property_set(&ap, PROPERTY_VALUE_DEVICE, PROPERTY_TYPE_INT16, &entity, 2ul);
                        ret = 1;
                    }
                    if (ap)
                    {
                        LogAction("device:%d value:%d", entity, attrs[3]);
                        ret += property_set(&ap, PROPERTY_VALUE_LEVEL, PROPERTY_TYPE_INT8, attrs + 3, 1ul);
                    }
                }
                
                if (ret)
                    build_mission_entity_dirty(&ap);

                attrs += 2 + 1 + zclGetAttrDataLength(attrs[2], attrs + 3);
            }
            build_mission_device_related(entity);
        }
    }

    if (property_exist(addr, PROPERTY_DEVICE_LINK))
    {
        uint8_t link[MAX_LINK] = {0};
        property_get(addr, PROPERTY_DEVICE_LINK, 0, link, MAX_LINK);

        attrs = e->attribute.report.attrs;
        for (i = 0; i < e->attribute.report.count; i++)
        {
            uint16_t attr = (((uint16_t)attrs[0]) << 8) | attrs[1];

            if (ZCL_CLUSTER_ID_GEN_ON_OFF == e->cluster && ATTRID_ON_OFF == attr && ZCL_DATATYPE_BOOLEAN == attrs[2])
            {
                uint8_t data[2 + 1 + 1] = {0};
                *(uint16_t *)(data + 0) = network_htons(entity);
                data[2] = EVENT_ENTITY_DEVICE_KEY;
                data[3] = attrs[3];
                cluster_send(cluster_id(), 0, OPCODE_ENTITY_EVENT, 0, 0, data, 4, 0);

                data[0] = ENTITY_COMMAND_DEVICE_OPERATE_ONOFF;
                data[1] = attrs[3];

                build_mission_link_execute(link, entity, data, 2);
            }

            attrs += 2 + 1 + zclGetAttrDataLength(attrs[2], attrs + 3);
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

static void cluster_handler(uint8_t code, const void *evt)
{
    uint32_t addr = 0;
    if (CLUSTER_EVENT_TERMINAL_ALLOC == code)
    {
        terminal_save((const uint8_t *)evt);
    }
    else if (CLUSTER_EVENT_CLOUD_STARTED == code)
    {
        led_set(LED_ID_NET_RED, 0, 0);
    }
    else if (CLUSTER_EVENT_LANWORK_STARTED == code)
    {
        build_mission_terminal_update();
    }
    else if (CLUSTER_EVENT_INVITE == code)
    {
        ClusterInviteEvent *e = (ClusterInviteEvent *)evt;

        cluster_save(e->cluster, e->pin);

        build_mission_terminal_update();
    }
    else if (CLUSTER_EVENT_TIME == code)
    {
        ClusterTimeEvent *e = (ClusterTimeEvent *)evt;
    }
    else if (CLUSTER_EVENT_PACKET == code)
    {
        PacketCluster *e = (PacketCluster *)evt;

        if (OPCODE_TERMINAL_SYNC == e->opcode)
        {
            if (os_memcmp(e->dst, terminal_local(), MAX_TERMINAL_ID))
            {
                if (os_memcmp(e->dst, bcast_addr(), MAX_TERMINAL_ID))
                    return;
            }

            if (!(e->data[0] & TERMINAL_TYPE_GATEWAY))
                return;

            build_mission_terminal_update();
        }
        else if (OPCODE_ENTITY_SYNC == e->opcode)
        {
            uint8_t *data;
            uint16_t entity;

            addr = network_ntohl(*(uint32_t *)e->data);//addr
            addr = entity_iterator(addr);
            if (!addr)
            {
                build_mission_entity_update(e->cluster, e->src, e->opt[0], 0, 0);
                return;
            }
            property_get(addr, PROPERTY_ENTITY_ID, 0, &entity, 2ul);

            build_mission_entity_update(e->cluster, e->src, e->opt[0], addr, entity);
        }
        else if (OPCODE_ENTITY_CREATE == e->opcode)
        {
            uint16_t entity;
            uint8_t command = ENTITY_COMMAND_CREATE;

            addr = entity_create(e->data[0], e->datalen - 1);
            if (!addr)
            {
                build_mission_entity_update(e->cluster, e->src, e->opt[0], 0, 0);
                return;
            }
            if (e->datalen > 1)
                entity_unpack(&addr, e->data + 1, e->datalen - 1);
            property_get(addr, PROPERTY_ENTITY_ID, 0, &entity, 2ul);
            command_execute(entity, e->data[0], &command, 1, 0);

            build_mission_entity_update(e->cluster, e->src, e->opt[0], addr, entity);
        }
        else if (OPCODE_ENTITY_DELETE == e->opcode)
        {
            uint8_t type;
            uint8_t command = ENTITY_COMMAND_DELETE;
            uint16_t entity;

            addr = entity_get(network_ntohs(*(uint16_t *)e->data));
            if (!addr)
            {
                build_mission_entity_update(e->cluster, e->src, e->opt[0], 0, 0);
                return;
            }
            property_get(addr, PROPERTY_ENTITY_TYPE, 0, &type, 1ul);
            property_get(addr, PROPERTY_ENTITY_ID, 0, &entity, 2ul);
            command_execute(entity, type, &command, 1, 0);
            
            entity_release(addr);

            build_mission_entity_update(e->cluster, e->src, e->opt[0], 0, entity);
        }
        else if (OPCODE_ENTITY_COMMAND == e->opcode)
        {
            uint8_t type;
            uint16_t entity;

            addr = entity_get(network_ntohs(*(uint16_t *)(e->data + 0)));
            if (!addr)
                return;
            cluster_send(e->cluster, e->src, OPCODE_ENTITY_RESPONSE, e->opt, e->optlen, 0, 0, 0);

            property_get(addr, PROPERTY_ENTITY_TYPE, 0, &type, 1ul);
            property_get(addr, PROPERTY_ENTITY_ID, 0, &entity, 2ul);

            command_execute(entity, type, e->data + 2, e->datalen - 2, 0);
        }
        else if (OPCODE_ENTITY_LINK == e->opcode)
        {
            build_mission_link_execute(e->data, 0, e->data + MAX_LINK, e->datalen - MAX_LINK);
        }
        else if (OPCODE_PROPERTY_CREATE == e->opcode || OPCODE_PROPERTY_SET == e->opcode)
        {
            addr = entity_get(network_ntohs(*(uint16_t *)e->data));
            if (addr)
            {
                uint8_t opt[MAX_TERMINAL_ID + 1] = {0};
                os_memcpy(opt, e->src, MAX_TERMINAL_ID);
                opt[MAX_TERMINAL_ID] = e->opt[0];
                cluster_send(e->cluster, 0, OPCODE_PROPERTY_UPDATE, opt, MAX_TERMINAL_ID + 1, e->data, e->datalen, 0);
                entity_unpack(&addr, e->data + 2, e->datalen - 2);
            }
        }
        else if (OPCODE_PROPERTY_DELETE == e->opcode)
        {
            addr = entity_get(network_ntohs(*(uint16_t *)e->data));
            if (addr)
            {
                uint8_t opt[MAX_TERMINAL_ID + 1] = {0};
                uint8_t data[2 + 1 + 1] = {0};

                property_release(&addr, e->data[2]);

                os_memcpy(data, e->data, 3);
                data[3] = 0xff;
                
                os_memcpy(opt, e->src, MAX_TERMINAL_ID);
                opt[MAX_TERMINAL_ID] = e->opt[0];

                cluster_send(e->cluster, 0, OPCODE_PROPERTY_UPDATE, opt, MAX_TERMINAL_ID + 1, data, 4, 0);
            }
        }
        else if (OPCODE_GANGER_SYNC == e->opcode)
        {
            uint8_t value;
            uint16_t entity;
            uint16_t master;

            addr = 0;
            while ((addr = entity_find(ENTITY_TYPE_MASTER, addr)) && property_compare(addr, PROPERTY_MASTER_LINK, 0, e->data, MAX_LINK));
            if (!addr)
                return;
            property_get(addr, PROPERTY_ENTITY_ID, 0, &master, 2ul);

            addr = 0;
            while ((addr = entity_find(ENTITY_TYPE_GANGER, addr)))
            {
                if (property_compare(addr, PROPERTY_GANGER_TERMINAL, 0, e->src, MAX_TERMINAL_ID))
                    continue;
                if (property_compare(addr, PROPERTY_GANGER_LINK, 0, e->data, MAX_LINK))
                    continue;
                break;
            }
            if (e->data[MAX_LINK])
            {
                if (!addr)
                {
                    addr = entity_create(ENTITY_TYPE_GANGER, (uint32_t)(1 + 1 + MAX_LINK + 1 + 1 + MAX_TERMINAL_ID));
                    if (addr)
                    {
                        property_set(&addr, PROPERTY_GANGER_LINK, PROPERTY_TYPE_INT48, e->data, MAX_LINK);
                        property_set(&addr, PROPERTY_GANGER_TERMINAL, PROPERTY_TYPE_INT32, e->src, MAX_TERMINAL_ID);
                    }
                }
            }
            else
            {
                if (addr)
                    entity_release(addr);
            }
            
            addr = 0;
            while ((addr = entity_find(ENTITY_TYPE_SLAVE, addr)))
            {
                uint16_t device;
                uint32_t ap = 0;
                if (property_compare(addr, PROPERTY_SLAVE_MASTER, 0, &master, 2ul))
                    continue;
                property_get(addr, PROPERTY_SLAVE_DEVICE, 0, &device, 2ul);

                ap = 0;
                while ((ap = entity_find(ENTITY_TYPE_VALUE, ap)) && property_compare(ap, PROPERTY_VALUE_DEVICE, 0, &device, 2ul));
                if (ap)
                {
                    property_get(ap, PROPERTY_VALUE_ON_OFF, 0, &value, 1ul);
                    if (value)
                        break;
                    property_get(ap, PROPERTY_VALUE_LEVEL, 0, &value, 1ul);
                    if (value)
                        break;
                }
            }

            if (!value)
            {
                addr = 0;
                while ((addr = entity_find(ENTITY_TYPE_GANGER, addr)) && property_compare(addr, PROPERTY_GANGER_LINK, 0, e->data, MAX_LINK));
                value = !!addr;
            }
            
            addr = 0;
            while ((addr = entity_find(ENTITY_TYPE_DEVICE, addr)))
            {
                uint16_t i;
                uint16_t count;
                if (property_compare(addr, PROPERTY_DEVICE_LINK, 0, e->data, MAX_LINK))
                    continue;
                count = property_size(addr, PROPERTY_DEVICE_ZIGBEE_CLUSTERS_IN) / 2;
                if (!count)
                    continue;
                for (i = 0; i < count; i++)
                {
                    uint16_t cluster;
                    property_get(addr, PROPERTY_DEVICE_ZIGBEE_CLUSTERS_IN, i * 2, &cluster, 2ul);

                    if (ZCL_CLUSTER_ID_GEN_ON_OFF == cluster)
                        break;
                }
                if (i >= count)
                {
                    uint8_t ep;
                    uint16_t entity;
                    uint16_t node_addr;
                    uint32_t ap;
                    property_get(addr, PROPERTY_ENTITY_ID, 0, &entity, 2ul);

                    ap = 0;
                    while ((ap = entity_find(ENTITY_TYPE_VALUE, ap)) && property_compare(ap, PROPERTY_VALUE_DEVICE, 0, &entity, 2ul));
                    if (ap)
                    {
                        if (property_set(&ap, PROPERTY_VALUE_ON_OFF, PROPERTY_TYPE_INT8, &value, 1ul))
                            build_mission_entity_dirty(&ap);
                    }

                    property_get(addr, PROPERTY_DEVICE_ZIGBEE_ENDPOINT, 0, &ep, 1ul);
                    property_get(addr, PROPERTY_DEVICE_NODE, 0, &entity, 2ul);

                    ap = entity_get(entity);
                    if (!ap)
                        continue;
                    property_get(ap, PROPERTY_NODE_SHORT, 0, &node_addr, 2ul);

                    zigbee_zcl_command(node_addr, ep, ZCL_CLUSTER_ID_GEN_ON_OFF, value ? COMMAND_ON : COMMAND_OFF, 1, 0, 0, 0, 0);
                }
            }
        }
    }
}

void core_init(void)
{
    led_set(LED_ID_NET_RED, 0xff, 0);

    block_defrag();

    cluster_init(terminal_id(), os_rand());

    cluster_listen(cluster_handler);

    zigbee_init(zigbee_handler);

    mission_init();
    
    if (os_memcmp(cluster_id(), local_addr(), MAX_CLUSTER_ID))
        cluster_join(cluster_id(), cluster_pin());
}

void core_update(void)
{
    cluster_update();

    zigbee_update();

    mission_update();
}
