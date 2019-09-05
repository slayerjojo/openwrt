#include "sigma_layer_link.h"
#include "sigma_sublayer_link_cloud.h"
#include "sigma_sublayer_link_lanwork.h"
#include "sigma_opcode.h"
#include "buddha_heap.h"
#include "interface_os.h"
#include "driver_linux.h"
#include "heap_mark.h"

static uint8_t _src[MAX_TERMINAL_ID] = {0};
static uint32_t _sequence = 0;
static uint32_t _session = 0;
static uint32_t _index = 0;
static uint16_t _reset = 0;

void sll_init(void)
{
    _reset = os_rand();

    ssll_init();
    sslc_init();
}

void sll_update(void)
{
    if (memcmp(_src, sll_terminal_local(), MAX_TERMINAL_ID))
        ssll_update();
    sslc_update();
}

const uint8_t *sll_terminal_bcast(void)
{
    return (const uint8_t *)"\xff\xff\xff\xff\xff\xff";
}

const uint8_t *sll_terminal_local(void)
{
    return (const uint8_t *)"\0\0\0\0\0\0";
}

const uint8_t *sll_src(uint8_t *src)
{
    if (src)
        memcpy(_src, src, MAX_TERMINAL_ID);
    return _src;
}

void *sll_send(uint8_t type, const uint8_t *dst, uint16_t size, uint8_t flags, uint8_t retry)
{
    uint32_t i = 0;
    uint8_t cs = 0;
    PacketTransmitSigmaLayerLink *packet = (PacketTransmitSigmaLayerLink *)buddha_heap_alloc(sizeof(PacketTransmitSigmaLayerLink) + size, HEAP_MARK_SLL_TRANSMIT);
    if (!packet)
        return 0;
    memset(packet, 0, sizeof(PacketTransmitSigmaLayerLink));
    packet->lanwork = !!(flags & SLL_FLAG_SEND_PATH_LANWORK);
    packet->cloud = !!(flags & SLL_FLAG_SEND_PATH_CLOUD);
    packet->retry = retry + 1;
    packet->time = os_ticks();
    packet->index = _index++;
    os_memcpy(packet->header.dst, dst, MAX_TERMINAL_ID);
    os_memcpy(packet->header.src, _src, MAX_TERMINAL_ID);
    packet->header.reset = network_htons(_reset);
    packet->header.sequence = network_htonl(_session++);
    packet->header.type = type;
    packet->header.opcode = OPCODE_LINK_SEND;
    packet->header.needAck = !!(flags & SLL_FLAG_SEND_ACK);
    packet->header.length = network_htons(size);
    for (i = 0; i < sizeof(PacketTransmitSigmaLayerLink); i++)
        cs += *(((uint8_t *)&(packet->header)) + i);
    return packet + 1;
}

void *sll_report(uint8_t type, uint16_t size, uint8_t flags)
{
    uint32_t i = 0;
    uint8_t cs = 0;
    PacketTransmitSigmaLayerLink *packet = (PacketTransmitSigmaLayerLink *)buddha_heap_alloc(sizeof(PacketTransmitSigmaLayerLink) + size, HEAP_MARK_SLL_TRANSMIT);
    if (!packet)
        return 0;
    memset(packet, 0, sizeof(PacketTransmitSigmaLayerLink));
    packet->lanwork = !!(flags & SLL_FLAG_SEND_PATH_LANWORK);
    packet->cloud = !!(flags & SLL_FLAG_SEND_PATH_CLOUD);
    packet->retry = 10;
    packet->time = os_ticks();
    packet->index = _index++;
    os_memcpy(packet->header.dst, sll_terminal_bcast(), MAX_TERMINAL_ID);
    os_memcpy(packet->header.src, _src, MAX_TERMINAL_ID);
    packet->header.reset = network_htons(_reset);
    packet->header.sequence = network_htonl(_sequence++);
    packet->header.type = type;
    packet->header.opcode = OPCODE_LINK_SEND;
    packet->header.needAck = 0;
    packet->header.length = network_htons(size);
    for (i = 0; i < sizeof(PacketTransmitSigmaLayerLink); i++)
        cs += *(((uint8_t *)&(packet->header)) + i);
    return packet + 1;
}

void sll_ack(const uint8_t *dst, uint16_t reset, uint32_t sequence)
{
    uint32_t i = 0;
    uint8_t cs = 0;
    PacketTransmitSigmaLayerLink *packet = (PacketTransmitSigmaLayerLink *)buddha_heap_alloc(sizeof(PacketTransmitSigmaLayerLink), HEAP_MARK_SLL_TRANSMIT);
    if (!packet)
        return;
    memset(packet, 0, sizeof(PacketTransmitSigmaLayerLink));
    packet->lanwork = 0;
    packet->cloud = 1;
    packet->retry = 1;
    packet->time = os_ticks();
    packet->index = _index++;
    os_memcpy(packet->header.dst, dst, MAX_TERMINAL_ID);
    os_memcpy(packet->header.src, _src, MAX_TERMINAL_ID);
    packet->header.reset = network_htons(reset);
    packet->header.sequence = network_htonl(sequence);
    packet->header.type = 0;
    packet->header.opcode = OPCODE_LINK_ACK;
    packet->header.needAck = 0;
    packet->header.length = network_htons(0);
    for (i = 0; i < sizeof(PacketTransmitSigmaLayerLink); i++)
        cs += *(((uint8_t *)&(packet->header)) + i);
}

void sll_retrans(const uint8_t *dst, uint16_t reset, uint32_t sequence)
{
    uint32_t i = 0;
    uint8_t cs = 0;
    PacketTransmitSigmaLayerLink *packet = (PacketTransmitSigmaLayerLink *)buddha_heap_alloc(sizeof(PacketTransmitSigmaLayerLink), HEAP_MARK_SLL_TRANSMIT);
    if (!packet)
        return;
    memset(packet, 0, sizeof(PacketTransmitSigmaLayerLink));
    packet->lanwork = 0;
    packet->cloud = 1;
    packet->retry = 1;
    packet->time = os_ticks();
    packet->index = _index++;
    os_memcpy(packet->header.dst, dst, MAX_TERMINAL_ID);
    os_memcpy(packet->header.src, _src, MAX_TERMINAL_ID);
    packet->header.reset = network_htons(reset);
    packet->header.sequence = network_htonl(sequence);
    packet->header.type = 0;
    packet->header.opcode = OPCODE_LINK_RETRANSMIT;
    packet->header.needAck = 0;
    packet->header.length = network_htons(0);
    for (i = 0; i < sizeof(PacketTransmitSigmaLayerLink); i++)
        cs += *(((uint8_t *)&(packet->header)) + i);
}

int sll_recv(uint8_t type, uint8_t **remote, void **buffer)
{
    int ret = 0;
    PacketReceiveSigmaLayerLink *packet = 0, *min = 0;
    while ((packet = (PacketReceiveSigmaLayerLink *)buddha_heap_find(packet, HEAP_MARK_SLL_RECEIVE)))
    {
        if (min->header.type != type)
            continue;
        if (!min)
            min = packet;
        if (!memcmp(min->header.src, packet->header.src, MAX_TERMINAL_ID) && 
            min->header.sequence > packet->header.sequence)
            min = packet;
    }
    if (min)
    {
        if (remote)
            *remote = min->header.src;
        if (buffer)
            *buffer = min + 1;
        ret = min->header.length;

        buddha_heap_mark_set(packet, HEAP_MARK_SLL_DEAL);
    }
    return ret;
}

