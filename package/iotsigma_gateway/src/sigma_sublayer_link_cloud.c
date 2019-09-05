#include "sigma_sublayer_link_cloud.h"
#include "sigma_layer_link.h"
#include "sigma_event.h"
#include "sigma_opcode.h"
#include "interface_crypto.h"
#include "interface_network.h"
#include "interface_os.h"
#include "driver_linux.h"
#include "driver_network_linux.h"
#include "buddha_heap.h"
#include "heap_mark.h"
#include "fsm.h"

#define HOST_CLOUD "control.iotsigma.com"
#define PORT_CLOUD 9000

typedef struct
{
    HeaderSigmaSublayerLink header;
    uint8_t raw[16];
    uint8_t auth[16];
}__attribute__((packed)) PacketSigmaSublayerLinkCryptoReq;

typedef struct
{
    HeaderSigmaSublayerLink header;
    uint8_t raw[16];
    uint8_t ver[32];
}__attribute__((packed)) PacketSigmaSublayerLinkCryptoResp;

typedef struct
{
    HeaderSigmaSublayerLink header;
    uint8_t ver[32];
}__attribute__((packed)) PacketSigmaSublayerLinkCryptoVerify;

typedef struct
{
    HeaderSigmaSublayerLink header;
    uint8_t auth[16];
}__attribute__((packed)) PacketSigmaSublayerLinkRegisterReq;

typedef struct
{
    HeaderSigmaSublayerLink header;
    uint8_t id[MAX_TERMINAL_ID];
}__attribute__((packed)) PacketSigmaSublayerLinkRegisterResp;

enum
{
    SSLC_STATE_CLOUD_INIT = 0,
    SSLC_STATE_CLOUD_RECONNECT, 
    SSLC_STATE_CLOUD_CRYPTO_REQ,
    SSLC_STATE_CLOUD_CRYPTO_RESP,
    SSLC_STATE_CLOUD_REGISTER,
    SSLC_STATE_CLOUD_REGISTER_WAIT,
    SSLC_STATE_CLOUD_AUTH,
    SSLC_STATE_CLOUD_SERVICE,
};

static FSMState _state = {0};
static int _fp = -1;
static uint32_t _pos_transmit = 0;
static PacketTransmitSigmaLayerLink *_transmit = 0;
static uint32_t _pos_receive = 0;
static PacketReceiveSigmaLayerLink *_receive = 0;

static int sslc_transmit(void)
{
    if (!_transmit)
    {
        PacketTransmitSigmaLayerLink *packet = 0, *min = 0;
        while ((packet = (PacketTransmitSigmaLayerLink *)buddha_heap_find(packet, HEAP_MARK_SLL_TRANSMIT)))
        {
            if (!packet->cloud)
                continue;
            if (!min)
                min = packet;
            if (min->index > packet->index)
                min = packet;
        }
        if (min)
        {
            _transmit = min;
            _pos_transmit = 0;
        }
    }
    if (!_transmit)
        return 0;
    int ret = network_tcp_send(_fp, (uint8_t *)(&_transmit->header) + _pos_transmit, sizeof(HeaderSigmaLayerLink) + network_ntohs(_transmit->header.length) - _pos_transmit);
    if (ret < 0)
    {
        _transmit = 0;
        _pos_transmit = 0;
        return ret;
    }
    _pos_transmit += ret;
    if (_pos_transmit >= sizeof(HeaderSigmaLayerLink) + network_ntohs(_transmit->header.length))
    {
        _transmit->cloud = 0;
        if (!_transmit->cloud && !_transmit->lanwork)
            buddha_heap_free(_transmit);
        _transmit = 0;
        _pos_transmit = 0;
    }
    return ret;
}

static int sslc_receive(void)
{
    if (!_receive)
    {
        _receive = (PacketReceiveSigmaLayerLink *)buddha_heap_alloc(sizeof(PacketReceiveSigmaLayerLink), 0);
        if (!_receive)
            return 0;
        _pos_receive = 0;
    }
    if (_pos_receive < sizeof(HeaderSigmaLayerLink))
    {
        int ret = network_tcp_recv(_fp, (uint8_t *)(&_receive->header) + _pos_receive, sizeof(HeaderSigmaLayerLink) - _pos_receive);
        if (ret < 0)
        {
            buddha_heap_free(_receive);
            _receive = 0;
            return -1;
        }
        _pos_receive += ret;
        if (_pos_receive >= sizeof(HeaderSigmaLayerLink))
        {
            uint8_t *p = (uint8_t *)buddha_heap_realloc(_receive, sizeof(PacketReceiveSigmaLayerLink) + network_ntohs(_receive->header.length));
            if (!p)
            {
                buddha_heap_free(_receive);
                _receive = 0;
                return -1;
            }
            _receive = (PacketReceiveSigmaLayerLink *)p;
        }
    }
    if (_pos_receive < sizeof(HeaderSigmaLayerLink) + network_ntohs(_receive->header.length))
    {
        int ret = network_tcp_recv(_fp, (uint8_t *)(&_receive->header) + _pos_receive, sizeof(HeaderSigmaLayerLink) - _pos_receive);
        if (ret < 0)
        {
            buddha_heap_free(_receive);
            _receive = 0;
            return -1;
        }
        _pos_receive += ret;
        if (_pos_receive >= sizeof(HeaderSigmaLayerLink) + network_ntohs(_receive->header.length))
        {
            size_t i = 0;
            uint8_t cs = 0;
            for (i = 0; i < sizeof(HeaderSigmaLayerLink); i++)
                cs += *((uint8_t *)(&_receive->header) + i);
            if (cs)
            {
                LogDump(LOG_LEVEL_ERROR, "cs error raw:", &_receive->header, sizeof(HeaderSigmaLayerLink));
                buddha_heap_free(_receive);
                _receive = 0;
                return -1;
            }

            _receive->header.sequence = network_ntohl(_receive->header.sequence);
            _receive->header.reset = network_ntohs(_receive->header.reset);
            _receive->header.length = network_ntohs(_receive->header.length);

            PacketReceiveSigmaLayerLink *deal = 0;
            while ((deal = (PacketReceiveSigmaLayerLink *)buddha_heap_find(deal, HEAP_MARK_SLL_DEAL)) && 
                os_memcmp(deal->header.src, _receive->header.src, MAX_TERMINAL_ID) &&
                deal->header.reset != _receive->header.reset && 
                deal->header.opcode != _receive->header.opcode && 
                deal->header.type != _receive->header.type &&
                deal->header.sequence != _receive->header.sequence);

            PacketReceiveSigmaLayerLink *receive = 0;
            while ((receive = (PacketReceiveSigmaLayerLink *)buddha_heap_find(receive, HEAP_MARK_SLL_RECEIVE)) && 
                os_memcmp(receive->header.src, _receive->header.src, MAX_TERMINAL_ID) &&
                receive->header.reset != _receive->header.reset && 
                receive->header.opcode != _receive->header.opcode && 
                receive->header.type != _receive->header.type &&
                receive->header.sequence != _receive->header.sequence);
            if (deal || receive)
                buddha_heap_destory(_receive);
            else if (_receive->header.type == SLL_TYPE_CRYPTO)
                buddha_heap_mark_set(_receive, HEAP_MARK_SSLC_CRYPTO);
            else if (_receive->header.type == SLL_TYPE_REGISTER)
                buddha_heap_mark_set(_receive, HEAP_MARK_SSLC_REGISTER);
            else
                buddha_heap_mark_set(_receive, HEAP_MARK_SLL_RECEIVE);
            _receive = 0;
        }
    }
    return 0;
}

void sslc_init(void)
{
    fsm_init(&_state, SSLC_STATE_CLOUD_INIT);
    _fp = -1;
    _transmit = 0;
}

void sslc_update(void)
{
    if (SSLC_STATE_CLOUD_INIT == fsm_state(&_state))
    {
        if (_fp >= 0)
            network_tcp_close(_fp);
        _fp = network_tcp_client(HOST_CLOUD, PORT_CLOUD);
        fsm_update(&_state, SSLC_STATE_CLOUD_CRYPTO_REQ);
        if (_fp < 0)
        {
            LogError("connect %s:%d failed.", HOST_CLOUD, PORT_CLOUD);
            fsm_update(&_state, SSLC_STATE_CLOUD_RECONNECT);
        }
    }
    if (SSLC_STATE_CLOUD_RECONNECT == fsm_state(&_state))
    {
        if (fsm_time(&_state) > os_ticks_from(5000))
        {
            LogAction("cloud reconnecting");
            fsm_update(&_state, SSLC_STATE_CLOUD_INIT);
        }
    }
    if (SSLC_STATE_CLOUD_CRYPTO_REQ == fsm_state(&_state))
    {
        int i = 0;
        PacketSigmaSublayerLinkCryptoReq *packet = sll_send(SLL_TYPE_CRYPTO, _terminal_crypto, sizeof(PacketSigmaSublayerLinkCryptoReq), 0, SLL_FLAG_SEND_PATH_CLOUD, 0);
        if (!packet)
        {
            network_tcp_close(_fp);
            _fp = -1;
            fsm_update(&_state, SSLC_STATE_CLOUD_RECONNECT);
            return;
        }
        packet->header.opcode = OPCODE_CRYPTO_REQ;
        os_memcpy(packet->header.auth, crypto_auth(), 16);
        for (i = 0; i < 16; i++)
            packet->header.raw[i] = os_rand();

        uint8_t *verify = (uint8_t *)buddha_heap_find(0, HEAP_MARK_SSLC_VERIFY);
        if (!verify)
            verify = (uint8_t *)buddha_heap_alloc(32, HEAP_MARK_SSLC_VERIFY);

        uint8_t shared[MAX_CRYPTO_PIN] = {0};
        crypto_key_shared(shared, _public_server);
        crypto_encrypto(verify, packet->header.raw, 16, shared);

        fsm_update(&_state, SSLC_STATE_CLOUD_AUTH);

        LogAction("cloud crypto request");
    }
    if (SSLC_STATE_CLOUD_CRYPTO_RESP == fsm_state(&_state))
    {
        PacketReceiveSigmaLayerLink *resp = (PacketReceiveSigmaLayerLink *)buddha_heap_find(0, HEAP_MARK_SSLC_CRYPTO);
        if (!resp)
            return;
        HeaderSigmaSublayerLink *header = (HeaderSigmaSublayerLink *)(resp + 1);
        if (header->opcode == OPCODE_CRYPTO_FAILED)
        {
            LogError("crypto failed");
            buddha_heap_destory(resp);
            network_tcp_close(_fp);
            _fp = -1;
            fsm_update(&_state, SSLC_STATE_CLOUD_RECONNECT);
        }
        else if (header->opcode == OPCODE_CRYPTO_RESP)
        {
            PacketSigmaSublayerLinkCryptoResp *packet = (PacketSigmaSublayerLinkCryptoResp *)(resp + 1);
            uint8_t *verify = (uint8_t *)buddha_heap_find(0, HEAP_MARK_SSLC_VERIFY);
            if (!verify)
            {
                LogError("verify not found.");
                buddha_heap_destory(resp);
                network_tcp_close(_fp);
                _fp = -1;
                fsm_update(&_state, SSLC_STATE_CLOUD_RECONNECT);
                return;
            }

            if (memcmp(packet->ver, verify, 32))
            {
                LogError("verify not match.");
                buddha_heap_destory(resp);
                network_tcp_close(_fp);
                _fp = -1;
                fsm_update(&_state, SSLC_STATE_CLOUD_RECONNECT);
                return;
            }
            buddha_heap_destory(verify);

            PacketSigmaSublayerLinkCryptoVerify *psslcv = sll_send(SLL_TYPE_CRYPTO, _terminal_crypto, sizeof(PacketSigmaSublayerLinkCryptoVerify), 0, SLL_FLAG_SEND_PATH_CLOUD, 0);
            if (!psslcv)
            {
                buddha_heap_destory(resp);
                network_tcp_close(_fp);
                _fp = -1;
                fsm_update(&_state, SSLC_STATE_CLOUD_RECONNECT);
                return;
            }
            psslcv->header.opcode = OPCODE_CRYPTO_AUTH;
            crypto_encrypto(psslcv->ver, packet->raw, 16);
            fsm_update(&_state, SSLC_STATE_CLOUD_REGISTER);
        }
        buddha_heap_destory(resp);
        if (fsm_time(&_state) > os_ticks_ms(5000))
        {
            LogAction("cloud reconnecting");
            fsm_update(&_state, SSLC_STATE_CLOUD_INIT);
            return;
        }
        if (sslc_receive() < 0 || sslc_transmit() < 0)
            fsm_update(&_state, SSLC_STATE_CLOUD_INIT);
    }
    if (SSLC_STATE_CLOUD_REGISTER == fsm_state(&_state))
    {
        if (memcmp(sll_src(0), sll_terminal_local(), MAX_TERMINAL_ID))
        {
            fsm_update(&_state, SSLC_STATE_CLOUD_SERVICE);
            
            sigma_event_dispatch(EVENT_SIGMA_CLOUD_STARTED, 0);
            return;
        }
        PacketSigmaSublayerLinkRegisterReq *psslrr = sll_send(SLL_TYPE_REGISTER, _terminal_register, sizeof(PacketSigmaSublayerLinkRegisterReq), 0, SLL_FLAG_SEND_PATH_CLOUD, 0);
        if (!psslrr)
        {
            network_tcp_close(_fp);
            _fp = -1;
            fsm_update(&_state, SSLC_STATE_CLOUD_RECONNECT);
            return;
        }
        psslrr->header.opcode = OPCODE_REGISTER_REQ;
        memcpy(psslrr->auth, crypto_auth(), 16);
        fsm_update(&_state, SSLC_STATE_CLOUD_REGISTER_WAIT);
    }
    if (SSLC_STATE_CLOUD_REGISTER_WAIT == fsm_state(&_state))
    {
        PacketReceiveSigmaLayerLink *resp = (PacketReceiveSigmaLayerLink *)buddha_heap_find(0, HEAP_MARK_SSLC_REGISTER);
        if (!resp)
            return;
        HeaderSigmaSublayerLink *header = (HeaderSigmaSublayerLink *)(resp + 1);
        if (header->opcode == OPCODE_REGISTER_FAILED)
        {
            buddha_heap_destory(resp);
            network_tcp_close(_fp);
            _fp = -1;
            fsm_update(&_state, SSLC_STATE_CLOUD_RECONNECT);
            return;
        }
        else if (header->opcode == OPCODE_REGISTER_RESP)
        {
            PacketSigmaSublayerLinkRegisterResp *packet = (PacketSigmaSublayerLinkRegisterResp *)(resp + 1);
            sll_src(packet->id);
            uint8_t *id = (uint8_t *)sigma_event_dispatch(EVENT_SIGMA_TERMINAL_REGISTER, MAX_TERMINAL_ID);
            if (id)
                os_memcpy(id, packet->id, MAX_TERMINAL_ID);
            fsm_update(&_state, SSLC_STATE_CLOUD_SERVICE);
            
            sigma_event_dispatch(EVENT_SIGMA_CLOUD_STARTED, 0);
        }
        buddha_heap_destory(resp);
        if (sslc_receive() < 0 || sslc_transmit() < 0)
            fsm_update(&_state, SSLC_STATE_CLOUD_INIT);
    }
    if (SSLC_STATE_CLOUD_SERVICE == fsm_state(&_state))
    {
        if (sslc_receive() < 0 || sslc_transmit() < 0)
            fsm_update(&_state, SSLC_STATE_CLOUD_INIT);
    }
}
