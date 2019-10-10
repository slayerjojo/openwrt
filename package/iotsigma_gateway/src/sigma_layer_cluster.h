#ifndef __SIGMA_LAYER_CLUSTER_H__
#define __SIGMA_LAYER_CLUSTER_H__

#include "env.h"
#include "interface_crypto.h"

#define MAX_CLUSTER_ID (6)
#define MAX_CLUSTER_PIN (32)

#define CLUSTER_TYPE_RPC 1
#define CLUSTER_TYPE_ENTITY 2

#define EVENT_CLUSTER_JOIN 1
#define EVENT_CLUSTER_LEAVE 2
#define EVENT_CLUSTER_INVITE 3
#define EVENT_CLUSTER_REJECT 4
#define EVENT_CLUSTER_ACCEPT 5
#define EVENT_CLUSTER_DECLARE 6
#define EVENT_CLUSTER_DISCOVER 7
#define EVENT_CLUSTER_PUBLISH 8

typedef struct
{
    uint16_t type;
    uint8_t key[MAX_CRYPTO_PUBLIC];
}__attribute__((packed)) EventClusterDeclare;

typedef struct
{
    uint32_t session;
    int code;
}__attribute__((packed)) EventClusterReject;

typedef struct
{
    uint32_t session;
}__attribute__((packed)) EventClusterAccept;

typedef struct
{
    uint16_t session;
    uint8_t key[MAX_CRYPTO_PUBLIC];
    uint8_t cluster[MAX_CLUSTER_ID];
    uint8_t pin[MAX_CLUSTER_PIN];
}__attribute__((packed)) EventClusterInvite;

typedef struct
{
    uint8_t type;
    uint8_t payload[];
}__attribute__((packed)) EventClusterPublish;

typedef void (*SLClusterListener)(
        const uint8_t *cluster, const uint8_t *terminal, 
        uint8_t event, 
        const void *parameters, uint16_t size,
        void *ctx);

void slc_init(void);
void slc_update(void);

const uint8_t *slc_cluster_bcast(void);

int slc_monitor(SLClusterListener listener, void *ctx);

int slc_listen(const uint8_t *cluster, SLClusterListener listener, void *ctx);
void slc_unlisten(const uint8_t *cluster, SLClusterListener listener, void *ctx);

int slc_join(const uint8_t *cluster, const uint8_t *pin, uint16_t type);
void slc_leave(const uint8_t *cluster);

void slc_invite(const uint8_t *cluster, const uint8_t *terminal, uint32_t session, const uint8_t *key);
void slc_reject(const uint8_t *cluster, const uint8_t *terminal, uint32_t session, int code);
void slc_accept(const uint8_t *cluster, const uint8_t *terminal, uint32_t session);

void slc_discover(const uint8_t *cluster, const uint8_t *terminal, uint16_t type, uint16_t interval);
void slc_declare(const uint8_t *cluster);

int slc_publish(const uint8_t *cluster, const uint8_t *terminal, uint8_t type, const void *payload, uint32_t size);

#endif
