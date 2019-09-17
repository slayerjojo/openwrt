#ifndef __SIGMA_LAYER_ENTITY_H__
#define __SIGMA_LAYER_ENTITY_H__

#include "env.h"
#include "sigma_layer_rpc.h"

#define EVENT_ENTITY_CREATE 1
#define EVENT_ENTITY_ACTION 2
#define EVENT_ENTITY_UPDATE 3
#define EVENT_ENTITY_RELEASE 4
#define EVENT_ENTITY_RESULT 5
#define EVENT_ENTITY_FINISHED 6
#define EVENT_ENTITY_RESULT_FAILED 7

#define SLE_FLAG_SYNC 1

#define SLE_FILTER_EQUAL 1
#define SLE_FILTER_NOTEQ 2

typedef struct
{
    const SigmaDomain *domain;
    uint16_t entity;
}SLEEntity;

typedef struct
{
    uint8_t type;
    uint8_t value[];
}__attribute__((packed)) SLEValue;

typedef int (*SigmaEntityListener)(void *ctx, int event, const SLEEntity *entity, uint8_t property, const SLEValue *value);

void sle_init(void);
void sle_update(void);

int sle_create(const SigmaDomain *domain, uint8_t type, uint16_t size, SLEValue *value, SigmaEntityListener resp, void *ctx);
int sle_release(const SLEEntity *entity, SigmaEntityListener resp, void *ctx);

int sle_write(const SLEEntity *entity, uint8_t property, const SLEValue *value, SigmaEntityListener resp, void *ctx);
int sle_read(const SLEEntity *entity, uint8_t property, SLEValue *value, SigmaEntityListener resp, void *ctx);

int sle_action(const SLEEntity *entity, uint8_t action, uint8_t *parameter, uint16_t size);

uint32_t sle_iterator_create(const SigmaDomain *domain, uint8_t type, SigmaEntityListener resp, void *ctx);
void sle_iterator_release(uint32_t session);
int sle_iterator_filter(uint32_t session, uint8_t filter, uint8_t property, const SLEValue *value);
int sle_iterator_execute(uint32_t session, uint32_t last);

void sle_listen(SigmaEntityListener listener, void *ctx);
void sle_unlisten(SigmaEntityListener listener, void *ctx);

#endif
