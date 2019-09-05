#include "sigma_layer_cluster.h"
#include "sigma_layer_link.h"
#include "sigma_opcode.h"
#include "interface_network.h"
#include "interface_os.h"
#include "interface_crypto.h"
#include "driver_linux.h"
#include "log.h"
#include "aes.h"

#define MAX_RETRY_CLUSTER (5)

typedef struct
{
    uint8_t opcode;
    uint8_t cluster[MAX_CLUSTER_ID];
}__attribute__((packed)) HeaderSigmaLayerCluster;

typedef struct
{
    HeaderSigmaLayerCluster header;
}__attribute__((packed)) PacketSigmaLayerClusterJoin;

typedef struct
{
    HeaderSigmaLayerCluster header;
}__attribute__((packed)) PacketSigmaLayerClusterLeave;

typedef struct
{
    HeaderSigmaLayerCluster header;
    uint32_t session;
    uint8_t key[MAX_CRYPTO_PUBLIC];
    uint8_t cluster[MAX_CLUSTER_ID];
    uint8_t pin[MAX_CLUSTER_PIN + 16];
}__attribute__((packed)) PacketSigmaLayerClusterInvite;

typedef struct
{
    HeaderSigmaLayerCluster header;
    uint32_t session;
    int code;
}__attribute__((packed)) PacketSigmaLayerClusterReject;

typedef struct
{
    HeaderSigmaLayerCluster header;
    uint32_t session;
}__attribute__((packed)) PacketSigmaLayerClusterAccept;

typedef struct
{
    HeaderSigmaLayerCluster header;
    uint16_t type;
    uint16_t interval;
}__attribute__((packed)) PacketSigmaLayerClusterDiscover;

typedef struct
{
    HeaderSigmaLayerCluster header;
    uint16_t type;
    uint8_t key[MAX_CRYPTO_PUBLIC];
}__attribute__((packed)) PacketSigmaLayerClusterDeclare;

typedef struct
{
    HeaderSigmaLayerCluster header;
    uint8_t type;
    uint8_t payload[];
}__attribute__((packed)) PacketSigmaLayerClusterPublish;

typedef struct cluster_listener
{
    struct cluster_listener *next;

    SigmaLayerClusterListener listener;
    void *ctx;
}ClusterListener;

typedef struct cluster
{
    struct cluster *next;

    uint32_t timer_declare;
    uint16_t interval_declare;
    uint16_t type;
    uint8_t id[MAX_CLUSTER_ID];
    uint8_t pin[MAX_CLUSTER_PIN];

    ClusterListener *listeners;
}Cluster;

static Cluster *_clusters = 0;
static ClusterListener *_monitors = 0;

void slc_init(void)
{
    _clusters = 0;
    _monitors = 0;
}

void slc_update(void)
{
    uint8_t *terminal = 0;
    HeaderSigmaLayerCluster *header = 0;
    int ret = sll_recv(SLL_TYPE_CLUSTER, &terminal, (void **)&header);
    if (ret > 0)
    {
        if (OPCODE_CLUSTER_INVITE == header->opcode)
        {
            PacketSigmaLayerClusterInvite *packet = (PacketSigmaLayerClusterInvite *)header;

            packet->session = network_ntohl(packet->session);

            uint8_t shared[MAX_CRYPTO_PIN] = {0};
            crypto_key_shared(shared, packet->key);
            crypto_decrypto(packet->pin, packet->pin, MAX_CLUSTER_PIN + 16, shared);
        }
        else if (OPCODE_CLUSTER_ACCEPT == header->opcode)
        {
            PacketSigmaLayerClusterAccept *packet = (PacketSigmaLayerClusterAccept *)header;
            packet->session = network_ntohl(packet->session);
        }
        else if (OPCODE_CLUSTER_REJECT == header->opcode)
        {
            PacketSigmaLayerClusterReject *packet = (PacketSigmaLayerClusterReject *)header;
            packet->session = network_ntohl(packet->session);
            packet->code = network_ntohl(packet->code);
        }
        else if (OPCODE_CLUSTER_DISCOVER == header->opcode)
        {
            PacketSigmaLayerClusterDiscover *packet = (PacketSigmaLayerClusterDiscover *)header;
            packet->interval = network_ntohs(packet->interval);
            packet->type = network_ntohs(packet->type);
        }
    }

    uint8_t event = 0;

    Cluster *c = _clusters;
    while (c)
    {
        if (c->timer_declare && os_ticks_from(c->timer_declare) >= c->interval_declare)
        {
            c->timer_declare = 0;
            c->interval_declare = 0;

            slc_declare(c->id);
        }
        if (ret > 0 && (!os_memcmp(header->cluster, slc_cluster_bcast(), MAX_CLUSTER_ID) || !os_memcmp(c->id, header->cluster, MAX_CLUSTER_ID)))
        {
            if (OPCODE_CLUSTER_JOIN == header->opcode)
                event = EVENT_CLUSTER_JOIN;
            else if (OPCODE_CLUSTER_LEAVE == header->opcode)
                event = EVENT_CLUSTER_LEAVE;
            else if (OPCODE_CLUSTER_INVITE == header->opcode)
                event = EVENT_CLUSTER_INVITE;
            else if (OPCODE_CLUSTER_REJECT == header->opcode)
                event = EVENT_CLUSTER_REJECT;
            else if (OPCODE_CLUSTER_ACCEPT == header->opcode)
                event = EVENT_CLUSTER_ACCEPT;
            else if (OPCODE_CLUSTER_DECLARE == header->opcode)
                event = EVENT_CLUSTER_DECLARE;
            else if (OPCODE_CLUSTER_DISCOVER == header->opcode)
            {
                PacketSigmaLayerClusterDiscover *packet = (PacketSigmaLayerClusterDiscover *)header;
                if (packet->type == 0xffff || packet->type == c->type)
                {
                    c->timer_declare = os_ticks();
                    c->interval_declare = packet->interval;
                }
            }
            else if (OPCODE_CLUSTER_PUBLISH == header->opcode)
            {
                event = EVENT_CLUSTER_PUBLISH;

                PacketSigmaLayerClusterPublish *packet = (PacketSigmaLayerClusterPublish *)header;
                ret = sizeof(PacketSigmaLayerClusterPublish) + crypto_decrypto(packet + 1, packet + 1, ret - sizeof(PacketSigmaLayerClusterPublish), c->pin);
            }

            if (event)
            {
                ClusterListener *l = c->listeners;
                while (l)
                {
                    l->listener(
                        header->cluster, terminal, 
                        event, 
                        header + 1, ret - sizeof(HeaderSigmaLayerCluster), 
                        l->ctx);
                    l = l->next;
                }
            }
        }
        c = c->next;
    }

    if (event)
    {
        ClusterListener *l = _monitors;
        while (l)
        {
            l->listener(
                header->cluster, terminal, 
                event, 
                header + 1, ret - sizeof(HeaderSigmaLayerCluster),
                l->ctx);
            l = l->next;
        }
    }
}

const uint8_t *slc_cluster_bcast(void)
{
    return (const uint8_t *)"\xff\xff\xff\xff\xff\xff";
}

int slc_monitor(SigmaLayerClusterListener listener, void *ctx)
{
    ClusterListener *cl = (ClusterListener *)os_malloc(sizeof(ClusterListener));
    if (!cl)
    {
        LogError("out of memory");
        return -1;
    }
    cl->listener = listener;
    cl->ctx = ctx;
    return 0;
}

int slc_listen(const uint8_t *cluster, SigmaLayerClusterListener listener, void *ctx)
{
    Cluster *c = _clusters;
    while (c)
    {
        if (!os_memcmp(cluster, c->id, MAX_CLUSTER_ID))
            break;
        c = c->next;
    }
    if (!c)
    {
        LogDump(LOG_LEVEL_ERROR, "cluster not found raw:", cluster, MAX_CLUSTER_ID);
        return -1;
    }
    ClusterListener *cl = (ClusterListener *)os_malloc(sizeof(ClusterListener));
    if (!cl)
    {
        LogError("out of memory");
        return -1;
    }
    cl->listener = listener;
    cl->ctx = ctx;
    cl->next = c->listeners;
    c->listeners = cl;
    return 0;
}

void slc_unlisten(const uint8_t *cluster, SigmaLayerClusterListener listener, void *ctx)
{
    Cluster *c = _clusters;
    while (c)
    {
        if (!os_memcmp(cluster, c->id, MAX_CLUSTER_ID))
            break;
        c = c->next;
    }
    if (!c)
    {
        LogDump(LOG_LEVEL_ACTION, "cluster not found raw:", cluster, MAX_CLUSTER_ID);
        return;
    }
    ClusterListener *cl = c->listeners, *prev = 0;
    while(cl)
    {
        if (cl->listener == listener && cl->ctx == ctx)
            break;

        prev = cl;
        cl = cl->next;
    }
    if (cl)
    {
        if (prev)
            prev->next = cl->next;
        else
            c->listeners = cl->next;
        os_free(cl);
    }
}

int slc_join(const uint8_t *cluster, const uint8_t *pin, uint16_t type)
{
    Cluster *c = _clusters;
    while (c)
    {
        if (!os_memcmp(c->id, cluster, MAX_CLUSTER_ID))
            break;
        c = c->next;
    }
    if (!c)
    {
        c = (Cluster *)os_malloc(sizeof(Cluster));
        if (!c)
            return -1;
        os_memset(c, 0, sizeof(Cluster));

        c->next = _clusters;
        _clusters = c;

        os_memcpy(c->id, cluster, MAX_CLUSTER_ID);
    }
    c->type = type;
    os_memcpy(c->pin, pin, MAX_CLUSTER_PIN);

    PacketSigmaLayerClusterJoin *packet = (PacketSigmaLayerClusterJoin *)sll_report(SLL_TYPE_CLUSTER, sizeof(PacketSigmaLayerClusterJoin), SLL_FLAG_SEND_PATH_ALL);
    if (packet)
    {
        packet->header.opcode = OPCODE_CLUSTER_JOIN;
        os_memcpy(packet->header.cluster, cluster, MAX_CLUSTER_ID);
    }
    return 0;
}

void slc_leave(const uint8_t *cluster)
{
    Cluster *c = _clusters, *prev = 0;
    while (c)
    {
        if (!os_memcmp(c->id, cluster, MAX_CLUSTER_ID))
            break;
        prev = c;
        c = c->next;
    }
    if (!c)
        return;
    if (prev)
        prev->next = c->next;
    else
        _clusters = c->next;
    while (c->listeners)
    {
        ClusterListener *cl = c->listeners;
        c->listeners = c->listeners->next;
        os_free(cl);
    }
    os_free(c);

    PacketSigmaLayerClusterLeave *packet = (PacketSigmaLayerClusterLeave *)sll_report(SLL_TYPE_CLUSTER, sizeof(PacketSigmaLayerClusterLeave), SLL_FLAG_SEND_PATH_ALL);
    if (packet)
    {
        packet->header.opcode = OPCODE_CLUSTER_LEAVE;
        os_memcpy(packet->header.cluster, cluster, MAX_CLUSTER_ID);
    }
}

void slc_invite(const uint8_t *cluster, const uint8_t *terminal, uint32_t session, const uint8_t *key)
{
    Cluster *c = _clusters, *prev = 0;
    while (c)
    {
        if (!os_memcmp(c->id, cluster, MAX_CLUSTER_ID))
            break;
        prev = c;
        c = c->next;
    }
    if (!c)
        return;

    PacketSigmaLayerClusterInvite *packet = (PacketSigmaLayerClusterInvite *)sll_send(SLL_TYPE_CLUSTER, terminal, sizeof(PacketSigmaLayerClusterInvite), SLL_FLAG_SEND_PATH_ALL, MAX_RETRY_CLUSTER);
    if (packet)
    {
        packet->header.opcode = OPCODE_CLUSTER_INVITE;
        os_memcpy(packet->header.cluster, slc_cluster_bcast(), MAX_CLUSTER_ID);

        packet->session = network_htonl(session);

        os_memcpy(packet->key, crypto_key_public(), MAX_CRYPTO_PUBLIC);
        os_memcpy(packet->cluster, cluster, MAX_CLUSTER_ID);

        uint8_t shared[MAX_CRYPTO_PIN] = {0};
        crypto_key_shared(shared, key);
        crypto_encrypto(packet->pin, c->pin, MAX_CLUSTER_PIN, shared);
    }
}

void slc_reject(const uint8_t *cluster, const uint8_t *terminal, uint32_t session, int code)
{
    Cluster *c = _clusters, *prev = 0;
    while (c)
    {
        if (!os_memcmp(c->id, cluster, MAX_CLUSTER_ID))
            break;
        prev = c;
        c = c->next;
    }
    if (!c)
        return;

    PacketSigmaLayerClusterReject *packet = (PacketSigmaLayerClusterReject *)sll_send(SLL_TYPE_CLUSTER, terminal, sizeof(PacketSigmaLayerClusterReject), SLL_FLAG_SEND_PATH_ALL, MAX_RETRY_CLUSTER);
    if (packet)
    {
        packet->header.opcode = OPCODE_CLUSTER_REJECT;
        os_memcpy(packet->header.cluster, cluster, MAX_CLUSTER_ID);

        packet->session = network_htonl(session);

        packet->code = network_htonl(code);
    }
}

void slc_accept(const uint8_t *cluster, const uint8_t *terminal, uint32_t session)
{
    Cluster *c = _clusters, *prev = 0;
    while (c)
    {
        if (!os_memcmp(c->id, cluster, MAX_CLUSTER_ID))
            break;
        prev = c;
        c = c->next;
    }
    if (!c)
        return;

    PacketSigmaLayerClusterAccept *packet = (PacketSigmaLayerClusterAccept *)sll_send(SLL_TYPE_CLUSTER, terminal, sizeof(PacketSigmaLayerClusterAccept), SLL_FLAG_SEND_PATH_ALL, MAX_RETRY_CLUSTER);
    if (packet)
    {
        packet->header.opcode = OPCODE_CLUSTER_ACCEPT;
        os_memcpy(packet->header.cluster, cluster, MAX_CLUSTER_ID);

        packet->session = network_htonl(session);
    }
}

void slc_discover(const uint8_t *cluster, const uint8_t *terminal, uint16_t type, uint16_t interval)
{
    PacketSigmaLayerClusterDiscover *packet = 0;
    if (!os_memcmp(terminal, sll_terminal_bcast(), MAX_TERMINAL_ID))
    {
        packet = (PacketSigmaLayerClusterDiscover *)sll_report(SLL_TYPE_CLUSTER, sizeof(PacketSigmaLayerClusterDiscover), SLL_FLAG_SEND_PATH_ALL);
    }
    else
    {
        packet = (PacketSigmaLayerClusterDiscover *)sll_send(SLL_TYPE_CLUSTER, terminal, sizeof(PacketSigmaLayerClusterDiscover), SLL_FLAG_SEND_PATH_ALL, MAX_RETRY_CLUSTER);
    }
    if (packet)
    {
        packet->header.opcode = OPCODE_CLUSTER_DISCOVER;
        os_memcpy(packet->header.cluster, cluster, MAX_CLUSTER_ID);

        packet->type = network_htons(type);
        packet->interval = network_htons(interval);
    }
}

void slc_declare(const uint8_t *cluster, const uint8_t *terminal)
{
    Cluster *c = _clusters, *prev = 0;
    while (c)
    {
        if (!os_memcmp(c->id, cluster, MAX_CLUSTER_ID))
            break;
        prev = c;
        c = c->next;
    }
    if (!c)
        return;

    PacketSigmaLayerClusterDeclare *packet = 0;
    if (!os_memcmp(terminal, sll_terminal_bcast(), MAX_TERMINAL_ID))
    {
        packet = (PacketSigmaLayerClusterDeclare *)sll_report(SLL_TYPE_CLUSTER, sizeof(PacketSigmaLayerClusterDeclare), SLL_FLAG_SEND_PATH_ALL);
    }
    else
    {
        packet = (PacketSigmaLayerClusterDeclare *)sll_send(SLL_TYPE_CLUSTER, terminal, sizeof(PacketSigmaLayerClusterDeclare), SLL_FLAG_SEND_PATH_ALL, MAX_RETRY_CLUSTER);
    }
    if (packet)
    {
        packet->header.opcode = OPCODE_CLUSTER_DECLARE;
        os_memcpy(packet->header.cluster, cluster, MAX_CLUSTER_ID);

        packet->type = network_htons(c->type);
        os_memcpy(packet->key, crypto_key_public(), MAX_CRYPTO_PUBLIC);
    }
}

int slc_publish(const uint8_t *cluster, const uint8_t *terminal, uint8_t type, const void *payload, uint32_t size)
{
    Cluster *c = _clusters, *prev = 0;
    while (c)
    {
        if (!os_memcmp(c->id, cluster, MAX_CLUSTER_ID))
            break;
        prev = c;
        c = c->next;
    }
    if (!c)
        return -1;

    PacketSigmaLayerClusterPublish *packet = 0;
    if (!os_memcmp(terminal, sll_terminal_bcast(), MAX_TERMINAL_ID))
    {
        packet = (PacketSigmaLayerClusterPublish *)sll_report(SLL_TYPE_CLUSTER, sizeof(PacketSigmaLayerClusterPublish) + (size / 16 + 1) * 16, SLL_FLAG_SEND_PATH_ALL);
    }
    else
    {
        packet = (PacketSigmaLayerClusterPublish *)sll_send(SLL_TYPE_CLUSTER, terminal, sizeof(PacketSigmaLayerClusterPublish) + (size / 16 + 1) * 16, SLL_FLAG_SEND_PATH_ALL, MAX_RETRY_CLUSTER);
    }
    if (packet)
    {
        packet->header.opcode = OPCODE_CLUSTER_PUBLISH;
        os_memcpy(packet->header.cluster, cluster, MAX_CLUSTER_ID);
        packet->type = type;

        return crypto_encrypto(packet + 1, payload, size, c->pin);
    }
    return 0;
}
