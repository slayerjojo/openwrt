#ifndef __SIGMA_LAYER_ENTITY_H__
#define __SIGMA_LAYER_ENTITY_H__

#include "env.h"
#include "sigma_layer_rpc.h"

#define EVENT_ENTITY_RESULT 1
#define EVENT_ENTITY_RESULT_FAILED 2
#define EVENT_ENTITY_RESULT_ITERATOR 3
#define EVENT_ENTITY_RESULT_END 4

#define SLE_FLAG_SYNC 1

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

typedef struct
{
    SLEEntity entity;
    uint8_t property;
    SLEValue value;
}SLEProperty;

typedef int (*SigmaEntityListener)(void *ctx, int event, const SLEProperty *property);

void sle_init(void);
void sle_update(void);

int sle_create(const SigmaDomain *domain, uint8_t type, uint16_t size, uint8_t flags, SLEValue *value, SigmaEntityListener resp, void *ctx);
int sle_release(const SLEEntity *entity, SigmaEntityListener resp, void *ctx);

int sle_write(const SLEEntity *entity, uint8_t property, const SLEValue *value, SigmaEntityListener resp, void *ctx);
int sle_read(const SLEEntity *entity, uint8_t property, SLEValue *value, SigmaEntityListener resp, void *ctx);

int sle_action(const SLEEntity *entity, uint8_t action, uint8_t *parameter, uint16_t size);

void sle_iterator(const SigmaDomain *domain, uint8_t type, SigmaEntityListener resp, void *ctx);

void sle_listen(SigmaEntityListener listener, void *ctx);
void sle_unlisten(SigmaEntityListener listener, void *ctx);

#endif
