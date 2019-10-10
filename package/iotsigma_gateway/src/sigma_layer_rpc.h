#ifndef __SIGMA_LAYER_RPC_H__
#define __SIGMA_LAYER_RPC_H__

#include "env.h"
#include "sigma_layer_link.h"
#include "sigma_layer_cluster.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define RET_RPC_SUCCESS (0)
#define RET_RPC_ERROR_INSUFFICIENT_SPACE (-1)
#define RET_RPC_ERROR_NOT_FOUND (-2)

typedef struct
{
    uint8_t cluster[MAX_CLUSTER_ID];
    uint8_t terminal[MAX_TERMINAL_ID];
}SigmaDomain;

typedef struct
{
    uint16_t opcode;
    uint32_t session;
}__attribute__((packed)) HeaderSigmaRPC;

typedef int (*SigmaRPCResponser)(void *ctx, const SigmaDomain *domain, int ret, const void *result, uint16_t size);

typedef struct
{
    HeaderSigmaRPC header;
    uint8_t response;
    uint8_t parameters[];
}__attribute__((packed)) PacketSigmaRPCRequester;

typedef struct _sigma_rpc_call
{
    struct _sigma_rpc_call *next;

    SigmaDomain domain;

    uint32_t time;
    uint32_t timeout;

    SigmaRPCResponser responser;
    void *ctx;

    uint8_t abort:1;
    uint16_t opcode;
    uint32_t session;
    uint16_t size;
    uint16_t pos;
    uint8_t extend[];
}__attribute__((packed)) SigmaRPCCall;

typedef int (*SigmaRPCRequester)(const SigmaDomain *domain, uint16_t opcode, uint32_t session, uint8_t response, const void *parameter, uint16_t size);

void slr_init(void);
void slr_update(void);

void slr_register(uint16_t opcode, SigmaRPCRequester req);

SigmaRPCCall *slr_create(const SigmaDomain *domain, uint16_t opcode, uint16_t size, uint16_t extend);

void *slr_extend(SigmaRPCCall *caller);
void *slr_parameter(SigmaRPCCall *caller);

void slr_abort(SigmaRPCCall *caller);

void slr_call(SigmaRPCCall *caller, uint32_t timeout, SigmaRPCResponser responser, void *ctx);
int slr_response(const SigmaDomain *domain, int ret, uint32_t session, const void *result, uint16_t size);

#ifdef __cplusplus
}
#endif

#endif