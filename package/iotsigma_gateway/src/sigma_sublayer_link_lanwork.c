#include "sigma_sublayer_link_lanwork.h"
#include "sigma_layer_link.h"
#include "sigma_event.h"
#include "sigma_opcode.h"
#include "buddha_heap.h"
#include "interface_network.h"
#include "driver_network_linux.h"
#include "interface_os.h"
#include "driver_linux.h"
#include "fsm.h"

#define HOST_LANWORK {255, 255, 255, 255}
#define PORT_LANWORK 7861

#define MAX_SSLL_RETRY (5)

enum 
{
    SSLL_STATE_INIT = 0,
    SSLL_STATE_SERVICE,
};

typedef struct
{
    uint8_t terminal[MAX_TERMINAL_ID];
    uint8_t ip[4];
    uint16_t port;
}SigmaLinkLanworkRemote;

typedef struct
{
    uint8_t retry;
    uint16_t reset;
    uint16_t lost;
    uint32_t sequence;
    uint8_t src[MAX_TERMINAL_ID];
}SigmaLinkLanworkLast;

static FSMState _state = {0};
static int _fp = -1;
static uint32_t _timer_transmit = 1;

static int ssll_receive(void)
{
    SigmaLinkLanworkRemote *remote = (SigmaLinkLanworkRemote *)buddha_heap_alloc(sizeof(SigmaLinkLanworkRemote), 0);
    if (!remote)
        return 0;
    PacketReceiveSigmaLayerLink *packet = (PacketReceiveSigmaLayerLink *)buddha_heap_alloc(sizeof(PacketReceiveSigmaLayerLink) + 256, 0);
    if (!packet)
    {
        buddha_heap_destory(remote);
        return 0;
    }
    int ret = network_udp_recv(_fp, &packet->header, sizeof(HeaderSigmaLayerLink) + 256, remote->ip, &remote->port);
    if (ret < 0)
    {
        buddha_heap_destory(remote);
        buddha_heap_destory(packet);
        network_udp_close(_fp);
        _fp = -1;
        fsm_update(&_state, SSLL_STATE_INIT);
        return ret;
    }
    if (ret < (int)sizeof(HeaderSigmaLayerLink) + network_ntohs(packet->header.length))
    {
        LogError("incomplete packet");
        buddha_heap_destory(remote);
        buddha_heap_destory(packet);
        return 0;
    }
    buddha_heap_realloc(packet, sizeof(PacketReceiveSigmaLayerLink) + network_ntohs(packet->header.length));
    
    size_t i = 0;
    uint8_t cs = 0;
    for (i = 0; i < sizeof(HeaderSigmaLayerLink); i++)
        cs += *((uint8_t *)(&packet->header) + i);
    if (cs)
    {
        LogDump(LOG_LEVEL_ERROR, "cs error raw:", &packet->header, sizeof(HeaderSigmaLayerLink));
        fsm_update(&_state, SSLL_STATE_INIT);
        buddha_heap_destory(packet);
        return 0;
    }
    
    packet->header.sequence = network_ntohl(packet->header.sequence);
    packet->header.reset = network_ntohs(packet->header.reset);
    packet->header.length = network_ntohs(packet->header.length);

    SigmaLinkLanworkRemote *r = 0;
    while ((r = (SigmaLinkLanworkRemote *)buddha_heap_find(r, HEAP_MARK_SSLL_REMOTE)) &&
        os_memcmp(r->terminal, packet->header.src, MAX_TERMINAL_ID));
    if (r)
        buddha_heap_destory(r);
    os_memcpy(remote->terminal, packet->header.src, MAX_TERMINAL_ID);
    buddha_heap_mark_set(remote, HEAP_MARK_SSLL_REMOTE);
    buddha_heap_free(remote);

    if (packet->header.opcode == OPCODE_LINK_ACK)
    {
        PacketTransmitSigmaLayerLink *ptsll = 0;
        while ((ptsll = (PacketTransmitSigmaLayerLink *)buddha_heap_find(ptsll, HEAP_MARK_SLL_TRANSMIT)) &&
            os_memcmp(ptsll->header.dst, packet->header.src, MAX_TERMINAL_ID) &&
            ptsll->header.reset != packet->header.reset &&
            ptsll->header.sequence != packet->header.sequence);
        if (ptsll)
            buddha_heap_destory(ptsll);

        buddha_heap_destory(packet);
    }
    else if (packet->header.opcode == OPCODE_LINK_RETRANSMIT)
    {
        PacketTransmitSigmaLayerLink *ptsll = 0;
        while ((ptsll = (PacketTransmitSigmaLayerLink *)buddha_heap_find(ptsll, HEAP_MARK_SLL_TRANSMIT)) &&
            os_memcmp(ptsll->header.dst, sll_bcast(), MAX_TERMINAL_ID) &&
            ptsll->header.reset != packet->header.reset &&
            ptsll->header.sequence != packet->header.sequence);
        if (ptsll)
        {
            ptsll->lanwork = 1;
            if (ptsll->retry < MAX_SSLL_RETRY)
                ptsll->retry = MAX_SSLL_RETRY;
        }

        buddha_heap_destory(packet);
    }
    else if (packet->header.opcode == OPCODE_LINK_SEND)
    {
        PacketReceiveSigmaLayerLink *deal = 0;
        while ((deal = (PacketReceiveSigmaLayerLink *)buddha_heap_find(deal, HEAP_MARK_SLL_DEAL)) && 
            os_memcmp(deal->header.src, packet->header.src, MAX_TERMINAL_ID) &&
            deal->header.reset != packet->header.reset && 
            deal->header.opcode != packet->header.opcode && 
            deal->header.type != packet->header.type &&
            deal->header.sequence != packet->header.sequence &&
            deal->header.sequence != packet->header.sequence);

        PacketReceiveSigmaLayerLink *receive = 0;
        while ((receive = (PacketReceiveSigmaLayerLink *)buddha_heap_find(deal, HEAP_MARK_SLL_RECEIVE)) && 
            os_memcmp(receive->header.src, packet->header.src, MAX_TERMINAL_ID) &&
            receive->header.reset != packet->header.reset && 
            receive->header.opcode != packet->header.opcode && 
            receive->header.type != packet->header.type &&
            receive->header.sequence != packet->header.sequence &&
            receive->header.sequence != packet->header.sequence);

        if (deal || receive)
        {
            buddha_heap_destory(packet);
        }
        else if (!os_memcmp(packet->header.dst, sll_src(0), MAX_TERMINAL_ID))
        {
            if (packet->header.needAck)
                sll_ack(packet->header.src, packet->header.reset, packet->header.sequence);

            buddha_heap_mark_set(packet, HEAP_MARK_SLL_RECEIVE);
        }
        else if (!os_memcmp(packet->header.dst, sll_bcast(), MAX_TERMINAL_ID))
        {
            SigmaLinkLanworkLast *last = 0;
            while ((last = (SigmaLinkLanworkLast *)buddha_heap_find(last, HEAP_MARK_SSLL_LAST)))
            {
                if (!os_memcmp(last->src, packet->header.src, MAX_TERMINAL_ID) && last->reset != packet->header.reset)
                    buddha_heap_destory(last);
            }

            last = 0;
            while ((last = (SigmaLinkLanworkLast *)buddha_heap_find(last, HEAP_MARK_SSLL_LAST)) && 
                last->reset != packet->header.reset &&
                os_memcmp(last->src, packet->header.src, MAX_TERMINAL_ID));
            if (last)
            {
                if (last->sequence < packet->header.sequence)
                {
                    size_t shift = packet->header.sequence - last->sequence;
                    if (shift >= sizeof(last->lost) * 8)
                        shift = sizeof(last->lost) * 8;
                    last->lost <<= shift;
                    last->lost |= 1;
                    last->sequence = packet->header.sequence;
                }
                else if (last->sequence > packet->header.sequence)
                {
                    size_t shift = last->sequence - packet->header.sequence;
                    if (shift >= sizeof(last->lost) * 8)
                        shift = sizeof(last->lost) * 8;
                    last->lost |= (0x01 << shift);
                }
            }
            else
            {
                last = (SigmaLinkLanworkLast *)buddha_heap_talloc(sizeof(SigmaLinkLanworkLast), HEAP_MARK_SSLL_LAST);
                if (last)
                {
                    last->reset = packet->header.reset;
                    last->sequence = packet->header.sequence;
                    last->lost = 0xffff;
                    os_memcpy(last->src, packet->header.src, MAX_TERMINAL_ID);
                }
            }
            buddha_heap_mark_set(packet, HEAP_MARK_SLL_RECEIVE);
        }
    }
    else
    {
        LogError("unsupport opcode:%u", packet->header.opcode);
    }
    return ret;
}

static int ssll_transmit(void)
{
    if (os_ticks_from(_timer_transmit) > os_ticks_ms(50 + _timer_transmit % 50))
    {
        _timer_transmit = os_ticks();

        PacketTransmitSigmaLayerLink *packet = 0, *min = 0;
        while ((packet = (PacketTransmitSigmaLayerLink *)buddha_heap_find(packet, HEAP_MARK_SLL_TRANSMIT)))
        {
            if (!packet->lanwork)
                continue;
            if (!min)
                min = packet;
            if (min->index > packet->index)
                min = packet;
        }
        if (min)
        {
            uint8_t ip[] = HOST_LANWORK;
            uint16_t port = PORT_LANWORK;
            if (os_memcmp(min->header.dst, sll_bcast(), MAX_TERMINAL_ID))
            {
                SigmaLinkLanworkRemote *remote = 0;
                while ((remote = (SigmaLinkLanworkRemote *)buddha_heap_find(remote, HEAP_MARK_SSLL_REMOTE)) && os_memcmp(remote->terminal, min->header.dst, MAX_TERMINAL_ID));
                if (remote)
                {
                    os_memcpy(ip, remote->ip, 4);
                    port = remote->port;
                }
            }
            int ret = network_udp_send(_fp, &min->header, sizeof(HeaderSigmaLayerLink) + network_ntohs(min->header.length), ip, port);
            if (ret < 0)
                return ret;
            min->lanwork = 0;
            if (!min->cloud && !min->lanwork)
                buddha_heap_free(min);
        }
        else
        {
            SigmaLinkLanworkLast *last = 0;
            while ((last = (SigmaLinkLanworkLast *)buddha_heap_find(last, HEAP_MARK_SSLL_LAST)))
            {
                size_t shift = 0;
                for (shift = 0; shift < sizeof(last->lost) * 8; shift++)
                {
                    if (!(last->lost & (1ul << shift)))
                        break;
                }
                if (shift >= sizeof(last->lost) * 8)
                    continue;
                sll_retrans(last->src, last->reset, last->sequence - shift);
                last->retry++;
                if (last->retry > MAX_SSLL_RETRY)
                {
                    last->lost |= 1 << shift;
                    last->retry = 0;
                }
                break;
            }
        }
    }
    return 0;
}

void ssll_init(void)
{
    fsm_update(&_state, SSLL_STATE_INIT);
    _fp = -1;
}

void ssll_update(void)
{
    if (SSLL_STATE_INIT == fsm_state(&_state))
    {
        if (_fp >= 0)
            network_udp_close(_fp);
        _fp = network_udp_create(PORT_LANWORK);
        if (_fp >= 0)
        {
            fsm_update(&_state, SSLL_STATE_SERVICE);
            sigma_event_dispatch(EVENT_SIGMA_LANWORK_STARTED, 0);
        }
    }
    if (SSLL_STATE_SERVICE == fsm_state(&_state))
    {
        if (ssll_receive() < 0 || ssll_transmit() < 0)
            fsm_update(&_state, SSLL_STATE_INIT);
    }
}
