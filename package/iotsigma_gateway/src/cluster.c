#include "cluster.h"
#include "endian.h"
#include "interface_os.h"
#include "interface_network.h"
#include "driver_led.h"
#include "driver_linux.h"
#include "driver_network_linux.h"
#include "buddha_heap.h"
#include "aes.h"
#include "log.h"

#include "ecc.h"

#define HOST_LANWORK {255, 255, 255, 255}
#define PORT_LANWORK 7861

#define CLOUD_HOST "control.iotsigma.com"
#define CLOUD_PORT 9000

#define CLUSTER_API_INTERNAL 0
#define CLUSTER_API_VERIFY 1
#define CLUSTER_API_TIME 2

#define LANWORK_STATE_INIT 0
#define LANWORK_STATE_SERVICE 1

#define CLOUD_STATE_INIT 0
#define CLOUD_STATE_RECONNECT 1
#define CLOUD_STATE_AUTH 2
#define CLOUD_STATE_SERVICE 3

#define MAX_RMP_RETRY 10

typedef struct _cluster
{
    uint8_t id[MAX_CLUSTER_ID];
    uint8_t pin[MAX_CLUSTER_PIN];

    uint8_t leave;
}Cluster;

typedef struct
{
    uint8_t state;

    int fp;
    uint32_t timer;
}ClusterChannel;

static const uint8_t _local_addr[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t _bcast_addr[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

static const uint8_t _terminal_key_private[] = {
    0xda, 0x85, 0x8a, 0x02, 0xf2, 0x79, 0xe6, 0x95, 0x31, 0x4d, 0x70, 0xff, 0x1f, 0x35, 0x9f, 0xad
};
static const uint8_t _terminal_key_public[] = {
    0x8d, 0xad, 0x9f, 0x84, 0x90, 0x8e, 0xac, 0x3f, 0x03, 0xc6, 0x5c, 0xcd, 0x2e, 0xed, 0x1e, 0x4b,
    0xca, 0xa6, 0x47, 0xcc, 0x9f, 0xdc, 0xcb, 0x1f, 0x32, 0xb5, 0xa4, 0x6a, 0x35, 0x32, 0xd3, 0xc5
};

static const struct 
{
    uint8_t cluster[MAX_CLUSTER_ID];
    uint8_t pin[MAX_CLUSTER_PIN];
} _cluster_apis[] = {
    {
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
        {
            0x96, 0x13, 0xed, 0x44, 0x06, 0x47, 0x11, 0xe9, 0x9f, 0xdd, 0x57, 0xec, 0x51, 0x79, 0x71, 0x9a,
            0xce, 0x66, 0xab, 0xd2, 0x06, 0x47, 0x11, 0xe9, 0x93, 0x9e, 0xef, 0xe6, 0xf7, 0x00, 0x1e, 0x36
        }
    },
    {
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x01},
        {
            0x96, 0x13, 0xed, 0x44, 0x06, 0x47, 0x11, 0xe9, 0x9f, 0xdd, 0x57, 0xec, 0x51, 0x79, 0x71, 0x9a,
            0xce, 0x66, 0xab, 0xd2, 0x06, 0x47, 0x11, 0xe9, 0x93, 0x9e, 0xef, 0xe6, 0xf7, 0x00, 0x1e, 0x36
        }
    },
    {
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x02},
        {
            0xb7, 0xf8, 0x9c, 0x18, 0x93, 0x30, 0x11, 0xe9, 0xac, 0x18, 0x0f, 0x0b, 0x3a, 0xa8, 0xfb, 0x7f,
            0xd6, 0x62, 0x7d, 0x54, 0x93, 0x30, 0x11, 0xe9, 0xa1, 0x6f, 0x57, 0x07, 0x6e, 0x11, 0xd3, 0x62
        }
    }
};

static ClusterChannel _cloud = {0};
static ClusterChannel _lanwork = {0};

static uint16_t _terminal_reset = 0;
static uint32_t _session_bcast = 1;
static uint32_t _session_peer = 1;
static uint8_t _terminal_local[MAX_TERMINAL_ID] = {0};

int pack_size(PacketCluster *cluster)
{
    return 1 + 2 + 1 + 1 + 2 + 4 + MAX_TERMINAL_ID * 2 + MAX_CLUSTER_ID + 1 + cluster->optlen + cluster->datalen + 1;
}

void pack(uint8_t *buffer, PacketCluster *cluster)
{
    uint8_t i;
    uint8_t cs = 0;
    uint8_t *packet = buffer;

    cs = *packet++ = 0xaa;

    *(uint16_t *)packet = network_htons(cluster->optlen + cluster->datalen);
    cs += *(packet + 0);
    cs += *(packet + 1);
    packet += 2;

    *packet++ = 0 - cs;

    cs = 0;

    *packet++ = cluster->opcode;
    cs += cluster->opcode;

    *(uint16_t *)packet = network_htons(cluster->reset);
    cs += *(packet + 0);
    cs += *(packet + 1);
    packet += 2;
    
    *(uint32_t *)packet = network_htonl(cluster->seq);
    cs += *(packet + 0);
    cs += *(packet + 1);
    cs += *(packet + 2);
    cs += *(packet + 3);
    packet += 4;

    os_memcpy(packet, cluster->dst, MAX_TERMINAL_ID);
    for (i = 0; i < MAX_TERMINAL_ID; i++)
        cs += *(packet + i);
    packet += MAX_TERMINAL_ID;

    os_memcpy(packet, cluster->src, MAX_TERMINAL_ID);
    for (i = 0; i < MAX_TERMINAL_ID; i++)
        cs += *(packet + i);
    packet += MAX_TERMINAL_ID;

    os_memcpy(packet, cluster->cluster, MAX_CLUSTER_ID);
    for (i = 0; i < MAX_CLUSTER_ID; i++)
        cs += *(packet + i);
    packet += MAX_CLUSTER_ID;

    *packet++ = cluster->optlen;
    cs += cluster->optlen;

    if (cluster->optlen)
    {
        os_memcpy(packet, cluster->opt, cluster->optlen);
        for (i = 0; i < cluster->optlen; i++)
            cs += *(packet + i);
        packet += cluster->optlen;
    }

    if (cluster->datalen)
    {
        os_memcpy(packet, cluster->data, cluster->datalen);
        for (i = 0; i < cluster->datalen; i++)
            cs += *(packet + i);
        packet += cluster->datalen;
    }
    *packet++ = 0 - cs;
}

int unpack(uint8_t *buffer, uint16_t size, PacketCluster *cluster)
{
    uint8_t cs = 0;
    int i = 0;
    if (size < 1 + 2 + 1 + 1 + 2 + 4 + MAX_TERMINAL_ID * 2 + MAX_CLUSTER_ID + 1 + 1)
        return 0;
    if (0xaa != buffer[0])
    {
        LogError("lead code error");
        return -1;
    }
    cs = 0;
    for (i = 0; i < 4; i++)
        cs += buffer[i];
    if (cs)
    {
        LogError("head checksum error");
        return -1;
    }
    if (size < 1 + 2 + 1 + 1 + 2 + 4 + MAX_TERMINAL_ID * 2 + MAX_CLUSTER_ID + 1 + network_ntohs(*(uint16_t *)(buffer + 1)) + 1)
        return 0;
    cs = 0;
    for (i = 0; i < 1 + 2 + 1 + 1 + 2 + 4 + MAX_TERMINAL_ID * 2 + MAX_CLUSTER_ID + 1 + network_ntohs(*(uint16_t *)(buffer + 1)) + 1; i++)
        cs += buffer[i];
    if (cs)
    {
        LogError("body checksum error");
        return -1;
    }
    cluster->datalen = network_ntohs(*(uint16_t *)(buffer + 1));
    buffer += 4;
    cluster->opcode = *buffer++;

    cluster->reset = network_ntohs(*(uint16_t *)buffer);
    buffer += 2;

    cluster->seq = network_ntohl(*(uint32_t *)buffer);
    buffer += 4;

    cluster->dst = buffer;
    buffer += MAX_TERMINAL_ID;

    cluster->src = buffer;
    buffer += MAX_TERMINAL_ID;

    cluster->cluster = buffer;
    buffer += MAX_CLUSTER_ID;

    cluster->optlen = *buffer++;
    cluster->opt = buffer;
    buffer += cluster->optlen;

    cluster->datalen -= cluster->optlen;
    cluster->data = buffer;
    buffer += cluster->datalen;

    cluster->cloud = 0;
    cluster->lanwork = 0;
    return pack_size(cluster);
}

static void dispatch(uint8_t opcode, const void *evt)
{
    ClusterListenHandler *listener = 0;
    while ((listener = (ClusterListenHandler *)buddha_heap_find(listener, HEAP_MARK_CLUSTER_LISTENER)))
        (*listener)(opcode, evt);
}

static int cluster_recv(const char *channel, PacketCluster *packet)
{
    uint32_t last = 0;
    PacketCluster *old = 0;
    Cluster *cluster = 0;
    while ((cluster = (Cluster *)buddha_heap_find(cluster, HEAP_MARK_CLUSTER)) 
        && os_memcmp((const char *)cluster->id, (const char *)packet->cluster, MAX_CLUSTER_ID));
    if (!cluster)
        return -1;

    if (OPCODE_AUTH_RSP != packet->opcode &&
        OPCODE_AUTH_CONFIRM != packet->opcode)
    {
        if (!os_memcmp(packet->dst, terminal_local(), MAX_TERMINAL_ID))
        {
            if (OPCODE_RMP_ACK != packet->opcode)
            {
                uint8_t data[4] = {0};
                *(uint32_t *)data = network_htonl(packet->seq);
                cluster_send(packet->cluster, packet->src, OPCODE_RMP_ACK, 0, 0, data, 4, CLUSTER_SEND_FLAG_LANWORK);
            }
        }
        else if (os_memcmp(packet->dst, bcast_addr(), MAX_TERMINAL_ID))
        {
            return -1;
        }

        old = 0;
        while ((old = (PacketCluster *)buddha_heap_find(old, HEAP_MARK_PACKET)))
        {
            if (old == packet)
                continue;
            if (!os_memcmp(old->dst, packet->dst, MAX_TERMINAL_ID) && 
                !os_memcmp(old->src, packet->src, MAX_TERMINAL_ID) && 
                old->seq == packet->seq && 
                old->reset == packet->reset)
                return -1;
            if (!os_memcmp(old->src, packet->src, MAX_TERMINAL_ID) && 
                !os_memcmp(old->dst, bcast_addr(), MAX_TERMINAL_ID) && 
                old->reset == packet->reset && 
                old->seq < packet->seq && 
                old->seq > last)
                last = old->seq;
        }
        
        if (old && last && last + 1 < packet->seq)
        {
            uint8_t data[8] = {0};
            *(uint32_t *)(data + 0) = network_htonl(last + 1);
            *(uint32_t *)(data + 4) = network_htonl(packet->seq - 1);
            cluster_send(packet->cluster, packet->src, OPCODE_RMP_RETRY, 0, 0, data, 8, CLUSTER_SEND_FLAG_LANWORK);
        }
    }

    packet->datalen = AES128CBCDecrypt(packet->data, packet->data, packet->datalen, cluster->pin, cluster->pin + 16);
    if (packet->seq != network_ntohl(*(uint32_t *)(packet->data + packet->datalen - 4)))
    {
        LogError("decrypto error");
        return -1;
    }
    packet->datalen -= 4;

    if (OPCODE_AUTH_RSP == packet->opcode && !os_memcmp(terminal_local(), local_addr(), MAX_TERMINAL_ID))
    {
        dispatch(CLUSTER_EVENT_PACKET, packet);
        return -1;
    }
    if (OPCODE_AUTH_CONFIRM == packet->opcode)
    {
        dispatch(CLUSTER_EVENT_PACKET, packet);
        return -1;
    }
    if (packet->opcode == OPCODE_RMP_ACK)
    {
        uint32_t start = network_ntohl(*(uint32_t *)(packet->data + 0));
        old = 0;
        while ((old = (PacketCluster *)buddha_heap_find(old, HEAP_MARK_PACKET)))
        {
            if (!os_memcmp(old->src, terminal_local(), MAX_TERMINAL_ID) && old->reset == _terminal_reset && old->seq == start)
                break;
        }
        if (old)
            buddha_heap_destory(old);
        return -1;
    }
    if (packet->opcode == OPCODE_RMP_RETRY)
    {
        uint32_t start = 0;
        uint32_t end = 0;
        start = network_ntohl(*(uint32_t *)(packet->data + 0));
        end = network_ntohl(*(uint32_t *)(packet->data + 4));
        for (; start <= end; start++)
        {
            old = 0;
            while ((old = (PacketCluster *)buddha_heap_find(old, HEAP_MARK_PACKET)))
            {
                if (!os_memcmp(old->dst, bcast_addr(), MAX_TERMINAL_ID) && 
                    !os_memcmp(old->src, terminal_local(), MAX_TERMINAL_ID) && 
                    old->reset == _terminal_reset && 
                    old->seq == start)
                    break;
            }
            if (!old)
            {
                LogAction("retransition failed seq:%d", packet->seq);
            }
            else
            {
                LogAction("retransition successed seq:%d", packet->seq);
                buddha_heap_reuse(old);
                old->lanwork = 1;
                old->retry = 0;
            }
        }
        return -1;
    }
    
    LogAction("channel:%7s cluster:%02x%02x%02x%02x%02x%02x|dst:%02x%02x%02x%02x|src:%02x%02x%02x%02x|opcode:%3d|reset:%5d|seq:%d|optlen:%d|datalen:%d",
        channel,
        packet->cluster[0],
        packet->cluster[1],
        packet->cluster[2],
        packet->cluster[3],
        packet->cluster[4],
        packet->cluster[5],
        packet->dst[0],
        packet->dst[1],
        packet->dst[2],
        packet->dst[3],
        packet->src[0],
        packet->src[1],
        packet->src[2],
        packet->src[3],
        packet->opcode,
        packet->reset,
        packet->seq,
        packet->optlen,
        packet->datalen);

    dispatch(CLUSTER_EVENT_PACKET, packet);
    return 1;
}

static int lanwork_receiver(void)
{
    int ret = 0;
    uint8_t ip[4] = {0};
    uint16_t port;
    PacketCluster *packet = 0;
    packet = (PacketCluster *)buddha_heap_alloc(sizeof(PacketCluster) + 256, HEAP_MARK_PACKET);
    if (!packet)
        return 0;
    os_memset(packet, 0, sizeof(PacketCluster));
    
    ret = network_udp_recv(_lanwork.fp, packet + 1, 256, ip, &port);
    if (ret < 0)
        _lanwork.state = LANWORK_STATE_INIT;
    else if (ret && unpack((uint8_t *)(packet + 1), ret, packet) <= 0)
        ret = 0;
    if (ret && cluster_recv("lanwork", packet) < 0)
        ret = 0;
    if (ret <= 0)
    {
        buddha_heap_destory(packet);
    }
    else
    {
        os_memcpy(packet + 1, packet->src, MAX_TERMINAL_ID);
        os_memcpy((uint8_t *)(packet + 1) + MAX_TERMINAL_ID, packet->dst, MAX_TERMINAL_ID);
        packet = buddha_heap_realloc(packet, sizeof(PacketCluster) + MAX_TERMINAL_ID * 2);
        packet->src = (uint8_t *)(packet + 1);
        packet->dst = (uint8_t *)(packet + 1) + MAX_TERMINAL_ID;
        packet->time = os_ticks();
        //buddha_heap_free(packet);
    }
    return ret;
}

static int lanwork_sender(void)
{
    int ret = 0;
    PacketCluster *min = 0, *max = 0, *ack = 0, *peer = 0;
    PacketCluster *packet = 0;
    while ((packet = buddha_heap_find(packet, HEAP_MARK_PACKET)))
    {
        if (!packet->lanwork)
            continue;
        if (packet->reset != _terminal_reset)
        {
            packet->lanwork = 0;
            packet->cloud = 0;
            buddha_heap_free(packet);
            continue;
        }
        if (packet->opcode == OPCODE_RMP_ACK)
        {
            ack = packet;
            break;
        }
        if (!os_memcmp(packet->dst, bcast_addr(), MAX_TERMINAL_ID))
        {
            if (!min)
                min = packet;
            if (min->seq > packet->seq)
                min = packet;

            if (!max)
                max = packet;
            if (max->seq < packet->seq)
                max = packet;
        }
        else
        {
            if (!peer)
                peer = packet;
            if (peer->retry > packet->retry)
                peer = packet;
            else if (peer->retry == packet->retry && peer->seq > packet->seq)
                peer = packet;
        }
    }
    packet = 0;
    if (ack)
    {
        packet = ack;
    }
    else if (peer && !min)
    {
        packet = peer;
    }
    else if (min && !peer)
    {
        packet = min;
    }
    else
    {
        packet = (os_ticks() % 2) ? peer : min;
    }
    if (packet)
    {
        uint8_t *buffer = 0;
        uint8_t ip[] = HOST_LANWORK;

        if (packet->opcode != OPCODE_RMP_ACK && _lanwork.timer && os_ticks_from(_lanwork.timer) > os_ticks_ms(50 + _lanwork.timer % 50))
            return 0;
        buffer = (uint8_t *)buddha_heap_alloc(pack_size(packet), 0);
        if (buffer)
        {
            pack(buffer, packet);
            ret = network_udp_send(_lanwork.fp, buffer, pack_size(packet), ip, PORT_LANWORK);
            buddha_heap_free(buffer);
            if (ret < 0)
            {
                _lanwork.state = LANWORK_STATE_INIT;
            }
            else if (packet->opcode == OPCODE_RMP_ACK)
            {
                packet->lanwork = 0;
                packet->cloud = 0;
            }
            else if (os_memcmp(packet->dst, bcast_addr(), MAX_TERMINAL_ID))
            {
                _lanwork.timer = os_ticks();
                packet->retry++;
                if (packet->retry >= MAX_RMP_RETRY)
                    packet->lanwork = 0;
            }
            else if (packet == min && packet == max)
            {
                _lanwork.timer = os_ticks();
                packet->retry++;
                if (packet->retry >= MAX_RMP_RETRY)
                    packet->lanwork = 0;
            }
            else
            {
                packet->lanwork = 0;
            }
            if (!packet->lanwork && !packet->cloud)
                buddha_heap_free(packet);
        }
    }
    return ret;
}

static int cloud_receiver(void)
{
    int ret = 0;
    PacketCluster *packet = (PacketCluster *)buddha_heap_find(0, HEAP_MARK_PACKET_CLOUD);
    if (!packet)
    {
        packet = buddha_heap_alloc(sizeof(PacketCluster) + 2 + 256, HEAP_MARK_PACKET_CLOUD);
        *(uint16_t *)(packet + 1) = 0;
    }
    ret = network_tcp_recv(_cloud.fp, (uint8_t *)(packet + 1) + 2 + *(uint16_t *)(packet + 1), 256 - *(uint16_t *)(packet + 1));
    if (ret > 0)
    {
        *(uint16_t *)(packet + 1) += ret;

        ret = unpack((uint8_t *)(packet + 1) + 2, *(uint16_t *)(packet + 1), packet);
    }
    if (ret < 0)
    {
        LogError("reconnect");
        _cloud.state = CLOUD_STATE_RECONNECT;
        _cloud.timer = os_ticks();
    }
    else if (ret)
    {
        buddha_heap_mark_set(packet, HEAP_MARK_PACKET);
        ret = cluster_recv("cloud", packet);
    }
    if (ret < 0)
    {
        buddha_heap_destory(packet);
    }
    else if (ret > 0)
    {
        os_memcpy(packet + 1, packet->src, MAX_TERMINAL_ID);
        os_memcpy((uint8_t *)(packet + 1) + MAX_TERMINAL_ID, packet->dst, MAX_TERMINAL_ID);
        packet = buddha_heap_realloc(packet, sizeof(PacketCluster) + MAX_TERMINAL_ID * 2);
        packet->src = (uint8_t *)(packet + 1);
        packet->dst = (uint8_t *)(packet + 1) + MAX_TERMINAL_ID;
        packet->time = os_ticks();
        //buddha_heap_free(packet);
    }
    return ret;
}

static int cloud_sender(void)
{
    int ret = 0;
    uint8_t *buffer = 0;
    buffer = (uint8_t *)buddha_heap_find(buffer, HEAP_MARK_SENDOR);
    if (buffer)
    {
        ret = network_tcp_send(_cloud.fp, buffer + 4 + (*(uint16_t *)buffer), (*(uint16_t *)(buffer + 2)) - (*(uint16_t *)buffer));
        if (ret >= 0)
        {
            *(uint16_t *)buffer += ret;
            if (*(uint16_t *)buffer >= *(uint16_t *)(buffer + 2))
            {
                buddha_heap_destory(buffer);
                buffer = 0;
            }
        }
        else
        {
            buddha_heap_destory(buffer);
            buffer = 0;
        }
    }
    if (!buffer)
    {
        PacketCluster *min = 0;
        PacketCluster *packet = 0;
        while ((packet = buddha_heap_find(packet, HEAP_MARK_PACKET)))
        {
            if (packet->cloud && packet->reset == _terminal_reset && !os_memcmp(packet->src, terminal_local(), MAX_TERMINAL_ID))
            {
                if (!min)
                    min = packet;
                if (min->seq > packet->seq)
                    min = packet;
            }
        }
        if (min)
        {
            uint8_t *buffer = (uint8_t *)buddha_heap_alloc(2 + 2 + pack_size(min), HEAP_MARK_SENDOR);
            if (buffer)
            {
                LogAction("opcode:%3u seq:%u", min->opcode, min->seq);
                *(uint16_t *)(buffer + 0) = 0;
                *(uint16_t *)(buffer + 2) = pack_size(min);
                pack(buffer + 4, min);
                
                min->cloud = 0;
                if (!min->cloud && !min->lanwork)
                    buddha_heap_free(min);
            }
        }
    }
    return ret;
}

static void packet_auth_rsp(PacketCluster *packet)
{
    uint8_t terminal_key_auth[] = {
        0x9b, 0x51, 0x9d, 0x42, 0xfc, 0x29, 0x11, 0xe8, 0xad, 0xa7, 0xdb, 0xe3, 0xac, 0x74, 0xb5, 0x5c
    };
    EccPoint cloud_public = {
        {0x12, 0x59, 0x8e, 0xb9, 0x7b, 0x8e, 0xff, 0x6d, 0xd9, 0x87, 0xbd, 0xf7, 0x25, 0xc7, 0x03, 0x76},
        {0x21, 0x54, 0x25, 0xd1, 0x4e, 0xcf, 0x51, 0xab, 0xb7, 0x52, 0xd0, 0x27, 0xe3, 0x6d, 0x91, 0x2a}
    };

    uint8_t *terminal_verify = buddha_heap_find(0, HEAP_MARK_TERMINAL_VERITY);
    if (!terminal_verify)
    {
        LogError("verify not found");
        _cloud.state = CLOUD_STATE_RECONNECT;
        _cloud.timer = os_ticks();
        return;
    }

    if (os_memcmp(packet->cluster, _cluster_apis[CLUSTER_API_VERIFY].cluster, MAX_CLUSTER_ID))
    {
        LogError("verify cluster not match.");
        _cloud.state = CLOUD_STATE_RECONNECT;
        _cloud.timer = os_ticks();
        return;
    }

    if (!ecdsa_verify(&cloud_public, terminal_verify, packet->data, packet->data + MAX_CRYPTO_PRIVATE))
    {
        LogError("ecdsa verify failed!");
        _cloud.state = CLOUD_STATE_RECONNECT;
        _cloud.timer = os_ticks();
        return;
    }

    if (!ecdsa_sign(packet->data, packet->data + MAX_CRYPTO_PRIVATE, (uint8_t *)_terminal_key_private, packet->data + (os_ticks() % MAX_CRYPTO_PRIVATE), packet->opt))
    {
        LogError("ecdsa sign failed!");
        _cloud.state = CLOUD_STATE_RECONNECT;
        _cloud.timer = os_ticks();
        return;
    }

    if (!os_memcmp(_terminal_local, local_addr(), MAX_TERMINAL_ID))
        os_memcpy(_terminal_local, packet->dst, MAX_TERMINAL_ID);
    cluster_send(_cluster_apis[CLUSTER_API_VERIFY].cluster, packet->src,
        OPCODE_AUTH_VERIFY,
        (const uint8_t *)packet->data, MAX_CRYPTO_PRIVATE * 2,
        terminal_key_auth, sizeof(terminal_key_auth)/sizeof(terminal_key_auth[0]),
        CLUSTER_SEND_FLAG_CLOUD);
    LogAction("cloud verify successed");
}

static void packet_auth_confirm(PacketCluster *packet)
{
    if (os_memcmp(packet->cluster, _cluster_apis[CLUSTER_API_VERIFY].cluster, MAX_CLUSTER_ID))
    {
        LogError("verify cluster not match.");
        return;
    }
    if (!os_memcmp(packet->dst, local_addr(), MAX_TERMINAL_ID))
    {
        network_tcp_close(_cloud.fp);
        return;
    }
    if (!os_memcmp(_terminal_local + 1, bcast_addr(), 2UL))
    {
        os_memcpy(_terminal_local, packet->dst, MAX_TERMINAL_ID);

        LogAction("terminal id:%02x%02x%02x%02x", 
            _terminal_local[0],
            _terminal_local[1],
            _terminal_local[2],
            _terminal_local[3]);

        dispatch(CLUSTER_EVENT_TERMINAL_ALLOC, _terminal_local);
    }
    _cloud.state = CLOUD_STATE_SERVICE;
    _cloud.timer = 1;

    LogAction("cloud auth successed");

    dispatch(CLUSTER_EVENT_CLOUD_STARTED, 0);
}

static void packet_internal_declare(PacketCluster *packet)
{
    int i;
    ClusterDeclareEvent *cde = 0;
    uint8_t *pri = (uint8_t *)buddha_heap_talloc(MAX_CRYPTO_PRIVATE, 0);
    if (!pri)
        return;
    cde = (ClusterDeclareEvent *)buddha_heap_talloc(sizeof(ClusterDeclareEvent), 0);
    if (!cde)
        return;
    if (os_memcmp(packet->cluster, _cluster_apis[CLUSTER_API_INTERNAL].cluster, MAX_CLUSTER_ID))
        return;
    cde->terminal = packet->src;
    cde->type = packet->opt[0];
    for (i = 0; i < MAX_CRYPTO_PRIVATE; i++)
        pri[i] = os_rand();
    ecc_make_key((EccPoint *)cde->pub, pri, pri);

    ecdh_shared_secret(cde->crypto, (EccPoint *)packet->data, pri, 0);

    LogAction("declare|type:%d|terminal:%02x%02x%02x%02x",
        cde->type,
        cde->terminal[0],
        cde->terminal[1],
        cde->terminal[2],
        cde->terminal[3]);

    dispatch(CLUSTER_EVENT_DECLARE, cde);
    buddha_heap_destory(cde);
    buddha_heap_destory(pri);
}

static void packet_internal_invite(PacketCluster *packet)
{
    uint32_t i;
    uint8_t *declare = 0;
    ClusterInviteEvent cie;
    uint8_t *pin = buddha_heap_talloc(MAX_CRYPTO_PRIVATE, 0);
    if (!pin)
        return;
    declare = buddha_heap_find(0, HEAP_MARK_DECLARE);
    if (!declare)
    {
        LogError("declare not found");
        return;
    }
    if (os_memcmp(packet->cluster, _cluster_apis[CLUSTER_API_INTERNAL].cluster, MAX_CLUSTER_ID))
        return;
    if (os_memcmp(packet->dst, terminal_local(), MAX_TERMINAL_ID))
        return;

    ecdh_shared_secret(pin, (EccPoint *)packet->opt, declare, 0);

    packet->datalen = AES128CBCDecrypt(packet->data, packet->data, packet->datalen, pin, pin);
    for (i = 0; i < packet->datalen - 1; i++)
        packet->data[MAX_CLUSTER_ID + MAX_CLUSTER_PIN] += packet->data[i];
    if (packet->data[MAX_CLUSTER_ID + MAX_CLUSTER_PIN])
    {
        LogError("invite cs error");
        return;
    }

    LogAction("cluster:%02x%02x%02x%02x",
        packet->data[0],
        packet->data[1],
        packet->data[2],
        packet->data[3]);

    cluster_join(packet->data, packet->data + MAX_CLUSTER_ID);

    cluster_send(packet->data, packet->src, OPCODE_INTERNAL_ACCEPT, 0, 0, 0, 0, 0);

    buddha_heap_destory(declare);

    cie.cluster = packet->data;
    cie.terminal = packet->src;
    cie.pin = packet->data + MAX_CLUSTER_ID;
    dispatch(CLUSTER_EVENT_INVITE, &cie);
}

static void packet_internal_accept(PacketCluster *packet)
{
    ClusterAcceptEvent cae;

    if (os_memcmp(packet->dst, terminal_local(), MAX_TERMINAL_ID))
        return;

    cae.cluster = packet->cluster;
    cae.terminal = packet->src;

    dispatch(CLUSTER_EVENT_ACCEPT, &cae);
}

static void packet_time_sync(PacketCluster *packet)
{
    ClusterTimeEvent cte;

    if (os_memcmp(packet->dst, terminal_local(), MAX_TERMINAL_ID))
        return;

    cte.time = network_ntohl(*(uint32_t *)packet->opt);
    dispatch(CLUSTER_EVENT_TIME, &cte);
}

static void api_listener(uint8_t code, const void *evt)
{
    if (CLUSTER_EVENT_PACKET == code)
    {
        PacketCluster *packet = (PacketCluster *)evt;

        if (OPCODE_AUTH_RSP == packet->opcode)
        {
            packet_auth_rsp(packet);
        }
        else if (OPCODE_AUTH_CONFIRM == packet->opcode)
        {
            packet_auth_confirm(packet);
        }
        else if (OPCODE_INTERNAL_DECLARE == packet->opcode)
        {
            packet_internal_declare(packet);
        }
        else if (OPCODE_INTERNAL_INVITE == packet->opcode)
        {
            packet_internal_invite(packet);
        }
        else if (OPCODE_INTERNAL_ACCEPT == packet->opcode)
        {
            packet_internal_accept(packet);
        }
        else if (OPCODE_TIME_RESP == packet->opcode)
        {
            packet_time_sync(packet);
        }
    }
}

static void cluster_api(const uint8_t * id, const uint8_t * pin)
{
    Cluster *cluster = 0;
    while ((cluster = buddha_heap_find(cluster, HEAP_MARK_CLUSTER)) && os_memcmp(cluster->id, id, MAX_CLUSTER_ID));
    if (!cluster)
    {
        cluster = buddha_heap_alloc(sizeof(Cluster), HEAP_MARK_CLUSTER);
        if (!cluster)
            return;
        os_memset(cluster, 0, sizeof(Cluster));
        
        os_memcpy(cluster->id, id, MAX_CLUSTER_ID);
    }

    os_memcpy(cluster->pin, pin, MAX_CLUSTER_PIN);

    cluster->leave = 0;
}

void cluster_init(const uint8_t *terminal, uint16_t reset)
{
    int api = 0;

    _terminal_reset = reset;

    os_memset(_terminal_local, 0, MAX_TERMINAL_ID);
    if (terminal)
        os_memcpy(_terminal_local, terminal, MAX_TERMINAL_ID);

    _cloud.state = CLOUD_STATE_INIT;
    _cloud.fp = -1;
    _lanwork.state = LANWORK_STATE_INIT;
    _lanwork.fp = -1;

    cluster_listen(api_listener);

    for (api = 0; api < sizeof(_cluster_apis) / sizeof(_cluster_apis[0]); api++)
        cluster_api(_cluster_apis[api].cluster, _cluster_apis[api].pin);
}

void cluster_update(void)
{
    uint8_t *cleanup = 0;
    Cluster *cluster = 0;
    if (LANWORK_STATE_INIT == _lanwork.state)
    {
        if (terminal_valid())
        {
            if (_lanwork.fp >= 0)
                network_udp_close(_lanwork.fp);
            _lanwork.fp = network_udp_create(PORT_LANWORK);
            if (_lanwork.fp >= 0)
            {
                _lanwork.state = LANWORK_STATE_SERVICE;
                dispatch(CLUSTER_EVENT_LANWORK_STARTED, 0);
            }
        }
    }
    if (LANWORK_STATE_SERVICE == _lanwork.state)
    {
        if (lanwork_receiver() >= 0 && lanwork_sender() >= 0)
        {
        }
    }

    if (CLOUD_STATE_INIT == _cloud.state)
    {
        char *host = os_malloc(os_strlen(CLOUD_HOST) + 1);
        if (host)
        {
            os_strcpy(host, CLOUD_HOST);

            if (_cloud.fp > 0)
            {
                network_tcp_close(_cloud.fp);
                _cloud.fp = 0;
            }
            _cloud.fp = network_tcp_client(host, CLOUD_PORT);
            os_free(host);
            if (_cloud.fp < 0)
            {
                if (_cloud.fp < -1)
                {
                    LogError("connect %s:%d failed.", CLOUD_HOST, CLOUD_PORT);
                    _cloud.state = CLOUD_STATE_RECONNECT;
                    _cloud.timer = os_ticks();
                }
            }
            else
            {
                uint8_t *terminal_verify = buddha_heap_find(0, HEAP_MARK_TERMINAL_VERITY);
                if (!terminal_verify)
                    terminal_verify = buddha_heap_alloc(16 + 4, HEAP_MARK_TERMINAL_VERITY);
                if (terminal_verify)
                {
                    int i = 0;
                    for (i = 0; i < 16; i++)
                        terminal_verify[i] = os_rand();
                    *(uint32_t *)(terminal_verify + 16) = os_ticks();

                    cluster_send(_cluster_apis[CLUSTER_API_VERIFY].cluster, 0, 
                        OPCODE_AUTH_REQ,
                        0, 0, 
                        terminal_verify, 16, 
                        CLUSTER_SEND_FLAG_CLOUD);
                    _cloud.state = CLOUD_STATE_AUTH;
                    LogAction("cloud auth req");
                }
            }
        }
    }
    if (CLOUD_STATE_RECONNECT == _cloud.state)
    {
        if (os_ticks_from(_cloud.timer) > os_ticks_ms(5000))
        {
            LogAction("cloud reconnect");
            _cloud.state = CLOUD_STATE_INIT;
        }
    }
    if (CLOUD_STATE_AUTH == _cloud.state)
    {
        if (cloud_receiver() >= 0 && cloud_sender() >= 0)
        {
        }
    }
    if (CLOUD_STATE_SERVICE == _cloud.state)
    {
        if (cloud_receiver() >= 0 && cloud_sender() >= 0)
        {
        }
    }

    cluster = 0;
    while ((cluster = buddha_heap_find(cluster, HEAP_MARK_CLUSTER)))
    {
        if (cluster->leave)
        {
            PacketCluster *packet = 0;
            while ((packet = buddha_heap_find(packet, HEAP_MARK_PACKET)))
            {
                if (!os_memcmp(packet->cluster, cluster->id, MAX_CLUSTER_ID))
                {
                    if (packet->lanwork || packet->cloud)
                        break;
                }
            }
            if (!packet)
            {
                buddha_heap_destory(cluster);
                continue;
            }
        }
    }

    cleanup = buddha_heap_find(0, HEAP_MARK_DECLARE);
    if (cleanup && os_ticks_from(*(uint32_t *)(cleanup + MAX_CRYPTO_PRIVATE + MAX_CRYPTO_PUBLIC)) > os_ticks_ms(10000))
    {
        LogAction("declare cleanup");
        buddha_heap_destory(cleanup);
    }

    cleanup = buddha_heap_find(0, HEAP_MARK_TERMINAL_VERITY);
    if (cleanup && os_ticks_from(*(uint32_t *)(cleanup + 16)) > os_ticks_ms(10000))
    {
        LogAction("verify cleanup");
        buddha_heap_destory(cleanup);
    }

    cleanup = 0;
    while ((cleanup = buddha_heap_find(cleanup, HEAP_MARK_PACKET)))
    {
        if (os_ticks_from(((PacketCluster *)cleanup)->time) > os_ticks_ms(5000))
        {
            ((PacketCluster *)cleanup)->time = 0;
            buddha_heap_free(cleanup);
        }
    }
}

const uint8_t * local_addr(void)
{
    return (const uint8_t *)_local_addr;
}

const uint8_t * bcast_addr(void)
{
    return (const uint8_t *)_bcast_addr;
}

int terminal_valid(void)
{
    if (!os_memcmp(_terminal_local, local_addr(), MAX_TERMINAL_ID))
        return 0;

    if (!os_memcmp(_terminal_local + 1, bcast_addr(), 2UL))
        return 0;

    return 1;
}

const uint8_t *terminal_local(void)
{
    return _terminal_local;
}

void cluster_listen(ClusterListenHandler listener)
{
    ClusterListenHandler *l = (ClusterListenHandler *)buddha_heap_alloc(sizeof(ClusterListenHandler), HEAP_MARK_CLUSTER_LISTENER);
    if (!l)
        return;
    *l = listener;
}

void cluster_unlisten(ClusterListenHandler listener)
{
    ClusterListenHandler *l = 0;
    while ((l = (ClusterListenHandler *)buddha_heap_find(l, HEAP_MARK_CLUSTER_LISTENER)) && *l != listener);
    if (l)
        buddha_heap_destory(l);
}

void cluster_join(const uint8_t *id, const uint8_t *pin)
{
    int i = 0;
    Cluster *cluster = 0;
    while ((cluster = (Cluster *)buddha_heap_find(cluster, HEAP_MARK_CLUSTER)) && os_memcmp(id, cluster->id, MAX_CLUSTER_ID));
    if (!cluster)
    {
        cluster = (Cluster *)buddha_heap_alloc(sizeof(Cluster), HEAP_MARK_CLUSTER);
        if (!cluster)
            return;
        os_memset(cluster, 0, sizeof(Cluster));
        
        os_memcpy(cluster->id, id, MAX_CLUSTER_ID);
    }

    os_memcpy(cluster->pin, pin, MAX_CLUSTER_PIN);

    cluster->leave = 0;

    cluster_send(id, 0, OPCODE_CLUSTER_JOIN, 0, 0, 0, 0, 0);

    dispatch(CLUSTER_EVENT_JOIN, id);

    LogAction("cluster:%02x%02x%02x%02x%02x%02x pin:%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x", 
        id[0],
        id[1],
        id[2],
        id[3],
        id[4],
        id[5],
        pin[0],
        pin[1],
        pin[2],
        pin[3],
        pin[4],
        pin[5],
        pin[6],
        pin[7],
        pin[8],
        pin[9],
        pin[10],
        pin[11],
        pin[12],
        pin[13],
        pin[14],
        pin[15]);
}

void cluster_leave(const uint8_t *id)
{
    Cluster *cluster = 0;
    while ((cluster = (Cluster *)buddha_heap_find(cluster, HEAP_MARK_CLUSTER)) && os_memcmp(id, cluster->id, MAX_CLUSTER_ID));
    if (!cluster)
        return;
    
    cluster->leave = 1;
    cluster_send(id, 0, OPCODE_CLUSTER_LEAVE, 0, 0, 0, 0, 0);

    dispatch(CLUSTER_EVENT_LEAVE, id);
}

uint8_t *cluster_declare(uint8_t type, uint8_t lan)
{
    uint8_t i = 0;
    uint8_t *declare = buddha_heap_find(0, HEAP_MARK_DECLARE);
    if (!declare)
        declare = buddha_heap_alloc(MAX_CRYPTO_PRIVATE + MAX_CRYPTO_PUBLIC + 4, HEAP_MARK_DECLARE);
    if (!declare)
        return 0;
    *(uint32_t *)(declare + MAX_CRYPTO_PRIVATE + MAX_CRYPTO_PUBLIC) = os_ticks();

    for (i = 0; i < MAX_CRYPTO_PRIVATE; i++)
        declare[i] = os_rand();
    ecc_make_key((EccPoint *)(declare + MAX_CRYPTO_PRIVATE), declare, declare);

    LogAction("type:%d lan:%d", type, lan);

    if (lan)
    {
        cluster_send(
            _cluster_apis[CLUSTER_API_INTERNAL].cluster, 0, 
            OPCODE_INTERNAL_DECLARE, 
            &type, 1,
            declare + MAX_CRYPTO_PRIVATE, MAX_CRYPTO_PUBLIC,
            CLUSTER_SEND_FLAG_LANWORK);
    }
    return declare + MAX_CRYPTO_PRIVATE;
}

void cluster_invite(const uint8_t *id, const uint8_t *terminal, const uint8_t *pin, uint8_t *pub, uint8_t *crypto)
{
    int i;
    uint32_t datalen = MAX_CLUSTER_ID + MAX_CLUSTER_PIN + 1;
    uint8_t *data = buddha_heap_talloc(datalen + 16, 0);
    if (!data)
        return;
    os_memcpy(data, id, MAX_CLUSTER_ID);
    os_memcpy(data + MAX_CLUSTER_ID, pin, MAX_CLUSTER_PIN);
    data[MAX_CLUSTER_ID + MAX_CLUSTER_PIN] = 0;
    for (i = 0; i < MAX_CLUSTER_ID + MAX_CLUSTER_PIN; i++)
        data[MAX_CLUSTER_ID + MAX_CLUSTER_PIN] += data[i];
    data[MAX_CLUSTER_ID + MAX_CLUSTER_PIN] = 0 - data[MAX_CLUSTER_ID + MAX_CLUSTER_PIN];

    datalen = AES128CBCEncrypt(data, data, datalen, crypto, crypto);

    cluster_send(
        _cluster_apis[CLUSTER_API_INTERNAL].cluster, terminal, 
        OPCODE_INTERNAL_INVITE,
        pub, MAX_CRYPTO_PUBLIC, 
        data, datalen,
        CLUSTER_SEND_FLAG_LANWORK);
}

void cluster_time_sync(void)
{
    cluster_send(_cluster_apis[CLUSTER_API_TIME].cluster, 0,
        OPCODE_TIME_REQ,
        0, 0,
        0, 0,
        CLUSTER_SEND_FLAG_CLOUD);
}

void cluster_send(
    const uint8_t * cluster, const uint8_t *terminal, 
    uint8_t opcode,
    const uint8_t *opt, uint8_t optlen, 
    const uint8_t *data, uint16_t datalen,
    uint8_t flags)
{
	Cluster *c = 0;
	PacketCluster *packet;

    if (!(flags & (CLUSTER_SEND_FLAG_CLOUD | CLUSTER_SEND_FLAG_LANWORK)))
        flags |= CLUSTER_SEND_FLAG_CLOUD | CLUSTER_SEND_FLAG_LANWORK;

    c = 0;
    while ((c = buddha_heap_find(c, HEAP_MARK_CLUSTER)) && os_memcmp(c->id, cluster, MAX_CLUSTER_ID));
    if (!c)
    {
        LogError("unknown cluster %02x%02x%02x%02x%02x%02x",
            cluster[0],
            cluster[1],
            cluster[2],
            cluster[3],
            cluster[4],
            cluster[5]);
        return;
    }

    packet = (PacketCluster *)buddha_heap_alloc(sizeof(PacketCluster) + MAX_TERMINAL_ID * 2 + MAX_CLUSTER_ID + optlen + datalen + 4 + 16, HEAP_MARK_PACKET);
    if (!packet)
        return;
    os_memset(packet, 0, sizeof(PacketCluster));
    packet->lanwork = !!(flags & CLUSTER_SEND_FLAG_LANWORK);
    packet->cloud = !!(flags & CLUSTER_SEND_FLAG_CLOUD);
    packet->retry = 0;
    packet->opcode = opcode;
    packet->reset = _terminal_reset;

    if (!terminal || !os_memcmp(terminal, bcast_addr(), MAX_TERMINAL_ID))
        packet->seq = _session_bcast++;
    else
        packet->seq = _session_peer++;

    packet->dst = (uint8_t *)(packet + 1);
    if (terminal)
        os_memcpy(packet->dst, terminal, MAX_TERMINAL_ID);
    else
        os_memset(packet->dst, 0xff, MAX_TERMINAL_ID);

    packet->src = ((uint8_t *)(packet + 1)) + MAX_TERMINAL_ID;
    os_memcpy(packet->src, terminal_local(), MAX_TERMINAL_ID);

    packet->cluster = ((uint8_t *)(packet + 1)) + MAX_TERMINAL_ID * 2;
    os_memcpy(packet->cluster, cluster, MAX_CLUSTER_ID);

    packet->opt = ((uint8_t *)(packet + 1)) + MAX_TERMINAL_ID * 2 + MAX_CLUSTER_ID;
    if (optlen)
        os_memcpy(packet->opt, opt, optlen);
    packet->optlen = optlen;
    
    packet->data = ((uint8_t *)(packet + 1)) + MAX_TERMINAL_ID * 2 + MAX_CLUSTER_ID + optlen;
    if (datalen)
        os_memcpy(packet->data, data, datalen);
    *(uint32_t *)(packet->data + datalen) = network_htonl(packet->seq);
    datalen = AES128CBCEncrypt(packet->data, packet->data, datalen + 4, c->pin, c->pin + 16);
    packet->datalen = datalen;
    packet = (PacketCluster *)buddha_heap_realloc(packet, sizeof(PacketCluster) + MAX_TERMINAL_ID * 2 + MAX_CLUSTER_ID + optlen + datalen);
    packet->dst = (uint8_t *)(packet + 1);
    packet->src = ((uint8_t *)(packet + 1)) + MAX_TERMINAL_ID;
    packet->cluster = ((uint8_t *)(packet + 1)) + MAX_TERMINAL_ID * 2;
    packet->opt = ((uint8_t *)(packet + 1)) + MAX_TERMINAL_ID * 2 + MAX_CLUSTER_ID;
    packet->data = ((uint8_t *)(packet + 1)) + MAX_TERMINAL_ID * 2 + MAX_CLUSTER_ID + optlen;

    if (OPCODE_RMP_ACK == opcode)
        return;

    LogAction("channel:%7s cluster:%02x%02x%02x%02x%02x%02x|dst:%02x%02x%02x%02x|src:%02x%02x%02x%02x|opcode:%3d|reset:%5d|seq:%d|optlen:%d|datalen:%d",
        flags | CLUSTER_SEND_FLAG_CLOUD ? (flags | CLUSTER_SEND_FLAG_LANWORK ? "both" : "cloud") : "lanwork",
        cluster[0],
        cluster[1],
        cluster[2],
        cluster[3],
        cluster[4],
        cluster[5],
        packet->dst[0],
        packet->dst[1],
        packet->dst[2],
        packet->dst[3],
        packet->src[0],
        packet->src[1],
        packet->src[2],
        packet->src[3],
        opcode,
        _terminal_reset,
        packet->seq,
        optlen,
        datalen);
}
