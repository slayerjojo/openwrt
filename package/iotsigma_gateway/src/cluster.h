#ifndef __CLUSTER_H__
#define __CLUSTER_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "env.h"
#include "opcode.h"
#include "interface_os.h"

#define MAX_CLUSTER_ID (6UL)
#define MAX_CLUSTER_PIN (32UL)

#define MAX_TERMINAL_ID (4UL)

#define MAX_CRYPTO_PUBLIC (32UL)
#define MAX_CRYPTO_PRIVATE (16UL)

#define CLUSTER_SEND_FLAG_CLOUD 0x01
#define CLUSTER_SEND_FLAG_LANWORK 0x02

#define CLUSTER_EVENT_PACKET 0x01
#define CLUSTER_EVENT_TERMINAL_ALLOC 0x02
#define CLUSTER_EVENT_LANWORK_STARTED 0x03
#define CLUSTER_EVENT_CLOUD_STARTED 0x04
#define CLUSTER_EVENT_LEAVE 0x06
#define CLUSTER_EVENT_JOIN 0x07
#define CLUSTER_EVENT_DECLARE 0x08
#define CLUSTER_EVENT_INVITE 0x09
#define CLUSTER_EVENT_ACCEPT 0x0a
#define CLUSTER_EVENT_TIME 0x0b

typedef void (*ClusterListenHandler)(uint8_t code, const void *evt);

typedef struct
{
    uint8_t lanwork:1;
    uint8_t cloud:1;
    uint8_t retry:6;
    uint8_t opcode; 
    uint16_t reset;
    uint32_t time;
    uint32_t seq;
    uint8_t *dst; uint8_t *src; uint8_t *cluster;
    uint8_t *opt; uint8_t optlen; 
    uint8_t *data; uint16_t datalen;
}__attribute__((packed)) PacketCluster;

typedef struct
{
    uint8_t type;
    uint8_t *terminal;
    uint8_t crypto[MAX_CRYPTO_PRIVATE];
    uint8_t pub[MAX_CRYPTO_PUBLIC];
}ClusterDeclareEvent;

typedef struct
{
    uint8_t *cluster;
    uint8_t *terminal;
    uint8_t *pin;
}ClusterInviteEvent;

typedef struct
{
    uint8_t *cluster;
    uint8_t *terminal;
}ClusterAcceptEvent;

typedef struct
{
    uint32_t time;
}ClusterTimeEvent;

void cluster_init(const uint8_t *terminal, uint16_t reset);
void cluster_update(void);

const uint8_t *local_addr(void);
const uint8_t *bcast_addr(void);
int terminal_valid(void);
const uint8_t *terminal_local(void);
void cluster_listen(ClusterListenHandler listener);
void cluster_unlisten(ClusterListenHandler listener);
void cluster_join(const uint8_t *id, const uint8_t *pin);
void cluster_leave(const uint8_t *id);
/*
 * type: type of terminal
 * lan: 0 - not broadcast to lan, 1 - broadcast to lan
 */
uint8_t *cluster_declare(uint8_t type, uint8_t lan);
/*
 * id: cluster id
 * terminal: terminal id
 * pin: pin of cluster
 * pub: public key of terminal
 * crypto: shared key of session
 */
void cluster_invite(const uint8_t *id, const uint8_t *terminal, const uint8_t *pin, uint8_t *pub, uint8_t *crypto);
void cluster_time_sync(void);
void cluster_send(
    const uint8_t * cluster, const uint8_t *terminal, 
    uint8_t opcode,
    const uint8_t *opt, uint8_t optlen, 
    const uint8_t *data, uint16_t datalen, 
    uint8_t flags);

#ifdef __cplusplus
}
#endif

#endif
