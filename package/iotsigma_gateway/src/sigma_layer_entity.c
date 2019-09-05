#include "sigma_layer_entity.h"
#include "sigma_layer_rpc.h"
#include "sigma_opcode.h"
#include "interface_os.h"
#include "interface_flash.h"
#include "entity.h"

#define TIMEOUT_ENTITY_RPC 5000

#define OPCODE_ENTITY_UPDATE 1
#define OPCODE_ENTITY_RELEASE 2
#define OPCODE_ENTITY_ACTION 3

typedef struct
{
    SLEValue *value;
    SigmaEntityListener resp;
    void *ctx;
}EntityListenerContext;

typedef struct _sigma_entity_listeners
{
    struct _sigma_entity_listeners *next;

    uint32_t session;
    SigmaEntityListener listener;
    void *ctx;
}SigmaEntityListeners;

typedef struct
{
    uint8_t opcode;
    uint32_t sequence;
    uint16_t entity;
}__attribute__((packed)) HeaderSigmaEntity;

typedef struct
{
    HeaderSigmaEntity header;
    uint8_t property;
    SLEValue value;
}__attribute__((packed)) PacketSigmaEntityUpdate;

typedef struct
{
    HeaderSigmaEntity header;
}__attribute__((packed)) PacketSigmaEntityRelease;

typedef struct
{
    HeaderSigmaEntity header;
    uint16_t action;
    uint8_t parameters[];
}__attribute__((packed)) PacketSigmaEntityAction;

static SigmaEntityListeners *_listeners = 0;
static uint32_t _sequence = 0;

//-------------------------------

typedef struct
{
    uint8_t type;
    uint8_t flags;
    uint16_t size;
}__attribute__((packed)) ParameterEntityCreate;

typedef struct
{
    uint16_t entity;
}__attribute__((packed)) ResultEntityCreate;

static int rpc_entity_create(const SigmaDomain *domain, uint16_t opcode, uint32_t session, const void *parameter, uint16_t size)
{
    UNUSED(opcode);
    UNUSED(size);

    ParameterEntityCreate *p = (ParameterEntityCreate *)parameter;
    p->size = network_ntohs(p->size);

    uint32_t addr = entity_create(p->type, p->size);
    if (!addr)
    {
        slr_response(domain, RET_RPC_ERROR_INSUFFICIENT_SPACE, session, 0, 0);
        return -1;
    }
    ResultEntityCreate result;
    property_get(addr, PROPERTY_ENTITY_ID, 0, &result.entity, sizeof(result.entity));
    result.entity = network_htons(result.entity);
    slr_response(domain, RET_RPC_SUCCESS, session, &result, sizeof(ResultEntityCreate));

    uint8_t buffer[sizeof(PacketSigmaEntityUpdate) + sizeof(uint16_t)];
    PacketSigmaEntityUpdate *packet = (PacketSigmaEntityUpdate *)buffer;
    packet->header.opcode = OPCODE_ENTITY_UPDATE;
    packet->header.entity = result.entity;
    packet->header.sequence = network_htonl(_sequence++);
    packet->property = PROPERTY_ENTITY_ID;
    packet->value.type = PROPERTY_TYPE_INT16;
    *(uint16_t *)packet->value.value = result.entity;
    slc_publish(domain->cluster, domain->terminal, CLUSTER_TYPE_ENTITY, buffer, sizeof(PacketSigmaEntityUpdate) + sizeof(uint16_t));

    return 0;
}

static int responser_create(void *ctx, const SigmaDomain *domain, int ret, const void *result, uint16_t size)
{
    UNUSED(size);

    EntityListenerContext *context = (EntityListenerContext *)ctx;

    if (ret != RET_RPC_SUCCESS)
    {
        LogError("ret:%d", ret);
        if (context->resp)
            ret = context->resp(context->ctx, EVENT_ENTITY_RESULT_FAILED, 0);
        return -1;
    }

    ResultEntityCreate *r = (ResultEntityCreate *)result;
    r->entity = network_ntohs(r->entity);

    uint8_t buffer[sizeof(SLEProperty) + sizeof(uint16_t)] = {0};
    SLEProperty *property = (SLEProperty *)buffer;
    property->entity.domain = domain;
    property->entity.entity = r->entity;
    property->property = PROPERTY_ENTITY_ID;
    property->value.type = PROPERTY_TYPE_INT16;
    *(uint16_t *)property->value.value = r->entity;

    if (context->value)
        os_memcpy(&context->value, &property->value, sizeof(SLEValue) + sizeof(uint16_t));
    
    if (context->resp)
        context->resp(context->ctx, EVENT_ENTITY_RESULT, property);

    return 0;
}

//-------------------------------

typedef struct
{
    uint16_t entity;
}__attribute__((packed)) ParameterEntityRelease;

static int rpc_entity_release(const SigmaDomain *domain, uint16_t opcode, uint32_t session, const void *parameter, uint16_t size)
{
    UNUSED(opcode);
    UNUSED(size);

    ParameterEntityRelease *p = (ParameterEntityRelease *)parameter;
    p->entity = network_ntohs(p->entity);

    uint32_t addr = entity_get(p->entity);
    if (!addr)
    {
        slr_response(domain, RET_RPC_ERROR_NOT_FOUND, session, 0, 0);
        return -1;
    }
    entity_release(addr);

    PacketSigmaEntityRelease packet;
    packet.header.opcode = OPCODE_ENTITY_RELEASE;
    packet.header.entity = network_htons(p->entity);
    packet.header.sequence = network_htonl(_sequence++);
    slc_publish(domain->cluster, domain->terminal, CLUSTER_TYPE_RPC, &packet, sizeof(PacketSigmaEntityRelease));

    slr_response(domain, RET_RPC_SUCCESS, session, 0, 0);
    return 0;
}

static int responser_release(void *ctx, const SigmaDomain *domain, int ret, const void *result, uint16_t size)
{
    UNUSED(domain);
    UNUSED(size);
    UNUSED(result);

    EntityListenerContext *context = (EntityListenerContext *)ctx;

    if (ret != RET_RPC_SUCCESS)
    {
        LogError("ret:%d", ret);
        if (context->resp)
            ret = context->resp(context->ctx, EVENT_ENTITY_RESULT_FAILED, 0);
        return -1;
    }

    if (context->resp)
        context->resp(context->ctx, EVENT_ENTITY_RESULT, 0);
    return 0;
}

//-------------------------------

typedef struct
{
    uint16_t entity;
    uint8_t property;
    uint8_t type;
    uint8_t value[];
}__attribute__((packed)) ParameterEntityWrite;

static int rpc_entity_write(const SigmaDomain *domain, uint16_t opcode, uint32_t session, const void *parameter, uint16_t size)
{
    UNUSED(opcode);

    ParameterEntityWrite *p = (ParameterEntityWrite *)parameter;
    p->entity = network_ntohs(p->entity);

    uint32_t addr = entity_get(p->entity);
    if (!addr)
    {
        slr_response(domain, RET_RPC_ERROR_NOT_FOUND, session, 0, 0);
        return -1;
    }
    property_set(&addr, p->property, p->type, p->value, size - sizeof(ParameterEntityWrite));

    slr_response(domain, RET_RPC_SUCCESS, session, 0, 0);
    
    uint8_t *buffer = (uint8_t *)os_malloc(sizeof(PacketSigmaEntityUpdate) + size - sizeof(ParameterEntityWrite));
    PacketSigmaEntityUpdate *packet = (PacketSigmaEntityUpdate *)buffer;
    packet->header.opcode = OPCODE_ENTITY_UPDATE;
    packet->header.entity = network_htons(p->entity);
    packet->header.sequence = network_htonl(_sequence++);
    packet->property = p->property;
    packet->value.type = p->type;
    os_memcpy(packet->value.value, p->value, size - sizeof(ParameterEntityWrite));
    slc_publish(domain->cluster, domain->terminal, CLUSTER_TYPE_ENTITY, buffer, sizeof(PacketSigmaEntityUpdate) + size + sizeof(ParameterEntityWrite));
    return 0;
}

static int responser_write(void *ctx, const SigmaDomain *domain, int ret, const void *result, uint16_t size)
{
    UNUSED(domain);
    UNUSED(size);
    UNUSED(result);

    EntityListenerContext *context = (EntityListenerContext *)ctx;

    if (ret != RET_RPC_SUCCESS)
    {
        LogError("ret:%d", ret);
        if (context->resp)
            ret = context->resp(context->ctx, EVENT_ENTITY_RESULT_FAILED, 0);
        return -1;
    }

    if (context->resp)
        context->resp(context->ctx, EVENT_ENTITY_RESULT, 0);
    return 0;
}

//-------------------------------

typedef struct
{
    uint16_t entity;
    uint8_t property;
}__attribute__((packed)) ParameterEntityRead;

typedef struct
{
    uint16_t entity;
    uint8_t property;
    uint8_t type;
    uint8_t value[];
}__attribute__((packed)) ResultEntityRead;

static int rpc_entity_read(const SigmaDomain *domain, uint16_t opcode, uint32_t session, const void *parameter, uint16_t size)
{
    UNUSED(opcode);
    UNUSED(size);

    ParameterEntityRead *p = (ParameterEntityRead *)parameter;
    p->entity = network_ntohs(p->entity);

    uint32_t addr = entity_get(p->entity);
    if (!addr)
    {
        slr_response(domain, RET_RPC_ERROR_NOT_FOUND, session, 0, 0);
        return -1;
    }
    uint8_t type = property_type(addr, p->property);
    uint8_t value[2];
    property_get(addr, p->property, 0, value, 2);
    uint16_t length = property_value_size(type, value);

    ResultEntityRead *result = (ResultEntityRead *)os_malloc(sizeof(ResultEntityRead) + length);
    result->entity = network_htons(p->entity);
    result->property = p->property;
    result->type = type;
    property_get(addr, p->property, 0, result->value, length);
    if (type == PROPERTY_TYPE_BUFFER || type == PROPERTY_TYPE_STRING)
        *(uint16_t *)result->value = network_htons(*(uint16_t *)result->value);
    slr_response(domain, RET_RPC_SUCCESS, session, result, sizeof(ResultEntityRead) + length);
    os_free(result);
    return 0;
}

static int responser_read(void *ctx, const SigmaDomain *domain, int ret, const void *result, uint16_t size)
{
    EntityListenerContext *context = (EntityListenerContext *)ctx;

    if (ret != RET_RPC_SUCCESS)
    {
        LogError("ret:%d", ret);
        if (context->resp)
            ret = context->resp(context->ctx, EVENT_ENTITY_RESULT_FAILED, 0);
        return -1;
    }
    
    ResultEntityRead *r = (ResultEntityRead *)result;
    r->entity = network_ntohs(r->entity);
    if (r->type == PROPERTY_TYPE_BUFFER || r->type == PROPERTY_TYPE_STRING)
        *(uint16_t *)r->value = network_ntohs(*(uint16_t *)r->value);

    SLEProperty *property = (SLEProperty *)os_malloc(sizeof(SLEProperty) + size - sizeof(ResultEntityRead));
    property->entity.domain = domain;
    property->entity.entity = r->entity;
    property->property = r->property;
    property->value.type = r->type;
    os_memcpy(property->value.value, r->value, size - sizeof(ResultEntityRead));

    if (context->value)
        os_memcpy(&context->value, &property->value, sizeof(SLEValue) + size - sizeof(ResultEntityRead));
    
    if (context->resp)
        context->resp(context->ctx, EVENT_ENTITY_RESULT, property);
    os_free(property);
    return 0;
}

//-------------------------

typedef struct
{
    uint8_t type;
    uint16_t entity;
    uint32_t addr;
}__attribute__((packed)) ParameterEntityIterator;

typedef struct
{
    uint8_t type;
    uint16_t entity;
    uint32_t addr;
}__attribute__((packed)) ResultEntityIterator;

static int rpc_entity_iterator(const SigmaDomain *domain, uint16_t opcode, uint32_t session, const void *parameter, uint16_t size)
{
    UNUSED(opcode);
    UNUSED(size);

    ParameterEntityIterator *p = (ParameterEntityIterator *)parameter;
    p->entity = network_ntohs(p->entity);
    p->addr = network_ntohs(p->addr);

    if (p->addr && p->entity && property_compare(p->addr, PROPERTY_ENTITY_ID, 0, &p->entity, 2))
    {
        p->addr = 0;
        p->entity = 0;
    }

    if (p->type == 0xff)
        p->addr = entity_iterator(p->addr);
    else
        p->addr = entity_find(p->type, p->addr);

    ResultEntityIterator result;
    property_get(p->addr, PROPERTY_ENTITY_ID, 0, &result.entity, 2);
    result.entity = network_htons(result.entity);
    result.addr = network_htonl(p->addr);
    result.type = p->type;
    slr_response(domain, RET_RPC_SUCCESS, session, &result, sizeof(ResultEntityIterator));
    return 0;
}

static int responser_iterator(void *ctx, const SigmaDomain *domain, int ret, const void *result, uint16_t size)
{
    UNUSED(size);

    EntityListenerContext *context = (EntityListenerContext *)ctx;

    if (ret != RET_RPC_SUCCESS)
    {
        LogError("ret:%d", ret);
        if (context->resp)
            ret = context->resp(context->ctx, EVENT_ENTITY_RESULT_FAILED, 0);
        return -1;
    }
    
    ResultEntityIterator *r = (ResultEntityIterator *)result;
    r->addr = network_ntohl(r->addr);
    r->entity = network_ntohs(r->entity);

    uint8_t buffer[sizeof(SLEProperty) + sizeof(uint16_t)] = {0};
    SLEProperty *property = (SLEProperty *)buffer;
    property->entity.domain = domain;
    property->entity.entity = r->entity;
    property->property = PROPERTY_ENTITY_ID;
    property->value.type = PROPERTY_TYPE_INT16;
    *(uint16_t *)property->value.value = r->entity;

    ret = 0;
    if (context->resp)
        ret = context->resp(context->ctx, EVENT_ENTITY_RESULT_ITERATOR, property);
    if (ret || !r->addr)
        return 0;

    SigmaRPCCall *caller = slr_create(domain, OPCODE_RPC_ENTITY_ITERATOR, sizeof(ParameterEntityIterator), sizeof(EntityListenerContext));
    if (!caller)
        return 0;
    ParameterEntityIterator *p = (ParameterEntityIterator *)slr_parameter(caller);
    p->addr = network_htonl(r->addr);
    p->entity = network_htons(r->entity);
    p->type = r->type;

    EntityListenerContext *extend = (EntityListenerContext *)slr_extend(caller);
    extend->value = 0;
    extend->resp = context->resp;
    extend->ctx = context->ctx;
    slr_call(caller, responser_iterator, extend, TIMEOUT_ENTITY_RPC);
    return 0;
}

//--------------------------

typedef struct
{
    uint8_t action;
    uint8_t parameter[];
}__attribute__((packed)) ParameterEntityAction;

static int rpc_entity_action(const SigmaDomain *domain, uint16_t opcode, uint32_t session, const void *parameter, uint16_t size)
{
    ParameterEntityAction *p = (ParameterEntityAction *)parameter;
}

static int responser_action(void *ctx, const SigmaDomain *domain, int ret, const void *result, uint16_t size)
{
}
//----------------------------

static void sle_monitor(const uint8_t *cluster, const uint8_t *terminal, uint8_t event, const void *parameters, uint16_t size, void *ctx)
{
    if (event != EVENT_CLUSTER_PUBLISH)
        return;
    EventClusterPublish *e = (EventClusterPublish *)parameters;
    if (e->type != CLUSTER_TYPE_ENTITY)
        return;
    HeaderSigmaEntity *header = (HeaderSigmaEntity *)e->payload;
    if (OPCODE_ENTITY_UPDATE == header->opcode)
    {
    }
    else if (OPCODE_ENTITY_RELEASE == header->opcode)
    {
    }
    else if (OPCODE_ENTITY_ACTION == header->opcode)
    {
    }
}

void sle_init(void)
{
    _listeners = 0;

    slc_monitor(sle_monitor, 0);

    slr_register(OPCODE_RPC_ENTITY_CREATE, rpc_entity_create);
    slr_register(OPCODE_RPC_ENTITY_RELEASE, rpc_entity_release);
    slr_register(OPCODE_RPC_ENTITY_WRITE, rpc_entity_write);
    slr_register(OPCODE_RPC_ENTITY_READ, rpc_entity_read);
    slr_register(OPCODE_RPC_ENTITY_ITERATOR, rpc_entity_iterator);
}

void sle_update(void)
{
}

int sle_create(const SigmaDomain *domain, uint8_t type, uint16_t size, uint8_t flags, SLEValue *value, SigmaEntityListener resp, void *ctx)
{
    SigmaRPCCall *caller = slr_create(domain, OPCODE_RPC_ENTITY_CREATE, sizeof(ParameterEntityCreate), sizeof(EntityListenerContext));
    if (!caller)
        return -1;
    ParameterEntityCreate *p = (ParameterEntityCreate *)slr_parameter(caller);
    p->type = type;
    p->size = network_htons(size);
    p->flags = flags;
    EntityListenerContext *extend = (EntityListenerContext *)slr_extend(caller);
    extend->value = value;
    extend->resp = resp;
    extend->ctx = ctx;
    slr_call(caller, responser_create, extend, TIMEOUT_ENTITY_RPC);
    return 0;
}

int sle_release(const SLEEntity *entity, SigmaEntityListener resp, void *ctx)
{
    SigmaRPCCall *caller = slr_create(entity->domain, OPCODE_RPC_ENTITY_RELEASE, sizeof(ParameterEntityRelease), sizeof(EntityListenerContext));
    if (!caller)
        return -1;
    ParameterEntityRelease *p = (ParameterEntityRelease *)slr_parameter(caller);
    p->entity = network_htons(entity->entity);
    EntityListenerContext *extend = (EntityListenerContext *)slr_extend(caller);
    extend->value = 0;
    extend->resp = resp;
    extend->ctx = ctx;
    slr_call(caller, responser_release, extend, TIMEOUT_ENTITY_RPC);
    return 0;
}

int sle_write(const SLEEntity *entity, uint8_t property, const SLEValue *value, SigmaEntityListener resp, void *ctx)
{
    SigmaRPCCall *caller = slr_create(entity->domain, OPCODE_RPC_ENTITY_WRITE, sizeof(ParameterEntityWrite), sizeof(EntityListenerContext));
    if (!caller)
        return -1;
    ParameterEntityWrite *p = (ParameterEntityWrite *)slr_parameter(caller);
    p->entity = network_htons(entity->entity);
    p->property = property;
    p->type = value->type;

    EntityListenerContext *extend = (EntityListenerContext *)slr_extend(caller);
    extend->value = 0;
    extend->resp = resp;
    extend->ctx = ctx;
    slr_call(caller, responser_write, extend, TIMEOUT_ENTITY_RPC);
    return 0;
}

int sle_read(const SLEEntity *entity, uint8_t property, SLEValue *value, SigmaEntityListener resp, void *ctx)
{
    SigmaRPCCall *caller = slr_create(entity->domain, OPCODE_RPC_ENTITY_READ, sizeof(ParameterEntityRead), sizeof(EntityListenerContext));
    if (!caller)
        return -1;
    ParameterEntityRead *p = (ParameterEntityRead *)slr_parameter(caller);
    p->entity = network_htons(entity->entity);
    p->property = property;

    EntityListenerContext *extend = (EntityListenerContext *)slr_extend(caller);
    extend->value = value;
    extend->resp = resp;
    extend->ctx = ctx;
    slr_call(caller, responser_read, extend, TIMEOUT_ENTITY_RPC);
    return 0;
}

int sle_action(const SLEEntity *entity, uint8_t action, uint8_t *parameter, uint16_t size, SigmaEntityListener resp, void *ctx)
{
    SigmaRPCCall *caller = slr_create(entity->domain, OPCODE_RPC_ENTITY_ACTION, sizeof(ParameterEntityAction) + size, sizeof(EntityListenerContext));
    if (!caller)
        return -1;
    ParameterEntityAction *p = (ParameterEntityAction *)slr_parameter(caller);
    p->action = action;
    os_memcpy(p->parameter, parameter, size);

    EntityListenerContext *extend = (EntityListenerContext *)slr_extend(caller);
    extend->value = 0;
    extend->resp = resp;
    extend->ctx = ctx;
    slr_call(caller, responser_action, extend, TIMEOUT_ENTITY_RPC);
    return 0;
}

void sle_iterator(const SigmaDomain *domain, uint8_t type, SigmaEntityListener resp, void *ctx)
{
    SigmaRPCCall *caller = slr_create(domain, OPCODE_RPC_ENTITY_ITERATOR, sizeof(ParameterEntityIterator), sizeof(EntityListenerContext));
    if (!caller)
        return;
    ParameterEntityIterator *p = (ParameterEntityIterator *)slr_parameter(caller);
    p->addr = 0;
    p->entity = 0;
    p->type = type;

    EntityListenerContext *extend = (EntityListenerContext *)slr_extend(caller);
    extend->value = 0;
    extend->resp = resp;
    extend->ctx = ctx;
    slr_call(caller, responser_iterator, extend, TIMEOUT_ENTITY_RPC);
}

void sle_listen(SigmaEntityListener listener, void *ctx)
{
    SigmaEntityListeners *listeners = (SigmaEntityListeners *)os_malloc(sizeof(SigmaEntityListeners));
    if (!listeners)
        return;
    listeners->listener = listener;
    listeners->ctx = ctx;
    
    listeners->next = _listeners;
    _listeners = listeners;
}

void sle_unlisten(SigmaEntityListener listener, void *ctx)
{
    SigmaEntityListeners *listeners = _listeners, *prev = 0;
    while (listeners)
    {
        if (listeners->listener == listener && listeners->ctx == ctx)
            break;
        prev = prev->next;
        listeners = listeners->next;
    }
    if (!listeners)
        return;
    if (prev)
        prev->next = listeners->next;
    else
        _listeners = listeners->next;
    os_free(listeners);
}
