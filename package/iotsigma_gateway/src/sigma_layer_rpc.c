#include "sigma_layer_rpc.h"
#include "sigma_layer_cluster.h"
#include "sigma_opcode.h"
#include "interface_os.h"
#include "interface_network.h"

typedef struct
{
    HeaderSigmaRPC header;
    int ret;
    uint8_t result[];
}__attribute__((packed)) PacketSigmaRPCResponser;

typedef struct _sigma_rpc_requester_item
{
    struct _sigma_rpc_requester_item *_next;

    uint16_t opcode;
    SigmaRPCRequester requester;
}SigmaRPCRequesterItem;

SigmaRPCRequesterItem *_requesters = 0;
SigmaRPCCall *_callers = 0;
static uint32_t _session = 1;

static void slr_local_call(const SigmaDomain *domain, uint16_t opcode, uint32_t session, uint8_t response, const void *parameter, uint16_t size)
{
    SigmaRPCRequesterItem * req = (SigmaRPCRequesterItem *)_requesters;
    while (req)
    {
        if (req->opcode == opcode)
            break;
        req = req->_next;
    }
    if (req)
        req->requester(domain, opcode, session, response, parameter, size);
}

static void slr_monitor(const uint8_t *cluster, const uint8_t *terminal, uint8_t event, const void *parameters, uint16_t size, void *ctx)
{
    UNUSED(ctx);

    if (event != EVENT_CLUSTER_PUBLISH)
        return;
    EventClusterPublish *e = (EventClusterPublish *)parameters;
    if (e->type != CLUSTER_TYPE_RPC)
        return;
    SigmaDomain domain;
    os_memcpy(domain.cluster, cluster, MAX_CLUSTER_ID);
    os_memcpy(domain.terminal, terminal, MAX_TERMINAL_ID);

    HeaderSigmaRPC *header = (HeaderSigmaRPC *)e->payload;
    if (OPCODE_RPC_RESPONSE == header->opcode)
    {
        PacketSigmaRPCResponser *packet = (PacketSigmaRPCResponser *)header;

        SigmaRPCCall *caller = _callers, *prev = 0;
        while (caller)
        {
            if (caller->session == header->session)
                break;
            prev = caller;
            caller = caller->next;
        }
        if (caller)
        {
            if (caller->responser)
                caller->responser(caller->ctx, &domain, packet->ret, packet->result, size);
            if (prev)
                prev->next = caller->next;
            else
                _callers = caller->next;
            os_free(caller);
        }
    }
    else
    {
        PacketSigmaRPCRequester *packet = (PacketSigmaRPCRequester *)header;

        slr_local_call(&domain, header->opcode, header->session, packet->response, packet->parameters, size - sizeof(EventClusterPublish) - sizeof(PacketSigmaRPCRequester));
    }
}

static int rpc_capability(const SigmaDomain *domain, uint16_t opcode, uint32_t session, uint8_t response, const void *parameter, uint16_t size)
{
    UNUSED(opcode);
    UNUSED(parameter);
    UNUSED(size);
    
    if (!response)
        return 0;

    size_t count = 0;
    SigmaRPCRequesterItem *r = _requesters;
    while (r)
    {
        count++;
        r = r->_next;
    }
    uint16_t *result = (uint16_t *)os_malloc(count * sizeof(uint16_t));
    if (!result)
    {
        LogError("out of memory");
        return -1;
    }
    count = 0;
    r = _requesters;
    while (r)
    {
        result[count++] = network_htons(r->opcode);
        r = r->_next;
    }
    slr_response(domain, RET_RPC_SUCCESS, session, result, count * sizeof(uint16_t));
    free(result);
    return 0;
}

void slr_init(void)
{
    slc_monitor(slr_monitor, 0);

    slr_register(OPCODE_RPC_CAPABILITY, rpc_capability);
}

void slr_update(void)
{
}

void slr_register(uint16_t opcode, SigmaRPCRequester req)
{
    SigmaRPCRequesterItem *item = (SigmaRPCRequesterItem *)os_malloc(sizeof(SigmaRPCRequesterItem));
    if (!item)
        return;
    item->opcode = opcode;
    item->requester = req;
    item->_next = _requesters;
    _requesters = item;
}

SigmaRPCCall *slr_create(const SigmaDomain *domain, uint16_t opcode, uint16_t parameter, uint16_t extend)
{
    SigmaRPCCall *caller = (SigmaRPCCall *)os_malloc(sizeof(SigmaRPCCall) + extend + parameter);
    if (!caller)
        return 0;
    os_memset(caller, 0, sizeof(SigmaRPCCall));
    os_memcpy(&caller->domain, domain, sizeof(SigmaDomain));
    caller->opcode = opcode;
    caller->session = _session++;
    caller->size = extend;
    caller->pos = 0;
    PacketSigmaRPCRequester *req = (PacketSigmaRPCRequester *)(caller->extend + caller->size);
    req->header.opcode = opcode;
    req->header.session = caller->session;
    return caller;
}

void slr_abort(SigmaRPCCall *caller)
{
    caller->abort = 1;
}

void *slr_extend(SigmaRPCCall *caller)
{
    return caller->extend;
}

void *slr_parameter(SigmaRPCCall *caller)
{
    return caller->extend + caller->size;
}

static void slr_push(void **parameter, const void *value, uint16_t size)
{
    os_memcpy(*parameter, value, size);
    *(uint8_t **)parameter += size;
}

void slr_push_byte(void **parameter, uint8_t value)
{
    slr_push(parameter, &value, 1);
}

void slr_push_short(void **parameter, uint16_t value)
{
    value = network_htons(value);
    slr_push(parameter, &value, 2);
}

void slr_push_int(void **parameter, uint32_t value)
{
    value = network_htonl(value);
    slr_push(parameter, &value, 4);
}

void slr_push_long(void **parameter, uint64_t value)
{
    value = network_htonll(value);
    slr_push(parameter, &value, 8);
}

void slr_push_float(void **parameter, double value)
{
    uint64_t v = network_htonll(*(uint64_t *)&value);
    slr_push(parameter, &v, 8);
}

void slr_push_buffer(void **parameter, const void *value, uint16_t size)
{
    slr_push_short(parameter, size);
    slr_push(parameter, value, size);
}

void slr_push_array(void **parameter, uint16_t count)
{
    slr_push_short(parameter, count);
}

uint8_t slr_pop_byte(const void **parameter)
{
    uint8_t v = **((uint8_t **)parameter);
    (*(uint8_t **)parameter)++;
    return v;
}

uint16_t slr_pop_short(const void **parameter)
{
    uint16_t v = network_ntohs(**(uint16_t **)parameter);
    (*(uint8_t **)parameter) += 2;
    return v;
}

uint32_t slr_pop_int(const void **parameter)
{
    uint32_t v = network_ntohl(**(uint32_t **)parameter);
    (*(uint8_t **)parameter) += 4;
    return v;
}

uint64_t slr_pop_long(const void **parameter)
{
    uint64_t v = network_ntohll(**(uint64_t **)parameter);
    (*(uint8_t **)parameter) += 8;
    return v;
}

double slr_pop_float(const void **parameter)
{
    uint64_t v = network_ntohll(**((uint64_t **)parameter));
    (*(uint8_t **)parameter) += 8;
    return *(double *)&v;
}

uint16_t slr_pop_buffer(const void **parameter, void **result)
{
    uint16_t size = slr_pop_short(parameter);
    *result = *(uint8_t **)parameter;
    (*(uint8_t **)parameter) += size;
    return size;
}

uint16_t slr_pop_array(const void **parameter)
{
    return slr_pop_short(parameter);
}

void slr_call(SigmaRPCCall *caller, SigmaRPCResponser responser, void *ctx, uint32_t timeout)
{
    caller->time = os_ticks();
    caller->timeout = timeout;
    caller->responser = responser;
    caller->ctx = ctx;

    SigmaDomain domain;
    os_memcpy(domain.cluster, caller->domain.cluster, MAX_CLUSTER_ID);
    os_memcpy(domain.terminal, sll_src(0), MAX_TERMINAL_ID);
    if (!os_memcmp(caller->domain.terminal, sll_terminal_local(), MAX_TERMINAL_ID) ||
        !os_memcmp(caller->domain.terminal, sll_src(0), MAX_TERMINAL_ID))
    {
        slr_local_call(&domain, caller->opcode, caller->session, !!responser, caller->extend + caller->size, caller->pos);
        os_free(caller);
        return;
    }
    else if (!os_memcmp(caller->domain.terminal, sll_terminal_bcast(), MAX_TERMINAL_ID))
    {
        slr_local_call(&domain, caller->opcode, caller->session, !!responser, caller->extend + caller->size, caller->pos);
    }

    caller->next = _callers;
    _callers = caller;

    PacketSigmaRPCRequester *req = (PacketSigmaRPCRequester *)(caller->extend + caller->size);
    req->header.opcode = network_ntohs(req->header.opcode);
    req->header.session = network_ntohl(req->header.session);
    req->response = !!responser;
    slc_publish(caller->domain.cluster, caller->domain.terminal, CLUSTER_TYPE_RPC, req, sizeof(PacketSigmaRPCRequester) + caller->pos);
    caller = (SigmaRPCCall *)os_realloc(caller, sizeof(SigmaRPCCall) + caller->size);
}

int slr_response(SigmaDomain *domain, int ret, uint32_t session, const void *result, uint16_t size)
{
    if (!os_memcmp(domain->terminal, sll_terminal_local(), MAX_TERMINAL_ID) ||
        !os_memcmp(domain->terminal, sll_src(0), MAX_TERMINAL_ID))
    {
        SigmaRPCCall *caller = _callers, *prev = 0;
        while (caller)
        {
            if (caller->session == session)
                break;
            prev = caller;
            caller = caller->next;
        }
        if (caller)
        {
            if (caller->responser)
                caller->responser(caller->ctx, domain, ret, result, size);
            if (prev)
                prev->next = caller->next;
            else
                _callers = caller->next;
            os_free(caller);
        }
        return 0;
    }
    PacketSigmaRPCResponser *packet = (PacketSigmaRPCResponser *)os_malloc(sizeof(PacketSigmaRPCResponser) + size);
    if (!packet)
    {
        LogError("out of memory.");
        return -1;
    }
    packet->header.opcode = network_htons(0);
    packet->header.session = network_htonl(session);
    packet->ret = network_htonl(ret);
    if (size)
        os_memcpy(packet->result, result, size);
    slc_publish(domain->cluster, domain->terminal, CLUSTER_TYPE_RPC, packet, sizeof(PacketSigmaRPCResponser) + size);
    os_free(packet);
    return 0;
}
