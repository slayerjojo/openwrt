#ifndef __SIGMA_LINK_LAYER__
#define __SIGMA_LINK_LAYER__

#include "env.h"

#define SLL_TYPE_CRYPTO 1
#define SLL_TYPE_REGISTER 2
#define SLL_TYPE_CLUSTER 3

#define SLL_FLAG_SEND_ACK (1ul << 0)
#define SLL_FLAG_SEND_PATH_LANWORK (1ul << 1)
#define SLL_FLAG_SEND_PATH_CLOUD (1ul << 2)
#define SLL_FLAG_SEND_PATH_ALL (SLL_FLAG_SEND_PATH_LANWORK | SLL_FLAG_SEND_PATH_CLOUD)

#define MAX_TERMINAL_ID (4ul)

typedef struct
{
    uint8_t dst[MAX_TERMINAL_ID];
    uint8_t src[MAX_TERMINAL_ID];
    uint32_t sequence;
    uint16_t reset;
    uint8_t opcode:3;
    uint8_t needAck:1;
    uint8_t type:4;
    uint16_t length;
    uint8_t cs;
}__attribute__((packed)) HeaderSLLink;

typedef struct
{
    uint8_t lanwork:1;
    uint8_t cloud:1;
    uint8_t retry:6;
    uint32_t time;
    uint32_t index;
    HeaderSLLink header;
}__attribute__((packed)) PacketTransmitSLLink;

typedef struct
{
    HeaderSLLink header;
}__attribute__((packed)) PacketReceiveSLLink;

typedef struct
{
    uint8_t opcode;
}__attribute__((packed)) HeaderSigmaSublayerLink;

void sll_init(void);
void sll_update(void);

const uint8_t *sll_terminal_bcast(void);
const uint8_t *sll_terminal_local(void);

const uint8_t *sll_src(const uint8_t *src);

void *sll_send(uint8_t type, const uint8_t *dst, uint16_t size, uint8_t flags, uint8_t retry);
void *sll_report(uint8_t type, uint16_t size, uint8_t flags);
void sll_ack(const uint8_t *dst, uint16_t reset, uint32_t sequence);
void sll_retrans(const uint8_t *dst, uint16_t reset, uint32_t sequence);
int sll_recv(uint8_t type, uint8_t **dst, void **buffer);

#endif
