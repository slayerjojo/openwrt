#ifndef __SIGMA_LAYER_ENTITY_H__
#define __SIGMA_LAYER_ENTITY_H__

#include "env.h"
#include "sigma_layer_rpc.h"
#include "interface_os.h"
#include "entity.h"

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

typedef struct
{
    uint8_t type;
    uint8_t value;
}__attribute__((packed)) SLEValueBit;

#define SLE_VALUE_DEF_BIT(var, def)   \
    SLEValue8 var = {0};                \
    var.type = PROPERTY_TYPE_BIT;       \
    var.value = (def);

typedef struct
{
    uint8_t type;
    uint8_t value;
}__attribute__((packed)) SLEValue8;

#define SLE_VALUE_DEF_8(var, def) \
    SLEValue8 var = {0};            \
    var.type = PROPERTY_TYPE_INT8;  \
    var.value = (def);

typedef struct
{
    uint8_t type;
    uint16_t value;
}__attribute__((packed)) SLEValue16;

#define SLE_VALUE_DEF_16(var, def) \
    SLEValue16 var;                  \
    var.type = PROPERTY_TYPE_INT16;  \
    var.value = (def);

typedef struct
{
    uint8_t type;
    uint32_t value;
}__attribute__((packed)) SLEValue32;

#define SLE_VALUE_DEF_32(var, def) \
    SLEValue32 var;                  \
    var.type = PROPERTY_TYPE_INT32;  \
    var.value = (def);

typedef struct
{
    uint8_t type;
    uint8_t value[6];
}__attribute__((packed)) SLEValue48;

#define SLE_VALUE_DEF_48(var, def) \
    SLEValue48 var;                  \
    var.type = PROPERTY_TYPE_INT48;  \
    os_memcpy(var.value, (def), 6);

typedef struct
{
    uint8_t type;
    uint8_t value[8];
}__attribute__((packed)) SLEValue64;

#define SLE_VALUE_DEF_64(var, def) \
    SLEValue64 var;                  \
    var.type = PROPERTY_TYPE_INT64;  \
    os_memcpy(var.value, (def), 8);

typedef struct
{
    uint8_t type;
    uint16_t size;
    uint8_t value[];
}__attribute__((packed)) SLEValueBuffer;

#define SLE_VALUE_DEF_BUF(var, def, def_size)                                               \
    SLEValueBuffer *var = (SLEValueBuffer *)os_malloc(sizeof(SLEValueBuffer) + (def_size)); \
    if (var)                                                                                \
    {                                                                                       \
        var->type = PROPERTY_TYPE_BUFFER;                                                   \
        var->size = (def_size);                                                             \
        if ((def) && def_size)                                                              \
            os_memcpy(var->value, (def), def_size);                                         \
    }

#define SLE_VALUE_DEF_BUF_STATIC(var, def, def_size)                \
    uint8_t buffer_##var[sizeof(SLEValueBuffer) + (def_size)] = {0};\
    SLEValueBuffer *var = (SLEValueBuffer *)buffer_##var;           \
    var->type = PROPERTY_TYPE_BUFFER;                               \
    var->size = (def_size);                                         \
    if ((def) && (def_size))                                        \
        os_memcpy(var->value, (def) ? (def) : buffer_##var, def_size);         \

typedef struct
{
    uint8_t type;
    uint16_t size;
    char value[];
}__attribute__((packed)) SLEValueString;

#define SLE_VALUE_DEF_STR(var, def)                                                             \
    SLEValueBuffer *var = (SLEValueBuffer *)os_malloc(sizeof(SLEValueBuffer) + strlen(def) + 1);\
    if (var)                                                                                    \
    {                                                                                           \
        var->type = PROPERTY_TYPE_STRING;                                                       \
        var->size = strlen(def) + 1;                                                            \
        if ((var) && var->size)                                                                 \
            os_memcpy(var->value, (def), var->size);                                            \
    }

typedef int (*SigmaEntityListener)(void *ctx, int event, const SLEEntity *entity, uint8_t property, const SLEValue *value);

void sle_init(void);
void sle_update(void);

int sle_create(const SigmaDomain *domain, uint8_t type, uint16_t size, SLEEntity *entity, SigmaEntityListener resp, void *ctx);
int sle_release(const SLEEntity *entity, SigmaEntityListener resp, void *ctx);

int sle_write(const SLEEntity *entity, uint8_t property, const SLEValue *value, SigmaEntityListener resp, void *ctx);
int sle_read(const SLEEntity *entity, uint8_t property, SLEValue *value, SigmaEntityListener resp, void *ctx);

int sle_action(const SLEEntity *entity, uint8_t action, const SLEValue *parameter, SigmaEntityListener resp, void *ctx);

uint32_t sle_iterator_create(const SigmaDomain *domain, uint8_t type, uint8_t manual);
void sle_iterator_release(uint32_t session);
int sle_iterator_filter(uint32_t session, uint8_t filter, uint8_t property, const SLEValue *value);
int sle_iterator_execute(uint32_t session, uint32_t *last, SLEEntity *result, SigmaEntityListener resp, void *ctx);

void sle_listen(SigmaEntityListener listener, void *ctx);
void sle_unlisten(SigmaEntityListener listener, void *ctx);

#endif
