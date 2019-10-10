#include "sigma_layer_entity.h"
#include "sigma_layer_rpc.h"
#include "sigma_opcode.h"
#include "sigma_mission.h"
#include "interface_os.h"
#include "interface_flash.h"
#include "entity.h"

#define TIMEOUT_ENTITY_RPC 5000

#define OPCODE_ENTITY_UPDATE 1
#define OPCODE_ENTITY_CREATE 2
#define OPCODE_ENTITY_RELEASE 3

typedef struct
{
    union
    {
        SLEValue *value;
        SLEEntity *entity;
    }imm;
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
}__attribute__((packed)) PacketSigmaEntityCreate;

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

static SigmaEntityListeners *_listeners = 0;
static uint32_t _sequence = 0;

//-------------------------------

typedef struct
{
    uint8_t type;
    uint16_t size;
}__attribute__((packed)) ParameterEntityCreate;

typedef struct
{
    uint16_t entity;
}__attribute__((packed)) ResultEntityCreate;

static int rpc_entity_create(const SigmaDomain *domain, uint16_t opcode, uint32_t session, uint8_t response, const void *parameter, uint16_t size)
{
    UNUSED(opcode);
    UNUSED(size);

    ParameterEntityCreate *p = (ParameterEntityCreate *)parameter;
    p->size = network_ntohs(p->size);

    uint8_t space[max(MAX_CLUSTER_ID + MAX_TERMINAL_ID, sizeof(PacketSigmaEntityCreate) + sizeof(uint8_t))] = {0};
    os_memcpy(space, domain->cluster, MAX_CLUSTER_ID);
    flash_space(space);

    uint32_t addr = entity_create(p->type, p->size);
    if (!addr)
    {
        if (response)
            slr_response(domain, RET_RPC_ERROR_INSUFFICIENT_SPACE, session, 0, 0);
        return -1;
    }
    ResultEntityCreate result;
    property_get(addr, PROPERTY_ENTITY_ID, 0, &result.entity, sizeof(result.entity));
    property_pack(PROPERTY_TYPE_INT16, &result.entity);
    if (response)
        slr_response(domain, RET_RPC_SUCCESS, session, &result, sizeof(ResultEntityCreate));

    PacketSigmaEntityCreate *packet = (PacketSigmaEntityCreate *)space;
    packet->header.opcode = OPCODE_ENTITY_CREATE;
    packet->header.entity = result.entity;
    packet->header.sequence = network_htonl(_sequence++);
    packet->property = PROPERTY_ENTITY_TYPE;
    packet->value.type = PROPERTY_TYPE_INT8;
    property_get(addr, PROPERTY_ENTITY_TYPE, 0, packet->value.value, 1);
    slc_publish(domain->cluster, domain->terminal, CLUSTER_TYPE_ENTITY, space, sizeof(PacketSigmaEntityUpdate) + sizeof(uint8_t));

    return 0;
}

//-------------------------------

typedef struct
{
    uint16_t entity;
}__attribute__((packed)) ParameterEntityRelease;

static int rpc_entity_release(const SigmaDomain *domain, uint16_t opcode, uint32_t session, uint8_t response, const void *parameter, uint16_t size)
{
    UNUSED(opcode);
    UNUSED(size);

    uint8_t space[max(sizeof(ParameterEntityRelease), MAX_CLUSTER_ID + MAX_TERMINAL_ID)] = {0};
    os_memcpy(space, domain->cluster, MAX_CLUSTER_ID);
    flash_space(space);

    ParameterEntityRelease *p = (ParameterEntityRelease *)parameter;
    p->entity = network_ntohs(p->entity);

    uint32_t addr = entity_get(p->entity);
    if (!addr)
    {
        if (response)
            slr_response(domain, RET_RPC_ERROR_NOT_FOUND, session, 0, 0);
        return -1;
    }
    entity_release(addr);

    PacketSigmaEntityRelease *packet = (PacketSigmaEntityRelease *)space;
    packet->header.opcode = OPCODE_ENTITY_RELEASE;
    packet->header.entity = network_htons(p->entity);
    packet->header.sequence = network_htonl(_sequence++);
    slc_publish(domain->cluster, domain->terminal, CLUSTER_TYPE_RPC, packet, sizeof(PacketSigmaEntityRelease));

    if (response)
        slr_response(domain, RET_RPC_SUCCESS, session, 0, 0);
    return 0;
}

//-------------------------------

typedef struct
{
    uint16_t entity;
    uint8_t property;
    SLEValue value;
}__attribute__((packed)) ParameterEntityWrite;

static int rpc_entity_write(const SigmaDomain *domain, uint16_t opcode, uint32_t session, uint8_t response, const void *parameter, uint16_t size)
{
    UNUSED(opcode);

    ParameterEntityWrite *p = (ParameterEntityWrite *)parameter;

    uint8_t *space = (uint8_t *)os_malloc(max(sizeof(PacketSigmaEntityUpdate) + size - sizeof(ParameterEntityWrite), MAX_CLUSTER_ID + MAX_TERMINAL_ID));
    if (!space)
    {
        if (response)
            slr_response(domain, RET_RPC_ERROR_INSUFFICIENT_SPACE, session, 0, 0);
        return -1;
    }
    os_memcpy(space, domain->cluster, MAX_CLUSTER_ID);
    os_memset(space + MAX_CLUSTER_ID, 0, MAX_TERMINAL_ID);
    flash_space(space);

    uint32_t addr = entity_get(p->entity);
    if (!addr)
    {
        os_free(space);

        if (response)
            slr_response(domain, RET_RPC_ERROR_NOT_FOUND, session, 0, 0);
        return -1;
    }
    
    PacketSigmaEntityUpdate *packet = (PacketSigmaEntityUpdate *)space;
    packet->header.opcode = OPCODE_ENTITY_UPDATE;
    packet->header.entity = p->entity;
    packet->header.sequence = network_htonl(_sequence++);
    packet->property = p->property;
    packet->value.type = p->value.type;
    os_memcpy(packet->value.value, p->value.value, size - sizeof(ParameterEntityWrite));
    slc_publish(domain->cluster, domain->terminal, CLUSTER_TYPE_ENTITY, space, sizeof(PacketSigmaEntityUpdate) + size - sizeof(ParameterEntityWrite));

    p->entity = network_ntohs(p->entity);
    property_unpack(p->value.type, p->value.value);
    property_set(&addr, p->property, p->value.type, p->value.value);

    if (response)
        slr_response(domain, RET_RPC_SUCCESS, session, 0, 0);
    
    os_free(space);
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
    SLEValue value;
}__attribute__((packed)) ResultEntityRead;

static int rpc_entity_read(const SigmaDomain *domain, uint16_t opcode, uint32_t session, uint8_t response, const void *parameter, uint16_t size)
{
    UNUSED(opcode);
    UNUSED(size);

    if (!response)
        return 0;

    uint8_t space[MAX_CLUSTER_ID + MAX_TERMINAL_ID] = {0}; 
    os_memcpy(space, domain->cluster, MAX_CLUSTER_ID);
    flash_space(space);

    ParameterEntityRead *p = (ParameterEntityRead *)parameter;
    p->entity = network_ntohs(p->entity);

    uint32_t addr = entity_get(p->entity);
    if (!addr)
    {
        slr_response(domain, RET_RPC_ERROR_NOT_FOUND, session, 0, 0);
        return -1;
    }
    uint8_t type = property_type(addr, p->property);
    uint16_t length = property_size(addr, p->property);

    ResultEntityRead *result = (ResultEntityRead *)os_malloc(sizeof(ResultEntityRead) + length);
    if (!result)
    {
        slr_response(domain, RET_RPC_ERROR_INSUFFICIENT_SPACE, session, 0, 0);
        return -1;
    }
    result->entity = network_htons(p->entity);
    result->property = p->property;
    result->value.type = type;
    property_get(addr, p->property, 0, result->value.value, length);
    property_pack(result->value.type, result->value.value);
    slr_response(domain, RET_RPC_SUCCESS, session, result, sizeof(ResultEntityRead) + length);
    os_free(result);
    return 0;
}

//-------------------------

typedef struct _sigma_entity_iterator
{
    struct _sigma_entity_iterator *next;

    SigmaDomain domain;

    uint8_t manual;

    uint8_t type;
    uint32_t session;

    uint32_t *last;
    SLEEntity *result;

    SigmaEntityListener resp;
    void *ctx;

    uint32_t timer;
}SigmaEntityIterator;

typedef struct _sigma_entity_iterator_filter
{
    struct _sigma_entity_iterator_filter *next;

    uint8_t filter;
    uint8_t property;
    SLEValue value;
}SigmaEntityIteratorFilter;

typedef struct _sigma_entity_iterator_session
{
    struct _sigma_entity_iterator_session *next;

    SigmaDomain domain;
    uint32_t session;

    SigmaEntityIteratorFilter *filters;

    uint32_t timer;
}SigmaEntityIteratorSession;

typedef struct
{
    uint8_t type;
    uint32_t session;
}__attribute__((packed)) ParameterEntityIteratorCreate;

typedef struct
{
    uint32_t session;
}__attribute__((packed)) ParameterEntityIteratorRelease;

typedef struct
{
    uint32_t session;
    uint8_t filter;
    uint8_t property;
    SLEValue value;
}__attribute__((packed)) ParameterEntityIteratorFilter;

typedef struct
{
    uint32_t session;
    uint32_t last;
}__attribute__((packed)) ParameterEntityIteratorExecute;

typedef struct
{
    uint32_t addr;
    uint16_t entity;
    uint8_t property;
    SLEValue value;
}__attribute__((packed)) ResultEntityIterator;

static uint32_t _session_iterator = 1;

static SigmaEntityIterator *_iterators = 0;
static SigmaEntityIteratorSession *_sessions = 0;

static int rpc_entity_iterator_create(const SigmaDomain *domain, uint16_t opcode, uint32_t session, uint8_t response, const void *parameter, uint16_t size)
{
    UNUSED(opcode);
    UNUSED(size);

    ParameterEntityIteratorCreate *p = (ParameterEntityIteratorCreate *)parameter;
    p->session = network_ntohl(p->session);

    SigmaEntityIteratorSession *is = _sessions;
    while (is)
    {
        if (!os_memcmp(&is->domain, domain, sizeof(SigmaDomain)) && is->session == p->session)
            break;
        is = is->next;
    }
    if (!is)
    {
        is = (SigmaEntityIteratorSession *)os_malloc(sizeof(SigmaEntityIteratorSession));
        if (!is)
        {
            if (response)
                slr_response(domain, RET_RPC_ERROR_INSUFFICIENT_SPACE, session, 0, 0);
            return 0;
        }
        os_memcpy(&is->domain, domain, sizeof(SigmaDomain));
        is->session = p->session;
        is->filters = 0;
        is->timer = os_ticks();
    }

    if (response)
        slr_response(domain, RET_RPC_SUCCESS, session, 0, 0);
    return 0;
}

static int rpc_entity_iterator_release(const SigmaDomain *domain, uint16_t opcode, uint32_t session, uint8_t response, const void *parameter, uint16_t size)
{
    UNUSED(opcode);
    UNUSED(size);

    ParameterEntityIteratorRelease *p = (ParameterEntityIteratorRelease *)parameter;
    p->session = network_ntohl(p->session);

    SigmaEntityIteratorSession *is = _sessions, *prev = 0;
    while (is)
    {
        if (!os_memcmp(&is->domain, domain, sizeof(SigmaDomain)) && is->session == p->session)
            break;
        prev = is;
        is = is->next;
    }
    if (is)
    {
        if (prev)
            prev->next = is->next;
        else
            _sessions = is->next;
        while (is->filters)
        {
            SigmaEntityIteratorFilter *filter = is->filters;
            is->filters = is->filters->next;

            os_free(filter);
        }
        os_free(is);
    }

    if (response)
        slr_response(domain, RET_RPC_SUCCESS, session, 0, 0);
    return 0;
}

static int rpc_entity_iterator_filter(const SigmaDomain *domain, uint16_t opcode, uint32_t session, uint8_t response, const void *parameter, uint16_t size)
{
    UNUSED(opcode);
    UNUSED(size);

    ParameterEntityIteratorFilter *p = (ParameterEntityIteratorFilter *)parameter;
    p->session = network_ntohl(p->session);
    property_unpack(p->value.type, p->value.value);

    SigmaEntityIteratorSession *is = _sessions, *prev = 0;
    while (is)
    {
        if (!os_memcmp(&is->domain, domain, sizeof(SigmaDomain)) && is->session == p->session)
            break;
        prev = is;
        is = is->next;
    }
    if (!is)
    {
        if (response)
            slr_response(domain, RET_RPC_ERROR_NOT_FOUND, session, 0, 0);
        return 0;
    }

    SigmaEntityIteratorFilter *filter = (SigmaEntityIteratorFilter *)os_malloc(sizeof(SigmaEntityIteratorFilter) + property_length(p->value.type, p->value.value));
    if (!filter)
    {
        if (response)
            slr_response(domain, RET_RPC_ERROR_INSUFFICIENT_SPACE, session, 0, 0);

        if (prev)
            prev->next = is->next;
        else
            _sessions = is->next;

        while (is->filters)
        {
            SigmaEntityIteratorFilter *filter = is->filters;
            is->filters = is->filters->next;

            os_free(filter);
        }

        os_free(is);
        return 0;
    }
    filter->next = is->filters;
    is->filters = filter;
    
    filter->filter = p->filter;
    filter->property = p->property;
    filter->value.type = p->value.type;
    os_memcpy(filter->value.value, p->value.value, property_length(p->value.type, p->value.value));

    if (response)
        slr_response(domain, RET_RPC_SUCCESS, session, 0, 0);
    return 0;
}

static int rpc_entity_iterator_execute(const SigmaDomain *domain, uint16_t opcode, uint32_t session, uint8_t response, const void *parameter, uint16_t size)
{
    UNUSED(opcode);
    UNUSED(size);

    ParameterEntityIteratorExecute *p = (ParameterEntityIteratorExecute *)parameter;
    p->session = network_ntohl(p->session);
    p->last = network_ntohl(p->last);

    SigmaEntityIteratorSession *is = _sessions, *prev = 0;
    while (is)
    {
        if (!os_memcmp(&is->domain, domain, sizeof(SigmaDomain)) && is->session == p->session)
            break;
        prev = is;
        is = is->next;
    }
    if (!is)
    {
        if (response)
            slr_response(domain, RET_RPC_ERROR_NOT_FOUND, session, 0, 0);
        return 0;
    }

    uint32_t addr = p->last;
    while ((addr = entity_iterator(addr)))
    {
        SigmaEntityIteratorFilter *filter = is->filters;
        while (filter)
        {
            if (filter->filter == SLE_FILTER_EQUAL)
            {
                if (property_compare(addr, filter->property, 0, filter->value.value, property_length(filter->value.type, filter->value.value)))
                    break;
            }
            else if (filter->filter == SLE_FILTER_NOTEQ)
            {
                if (!property_compare(addr, filter->property, 0, filter->value.value, property_length(filter->value.type, filter->value.value)))
                    break;
            }
            filter = filter->next;
        }
        if (!filter)
            break;
    }

    ResultEntityIterator *result = (ResultEntityIterator *)os_malloc(sizeof(ResultEntityIterator) + sizeof(uint8_t));
    if (!result)
    {
        if (response)
            slr_response(domain, RET_RPC_ERROR_INSUFFICIENT_SPACE, session, 0, 0);

        if (prev)
            prev->next = is->next;
        else
            _sessions = is->next;

        while (is->filters)
        {
            SigmaEntityIteratorFilter *filter = is->filters;
            is->filters = is->filters->next;

            os_free(filter);
        }
        os_free(is);
        return 0;
    }
    result->addr = network_htonl(addr);
    if (addr)
    {
        property_get(addr, PROPERTY_ENTITY_ID, 0, &result->entity, sizeof(uint16_t));
        result->entity = network_htons(result->entity);
        result->property = PROPERTY_ENTITY_TYPE;
        result->value.type = PROPERTY_TYPE_INT8;
        property_get(addr, PROPERTY_ENTITY_TYPE, 0, &result->value.value, sizeof(uint8_t));
    }
    if (response)
        slr_response(domain, RET_RPC_SUCCESS, session, result, sizeof(ResultEntityIterator) + sizeof(uint8_t));
    return 0;
}

//--------------------------

typedef struct
{
    uint16_t entity;
    uint8_t action;
    SLEValue parameter;
}__attribute__((packed)) ParameterEntityAction;

static int rpc_entity_action(const SigmaDomain *domain, uint16_t opcode, uint32_t session, uint8_t response, const void *parameter, uint16_t size)
{
    UNUSED(opcode);

    ParameterEntityAction *p = (ParameterEntityAction *)parameter;
    p->entity = network_ntohs(p->entity);

    SLEEntity entity;
    entity.domain = domain;
    entity.entity = p->entity;

    SigmaEntityListeners *listener = _listeners;
    while (listener)
    {
        listener->listener(listener->ctx, EVENT_ENTITY_ACTION, &entity, p->action, &p->parameter);
        listener = listener->next;
    }
    if (response)
        slr_response(domain, RET_RPC_SUCCESS, session, 0, 0);

    return 0;
}

//----------------------------

static void sle_monitor(const uint8_t *cluster, const uint8_t *terminal, uint8_t event, const void *parameters, uint16_t size, void *ctx)
{
    UNUSED(ctx);

    if (event != EVENT_CLUSTER_PUBLISH)
        return;
    EventClusterPublish *e = (EventClusterPublish *)parameters;
    if (e->type != CLUSTER_TYPE_ENTITY)
        return;
    SigmaDomain domain;
    os_memcpy(domain.cluster, cluster, MAX_CLUSTER_ID);
    os_memcpy(domain.terminal, terminal, MAX_TERMINAL_ID);

    SLEEntity entity;
    entity.domain = &domain;

    HeaderSigmaEntity *header = (HeaderSigmaEntity *)e->payload;
    if (OPCODE_ENTITY_CREATE == header->opcode)
    {
        PacketSigmaEntityCreate *packet = (PacketSigmaEntityCreate *)header;

        entity.entity = network_htons(packet->header.entity);

        SLEValue *value = (SLEValue *)os_malloc(sizeof(SLEValue) + size - sizeof(EventClusterPublish) - sizeof(PacketSigmaEntityCreate));
        if (value)
        {
            value->type = packet->value.type;
            os_memcpy(value->value, packet->value.value, size - sizeof(EventClusterPublish) - sizeof(PacketSigmaEntityCreate));
            if (value->type == PROPERTY_TYPE_BUFFER || value->type == PROPERTY_TYPE_STRING)
                *(uint16_t *)value->value = network_htons(*(uint16_t *)value->value);

            SigmaEntityListeners *listener = _listeners;
            while (listener)
            {
                listener->listener(listener->ctx, EVENT_ENTITY_CREATE, &entity, packet->property, value);
                listener = listener->next;
            }

            os_free(value);
        }
    }
    else if (OPCODE_ENTITY_UPDATE == header->opcode)
    {
        PacketSigmaEntityUpdate *packet = (PacketSigmaEntityUpdate *)header;

        entity.entity = network_htons(packet->header.entity);

        SLEValue *value = (SLEValue *)os_malloc(sizeof(SLEValue) + size - sizeof(EventClusterPublish) - sizeof(PacketSigmaEntityUpdate));
        if (value)
        {
            value->type = packet->value.type;
            os_memcpy(value->value, packet->value.value, size - sizeof(EventClusterPublish) - sizeof(PacketSigmaEntityUpdate));
            if (value->type == PROPERTY_TYPE_BUFFER || value->type == PROPERTY_TYPE_STRING)
                *(uint16_t *)value->value = network_htons(*(uint16_t *)value->value);

            SigmaEntityListeners *listener = _listeners;
            while (listener)
            {
                listener->listener(listener->ctx, EVENT_ENTITY_UPDATE, &entity, packet->property, value);
                listener = listener->next;
            }

            os_free(value);
        }
    }
    else if (OPCODE_ENTITY_RELEASE == header->opcode)
    {
        PacketSigmaEntityRelease *packet = (PacketSigmaEntityRelease *)header;
        
        entity.entity = network_htons(packet->header.entity);

        SigmaEntityListeners *listener = _listeners;
        while (listener)
        {
            listener->listener(listener->ctx, EVENT_ENTITY_RELEASE, &entity, PROPERTY_ENTITY_ID, 0);
            listener = listener->next;
        }
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
    slr_register(OPCODE_RPC_ENTITY_ITERATOR_CREATE, rpc_entity_iterator_create);
    slr_register(OPCODE_RPC_ENTITY_ITERATOR_RELEASE, rpc_entity_iterator_release);
    slr_register(OPCODE_RPC_ENTITY_ITERATOR_FILTER, rpc_entity_iterator_filter);
    slr_register(OPCODE_RPC_ENTITY_ITERATOR_EXECUTE, rpc_entity_iterator_execute);
}

void sle_update(void)
{
}

static int responser_create(void *ctx, const SigmaDomain *domain, int ret, const void *result, uint16_t size)
{
    UNUSED(size);

    EntityListenerContext *context = (EntityListenerContext *)ctx;

    if (ret != RET_RPC_SUCCESS)
    {
        LogError("ret:%d", ret);
        if (context->resp)
            ret = context->resp(context->ctx, EVENT_ENTITY_RESULT_FAILED, 0, 0, 0);
        return -1;
    }

    ResultEntityCreate *r = (ResultEntityCreate *)result;
    r->entity = network_ntohs(r->entity);

    SLEEntity entity;
    entity.domain = domain;
    entity.entity = r->entity;

    if (context->imm.entity)
    {
        context->imm.entity->domain = domain;
        context->imm.entity->entity = r->entity;
    }
    
    if (context->resp)
    {
        SLE_VALUE_DEF_16(value, r->entity);
        context->resp(context->ctx, EVENT_ENTITY_RESULT, &entity, PROPERTY_ENTITY_ID, (SLEValue *)&value);
    }

    return 0;
}

int sle_create(const SigmaDomain *domain, uint8_t type, uint16_t size, SLEEntity *entity, SigmaEntityListener resp, void *ctx)
{
    SigmaRPCCall *caller = slr_create(domain, OPCODE_RPC_ENTITY_CREATE, sizeof(ParameterEntityCreate), (entity || resp) ? sizeof(EntityListenerContext) : 0);
    if (!caller)
        return -1;

    ParameterEntityCreate *p = (ParameterEntityCreate *)slr_parameter(caller);
    p->type = type;
    p->size = network_htons(size);

    EntityListenerContext *extend = 0;
    if (entity || resp)
    {
        extend = (EntityListenerContext *)slr_extend(caller);
        extend->imm.entity = entity;
        extend->resp = resp;
        extend->ctx = ctx;
    }

    slr_call(caller, extend ? responser_create : 0, extend, TIMEOUT_ENTITY_RPC);
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
            ret = context->resp(context->ctx, EVENT_ENTITY_RESULT_FAILED, 0, 0, 0);
        return -1;
    }

    if (context->resp)
        context->resp(context->ctx, EVENT_ENTITY_RESULT, 0, 0, 0);
    return 0;
}

int sle_release(const SLEEntity *entity, SigmaEntityListener resp, void *ctx)
{
    SigmaRPCCall *caller = slr_create(entity->domain, OPCODE_RPC_ENTITY_RELEASE, sizeof(ParameterEntityRelease), resp ? sizeof(EntityListenerContext) : 0);
    if (!caller)
        return -1;

    ParameterEntityRelease *p = (ParameterEntityRelease *)slr_parameter(caller);
    p->entity = network_htons(entity->entity);

    EntityListenerContext *extend = 0;
    if (resp)
    {
        extend = (EntityListenerContext *)slr_extend(caller);
        extend->imm.value = 0;
        extend->resp = resp;
        extend->ctx = ctx;
    }
    slr_call(caller, resp ? responser_release : 0, extend, TIMEOUT_ENTITY_RPC);
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
            ret = context->resp(context->ctx, EVENT_ENTITY_RESULT_FAILED, 0, 0, 0);
        return -1;
    }

    if (context->resp)
        context->resp(context->ctx, EVENT_ENTITY_RESULT, 0, 0, 0);
    return 0;
}

int sle_write(const SLEEntity *entity, uint8_t property, const SLEValue *value, SigmaEntityListener resp, void *ctx)
{
    SigmaRPCCall *caller = slr_create(entity->domain, OPCODE_RPC_ENTITY_WRITE, sizeof(ParameterEntityWrite) + property_length(value->type, value->value), resp ? sizeof(EntityListenerContext) : 0);
    if (!caller)
        return -1;
    ParameterEntityWrite *p = (ParameterEntityWrite *)slr_parameter(caller);
    p->entity = network_htons(entity->entity);
    p->property = property;
    p->value.type = value->type;
    os_memcpy(p->value.value, value->value, property_length(value->type, value->value));

    EntityListenerContext *extend = 0;
    if (resp)
    {
        extend = (EntityListenerContext *)slr_extend(caller);
        extend->imm.value = 0;
        extend->resp = resp;
        extend->ctx = ctx;
    }
    slr_call(caller, resp ? responser_write : 0, extend, TIMEOUT_ENTITY_RPC);
    return 0;
}

static int responser_read(void *ctx, const SigmaDomain *domain, int ret, const void *result, uint16_t size)
{
    EntityListenerContext *context = (EntityListenerContext *)ctx;

    if (ret != RET_RPC_SUCCESS)
    {
        LogError("ret:%d", ret);
        if (context->resp)
            ret = context->resp(context->ctx, EVENT_ENTITY_RESULT_FAILED, 0, 0, 0);
        return -1;
    }
    
    ResultEntityRead *r = (ResultEntityRead *)result;
    r->entity = network_ntohs(r->entity);
    property_unpack(r->value.type, r->value.value);

    SLEEntity entity;
    entity.domain = domain;
    entity.entity = r->entity;

    if (context->imm.value)
    {
        context->imm.value->type = r->value.type;
        os_memcpy(context->imm.value->value, r->value.value, size - sizeof(ResultEntityRead));
    }
    
    if (context->resp)
        context->resp(context->ctx, EVENT_ENTITY_RESULT, &entity, r->property, &r->value);
    return 0;
}

int sle_read(const SLEEntity *entity, uint8_t property, SLEValue *value, SigmaEntityListener resp, void *ctx)
{
    SigmaRPCCall *caller = slr_create(entity->domain, OPCODE_RPC_ENTITY_READ, sizeof(ParameterEntityRead), (resp || value) ? sizeof(EntityListenerContext) : 0);
    if (!caller)
        return -1;
    ParameterEntityRead *p = (ParameterEntityRead *)slr_parameter(caller);
    p->entity = network_htons(entity->entity);
    p->property = property;

    EntityListenerContext *extend = 0;
    if (value || resp)
    {
        extend = (EntityListenerContext *)slr_extend(caller);
        extend->imm.value = value;
        extend->resp = resp;
        extend->ctx = ctx;
    }
    slr_call(caller, (resp || value) ? responser_read : 0, extend, TIMEOUT_ENTITY_RPC);
    return 0;
}

static int responser_action(void *ctx, const SigmaDomain *domain, int ret, const void *result, uint16_t size)
{
    UNUSED(domain);
    UNUSED(result);
    UNUSED(size);

    EntityListenerContext *context = (EntityListenerContext *)ctx;

    if (ret != RET_RPC_SUCCESS)
    {
        LogError("ret:%d", ret);
        if (context->resp)
            ret = context->resp(context->ctx, EVENT_ENTITY_RESULT_FAILED, 0, 0, 0);
        return -1;
    }
    if (context->resp)
        context->resp(context->ctx, EVENT_ENTITY_RESULT, 0, 0, 0);
    return 0;
}

int sle_action(const SLEEntity *entity, uint8_t action, const SLEValue *parameter, SigmaEntityListener resp, void *ctx)
{
    SigmaRPCCall *caller = slr_create(entity->domain, OPCODE_RPC_ENTITY_ACTION, sizeof(ParameterEntityAction) + property_length(parameter->type, parameter->value), resp ? sizeof(EntityListenerContext) : 0);
    if (!caller)
        return -1;
    ParameterEntityAction *p = (ParameterEntityAction *)slr_parameter(caller);
    p->action = action;
    p->entity = network_htons(entity->entity);
    p->parameter.type = parameter->type;
    os_memcpy(p->parameter.value, parameter->value, property_length(parameter->type, parameter->value));

    EntityListenerContext *extend = 0;
    if (resp)
    {
        extend = (EntityListenerContext *)slr_extend(caller);
        extend->imm.value = 0;
        extend->resp = resp;
        extend->ctx = ctx;
    }
    slr_call(caller, resp ? responser_action : 0, extend, TIMEOUT_ENTITY_RPC);
    return 0;
}

static int responser_iterator(void *ctx, const SigmaDomain *domain, int ret, const void *result, uint16_t size)
{
    UNUSED(domain);
    UNUSED(result);
    UNUSED(size);

    SigmaEntityIterator *it = _iterators, *prev = 0;
    while (it && it != ctx)
    {
        prev = it;
        it = it->next;
    }
    if (!it)
        return 0;
    
    if (ret != RET_RPC_SUCCESS)
    {
        LogError("ret:%d", ret);
        if (it->resp)
            ret = it->resp(it->ctx, EVENT_ENTITY_RESULT_FAILED, 0, 0, 0);
        
        if (prev)
            prev->next = it->next;
        else
            _iterators = it->next;
        os_free(it);

        return -1;
    }

    return 0;
}

uint32_t sle_iterator_create(const SigmaDomain *domain, uint8_t type, uint8_t manual)
{
    SigmaEntityIterator *it = (SigmaEntityIterator *)os_malloc(sizeof(SigmaEntityIterator));
    if (!it)
        return 0;
    os_memset(it, 0, sizeof(SigmaEntityIterator));

    it->next = _iterators;
    _iterators = it;

    if (domain)
        os_memcpy(&it->domain, domain, sizeof(SigmaDomain));
    else
        os_memset(&it->domain, 0, sizeof(SigmaDomain));

    it->manual = manual;

    it->type = type;
    it->session = _session_iterator++;

    it->timer = os_ticks();

    SigmaRPCCall *caller = slr_create(domain, OPCODE_RPC_ENTITY_ITERATOR_CREATE, sizeof(ParameterEntityIteratorCreate), 0);
    if (!caller)
        return 0;
    ParameterEntityIteratorCreate *p = (ParameterEntityIteratorCreate *)slr_parameter(caller);
    p->type = type;
    p->session = network_htonl(it->session);

    slr_call(caller, responser_iterator, it, TIMEOUT_ENTITY_RPC);
    return it->session;
}

void sle_iterator_release(uint32_t session)
{
    SigmaEntityIterator *it = _iterators, *prev = 0;
    while (it && it->session != session)
    {
        prev = it;
        it = it->next;
    }
    if (!it)
        return;

    SigmaRPCCall *caller = slr_create(&it->domain, OPCODE_RPC_ENTITY_ITERATOR_RELEASE, sizeof(ParameterEntityIteratorRelease), 0);
    if (!caller)
        return;
    ParameterEntityIteratorRelease *p = (ParameterEntityIteratorRelease *)slr_parameter(caller);
    p->session = network_htonl(it->session);

    slr_call(caller, 0, 0, TIMEOUT_ENTITY_RPC);

    if (prev)
        prev->next = it->next;
    else
        _iterators = it->next;
    os_free(it);
}

int sle_iterator_filter(uint32_t session, uint8_t filter, uint8_t property, const SLEValue *value)
{
    SigmaEntityIterator *it = _iterators;
    while (it && it->session != session)
        it = it->next;
    if (!it)
        return -1;

    SigmaRPCCall *caller = slr_create(&it->domain, OPCODE_RPC_ENTITY_ITERATOR_FILTER, sizeof(ParameterEntityIteratorFilter) + property_length(value->type, value->value), 0);
    if (!caller)
        return -1;

    ParameterEntityIteratorFilter *p = (ParameterEntityIteratorFilter *)slr_parameter(caller);
    p->session = network_htonl(session);
    p->property = property;
    p->filter = filter;
    p->value.type = value->type;
    os_memcpy(p->value.value, value->value, property_length(value->type, value->value));
    
    slr_call(caller, responser_iterator, it, TIMEOUT_ENTITY_RPC);
    return 0;
}

static int responser_iterator_execute(void *ctx, const SigmaDomain *domain, int ret, const void *result, uint16_t size)
{
    UNUSED(domain);
    UNUSED(size);

    SigmaEntityIterator *it = _iterators, *prev = 0;
    while (it && it != ctx)
    {
        prev = it;
        it = it->next;
    }
    if (!it)
        return 0;
    
    if (ret != RET_RPC_SUCCESS)
    {
        LogError("ret:%d", ret);
        if (it->resp)
            ret = it->resp(it->ctx, EVENT_ENTITY_RESULT_FAILED, 0, 0, 0);
        
        if (prev)
            prev->next = it->next;
        else
            _iterators = it->next;
        os_free(it);

        return -1;
    }

    ResultEntityIterator *r = (ResultEntityIterator *)result;
    r->addr = network_ntohl(r->addr);

    if (0 == r->addr)
    {
        if (it->last)
            *(it->last) = r->addr;

        if (it->resp)
            it->resp(it->ctx, EVENT_ENTITY_FINISHED, 0, 0, 0);

        if (prev)
            prev->next = it->next;
        else
            _iterators = it->next;
        os_free(it);
    }
    else
    {
        r->entity = network_ntohs(r->entity);
        property_unpack(r->value.type, r->value.value);

        SLEEntity entity;
        entity.domain = domain;
        entity.entity = r->entity;

        if (it->last)
            *(it->last) = r->addr;

        if (it->result)
        {
            if (it->result)
            {
                it->result->domain = domain;
                it->result->entity = entity.entity;
            }
        }

        int ret = 0;
        if (it->resp)
            ret = it->resp(it->ctx, EVENT_ENTITY_RESULT, &entity, r->property, &r->value);
        if (ret)
        {
            sle_iterator_release(it->session);
            return 1;
        }
        else if (!it->manual)
        {
            sle_iterator_execute(it->session, it->last, it->result, it->resp, it->ctx);
        }
    }

    return 0;
}

int sle_iterator_execute(uint32_t session, uint32_t *last, SLEEntity *result, SigmaEntityListener resp, void *ctx)
{
    SigmaEntityIterator *it = _iterators;
    while (it && it->session != session)
        it = it->next;
    if (!it)
        return -1;

    it->last = last;
    it->result = result;
    it->resp = resp;
    it->ctx = ctx;

    SigmaRPCCall *caller = slr_create(&it->domain, OPCODE_RPC_ENTITY_ITERATOR_EXECUTE, sizeof(ParameterEntityIteratorExecute), 0);
    if (!caller)
        return -1;

    ParameterEntityIteratorExecute *p = (ParameterEntityIteratorExecute *)slr_parameter(caller);
    p->session = network_htonl(session);
    p->last = network_htonl(*last);
    
    slr_call(caller, responser_iterator_execute, it, TIMEOUT_ENTITY_RPC);
    return 0;
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
