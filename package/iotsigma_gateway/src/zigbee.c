#include "zigbee.h"
#include "interface_os.h"
#include "interface_usart.h"
#include "driver_linux.h"
#include "driver_led.h"
#include "driver_usart_linux.h"
#include "buddha_heap.h"
#include "log.h"

#define MAX_GPIO 3

#define ZIGBEE_STATE_INIT 0
#define ZIGBEE_STATE_DISCOVER 1
#define ZIGBEE_STATE_SERVICE 2

#define Simple_Desc_req         ((uint16_t)0x0004)
#define Active_EP_req           ((uint16_t)0x0005)
#define Device_annce            ((uint16_t)0x0013)

typedef struct
{
    uint8_t value;
    uint8_t direction;
}ZigbeeGPIO;

typedef struct _zigbee_responser
{
    ZigbeeResponse response;
    void *ctx;

    uint32_t timer;
    uint16_t interval;
    uint8_t session;
    uint8_t retry;
}ZigbeeResponser;

static uint8_t _state = ZIGBEE_STATE_INIT;
static uint8_t _session = 1;
static ZigbeeGPIO _gpio[MAX_GPIO] = {0};
static uint32_t _timer = 0;
static void (*_listener)(uint8_t event, void *data, uint8_t size);

static void zigbee_response_handshake(void *ctx, uint8_t status, uint8_t retry, uint8_t *data, uint8_t size)
{
    if (OPCODE_ZIGBEE_RESPONSE_SUCCESSED != status)
    {
        zigbee_request(OPCODE_ZIGBEE_HANDSHAKE, zigbee_session(), 0, 0, retry + 1, 0, zigbee_response_handshake, 0);
        return;
    }
    _state = ZIGBEE_STATE_SERVICE;
    led_set(LED_ID_APP, 0, 0);
    LogAction("zigbee handleshark successed");
}

static void zigbee_response_discover_start(void *ctx, uint8_t status, uint8_t retry, uint8_t *data, uint8_t size)
{
    if (OPCODE_ZIGBEE_RESPONSE_SUCCESSED != status)
    {
        zigbee_request(OPCODE_ZIGBEE_HANDSHAKE, zigbee_session(), 0, 0, retry + 1, 0, zigbee_response_handshake, 0);
        return;
    }
    _state = ZIGBEE_STATE_DISCOVER;
}

static void zigbee_response_discover_stop(void *ctx, uint8_t status, uint8_t retry, uint8_t *data, uint8_t size)
{
    _state = ZIGBEE_STATE_SERVICE;
}

static void usart_packet(uint8_t opcode, uint8_t session, uint8_t *buffer, uint16_t size)
{
    LogAction("opcode:%u session:%u size:%u", opcode, session, size);

    if (OPCODE_ZIGBEE_RESPONSE_FAILED == opcode || 
        OPCODE_ZIGBEE_RESPONSE_SUCCESSED == opcode || 
        OPCODE_ZIGBEE_RESPONSE_TIMEOUT == opcode ||
        OPCODE_ZIGBEE_EVENT_GROUP_RSP == opcode ||
        OPCODE_ZIGBEE_EVENT_SCENE_RSP == opcode)
    {
        ZigbeeResponser *responser = 0;
        while ((responser = buddha_heap_find(responser, HEAP_MARK_ZIGBEE_RESPONSER)) && responser->session != session);
        if (responser)
        {
            responser->response(responser->ctx, opcode, responser->retry, buffer, size);
            buddha_heap_destory(responser);
        }
    }
    else if (_listener)
    {
        return;
    }
    else if (OPCODE_ZIGBEE_EVENT_ZDO == opcode)
    {
        ZigbeeZDOEvent e;

        uint8_t *packet = buffer;

        e.mode = *packet++;
        switch (e.mode)
        {
            case 1://afAddrGroup
            case 2://afAddr16Bit
                e.addr.sht = *(uint16_t *)packet;
                packet += 2;
                break;
            case 3://afAddr64Bit
                e.addr.ieee = packet;
                packet += 8;
                break;
        }

        e.cluster = *(uint16_t *)packet;
        packet += 2;

        if (Device_annce == e.cluster)
        {
            e.node_addr = *(uint16_t *)packet;
            packet += 2;

            e.payload.device_annouce.ieee = packet;
            packet += Z_EXTADDR_LEN;
            e.payload.device_annouce.capabilities = *packet++;

            LogAction("device annce|addr:0x%04x|ieee:%02x%02x%02x%02x%02x%02x%02x%02x|capabilities:%02x",
                e.node_addr,
                e.payload.device_annouce.ieee[0],
                e.payload.device_annouce.ieee[1],
                e.payload.device_annouce.ieee[2],
                e.payload.device_annouce.ieee[3],
                e.payload.device_annouce.ieee[4],
                e.payload.device_annouce.ieee[5],
                e.payload.device_annouce.ieee[6],
                e.payload.device_annouce.ieee[7],
                e.payload.device_annouce.capabilities);

            _listener(EVENT_DEVICE_ANNCE, &e, sizeof(ZigbeeZDOEvent));
        }
        else
        {
            _listener(opcode, buffer, size);
        }
    }
    else if (OPCODE_ZIGBEE_ATTRIBUTE_REPORT == opcode)
    {
        ZigbeeZCLEvent e;

        uint8_t *packet = buffer;

        e.mode = *packet++;
        switch (e.mode)
        {
            case 1://afAddrGroup
            case 2://afAddr16Bit
                e.addr.sht = *(uint16_t *)packet;
                packet += 2;
                break;
            case 3://afAddr64Bit
                e.addr.ieee = packet;
                packet += 8;
                break;
        }
        e.ep = *packet++;

        e.cluster = *(uint16_t *)packet;
        packet += 2;

        e.attribute.report.count = *packet++;
        e.attribute.report.attrs = packet;

        _listener(EVENT_ATTRIBUTE_REPORT, &e, sizeof(ZigbeeZCLEvent));
    }
    else
    {
        _listener(opcode, buffer, size);
    }
}

static uint16_t usart_handler(uint8_t *buffer, uint16_t size)
{
    uint16_t pos = 0;
    while (pos < size)
    {
        uint8_t cs = 0;
        uint16_t i = 0;
        while (pos < size && buffer[pos] != 0xaa)
            pos++;
        if (pos + 6 > size)
            return pos;
        if ((uint8_t)(buffer[pos + 0] + buffer[pos + 1] + buffer[pos + 2]))
        {
            LogError("header cs error");
            pos++;
            continue;
        }
        if (pos + 3 + 1 + 1 + buffer[pos + 1] + 1 > size)
            return pos;
        for (i = 0; i < 1 + 1 + buffer[pos + 1] + 1; i++)
            cs += buffer[pos + 3 + i];
        if (cs)
        {
            LogError("cs error");
            pos++;
            continue;
        }
        usart_packet(buffer[pos + 3], buffer[pos + 4], buffer + pos + 5, buffer[pos + 1]);
        pos += 6 + buffer[pos + 1];
    }
    return pos;
}

uint8_t zclGetDataTypeLength( uint8_t dataType )
{
  uint8_t len;

  switch ( dataType )
  {
    case ZCL_DATATYPE_DATA8:
    case ZCL_DATATYPE_BOOLEAN:
    case ZCL_DATATYPE_BITMAP8:
    case ZCL_DATATYPE_INT8:
    case ZCL_DATATYPE_UINT8:
    case ZCL_DATATYPE_ENUM8:
      len = 1;
      break;

    case ZCL_DATATYPE_DATA16:
    case ZCL_DATATYPE_BITMAP16:
    case ZCL_DATATYPE_UINT16:
    case ZCL_DATATYPE_INT16:
    case ZCL_DATATYPE_ENUM16:
    case ZCL_DATATYPE_SEMI_PREC:
    case ZCL_DATATYPE_CLUSTER_ID:
    case ZCL_DATATYPE_ATTR_ID:
      len = 2;
      break;

    case ZCL_DATATYPE_DATA24:
    case ZCL_DATATYPE_BITMAP24:
    case ZCL_DATATYPE_UINT24:
    case ZCL_DATATYPE_INT24:
      len = 3;
      break;

    case ZCL_DATATYPE_DATA32:
    case ZCL_DATATYPE_BITMAP32:
    case ZCL_DATATYPE_UINT32:
    case ZCL_DATATYPE_INT32:
    case ZCL_DATATYPE_SINGLE_PREC:
    case ZCL_DATATYPE_TOD:
    case ZCL_DATATYPE_DATE:
    case ZCL_DATATYPE_UTC:
    case ZCL_DATATYPE_BAC_OID:
      len = 4;
      break;

   case ZCL_DATATYPE_UINT40:
   case ZCL_DATATYPE_INT40:
       len = 5;
       break;

   case ZCL_DATATYPE_UINT48:
   case ZCL_DATATYPE_INT48:
       len = 6;
       break;

   case ZCL_DATATYPE_UINT56:
   case ZCL_DATATYPE_INT56:
       len = 7;
       break;

   case ZCL_DATATYPE_DOUBLE_PREC:
   case ZCL_DATATYPE_IEEE_ADDR:
   case ZCL_DATATYPE_UINT64:
   case ZCL_DATATYPE_INT64:
     len = 8;
     break;

    case ZCL_DATATYPE_128_BIT_SEC_KEY:
     len = SEC_KEY_LEN;
     break;

    case ZCL_DATATYPE_NO_DATA:
    case ZCL_DATATYPE_UNKNOWN:
      // Fall through

    default:
      len = 0;
      break;
  }

  return ( len );
}

uint16_t zclGetAttrDataLength( uint8_t dataType, uint8_t *pData )
{
    uint16_t dataLen = 0;

    if ( dataType == ZCL_DATATYPE_LONG_CHAR_STR || dataType == ZCL_DATATYPE_LONG_OCTET_STR )
    {
        dataLen = ((uint16_t)pData[1]) << 8;
        dataLen |= pData[0];
        dataLen += 2;
    }
    else if ( dataType == ZCL_DATATYPE_CHAR_STR || dataType == ZCL_DATATYPE_OCTET_STR )
    {
        dataLen = *pData + 1;
    }
    else
    {
        dataLen = zclGetDataTypeLength( dataType );
    }

    return ( dataLen );
}

void zigbee_init(void (*listener)(uint8_t event, void *data, uint8_t size))
{
    _listener = listener;

    led_set(LED_ID_APP, 0xaa, 0xff);

    if (usart_open(0, 115200, 8, 'N', 1, usart_handler) < 0)
        return;

    zigbee_request(OPCODE_ZIGBEE_HANDSHAKE, zigbee_session(), 0, 0, 0, 0, zigbee_response_handshake, 0);

    _state = ZIGBEE_STATE_INIT;
}

void zigbee_update(void)
{
    ZigbeeResponser * responser = 0;
    while ((responser = buddha_heap_find(responser, HEAP_MARK_ZIGBEE_RESPONSER)) && os_ticks_from(responser->timer) < os_ticks_ms(responser->interval));
    if (responser)
    {
        LogError("response timeout");
        responser->response(responser->ctx, OPCODE_ZIGBEE_RESPONSE_TIMEOUT, responser->retry, 0, 0);
        buddha_heap_destory(responser);
    }

    if (ZIGBEE_STATE_DISCOVER == _state)
    {
        if (os_ticks_from(_timer) > os_ticks_ms(10000))
            zigbee_discover_stop();
    }
}

int zigbee_idle(void)
{
    return !buddha_heap_find(0, HEAP_MARK_ZIGBEE_RESPONSER);
}

uint8_t zigbee_session(void)
{
    uint8_t session = _session++;
    if (!_session)
        _session = 1;
    return session;
}

int zigbee_request(uint8_t opcode, uint8_t session, uint8_t *data, uint8_t size, uint8_t retry, uint16_t timeout, ZigbeeResponse response, void *ctx)
{
    uint8_t i = 0;
    uint8_t cs = 0xaa;
    ZigbeeResponser *responser = 0;
    if (response)
    {
        responser = (ZigbeeResponser *)buddha_heap_alloc(sizeof(ZigbeeResponser), HEAP_MARK_ZIGBEE_RESPONSER);
        if (!responser)
            return -1;
        responser->response = response;
        responser->ctx = ctx;
        responser->timer = os_ticks();
        responser->interval = timeout ? timeout : 5000;
        responser->retry = retry;
        responser->session = session;
    }
    usart_write(0, &cs, 1);
    cs = size;
    usart_write(0, &cs, 1);
    cs += 0xaa;
    cs = 0 - cs;
    usart_write(0, &cs, 1);
    usart_write(0, &opcode, 1);
    cs = session;
    usart_write(0, &cs, 1);
    usart_write(0, data, size);
    cs += opcode;
    for (i = 0; i < size; i++)
        cs += data[i];
    cs = 0 - cs;
    usart_write(0, &cs, 1);
    LogAction("opcode:%u session:%u size:%u", opcode, session, size);
    return 0;
}

int zigbee_discover_start(void)
{
    uint8_t time = 0xfe;
    zigbee_request(OPCODE_ZIGBEE_DISCOVER_START, zigbee_session(), &time, 1, 0, 0, zigbee_response_discover_start, 0);
    _timer = os_ticks();

    return 0;
}

int zigbee_discover_stop(void)
{
    if (_state != ZIGBEE_STATE_DISCOVER)
        return -1;

    zigbee_request(OPCODE_ZIGBEE_DISCOVER_STOP, zigbee_session(), 0, 0, 0, 0, zigbee_response_discover_stop, 0);

    return 0;
}

typedef struct
{
    uint16_t addr;
    void (*rsp)(int ret, uint16_t addr, uint8_t *eps, uint8_t count);
}ContextActiveEndpoint;

static void packet_active_ep(uint8_t retry, ZigbeeResponse response, ContextActiveEndpoint *ctx)
{
    uint8_t session = 0;
    uint8_t buffer[9];
    uint8_t *packet = buffer;

    *packet++ = afAddr16Bit;
    *(uint16_t *)packet = ctx->addr;
    packet += 2;
    *packet++ = 0;
    *(uint16_t *)packet = Active_EP_req;
    packet += 2;
    *packet++ = session = zigbee_session();
    *(uint16_t *)packet = ctx->addr;
    packet += 2;
    zigbee_request(OPCODE_ZIGBEE_DATA_REQUEST, session, buffer, 9, retry, 5000, response, ctx);
}

static void response_active_ep(void *ctx, uint8_t status, uint8_t retry, uint8_t *data, uint8_t size)
{
    uint8_t *packet = data;
    ContextActiveEndpoint *cae = (ContextActiveEndpoint *)ctx;

    if (OPCODE_ZIGBEE_RESPONSE_SUCCESSED != status)
    {
        if (retry < 3)
        {
            packet_active_ep(retry + 1, response_active_ep, cae);
            return;
        }
        cae->rsp(-1, cae->addr, 0, 0);
        os_free(cae);
        return;
    }

    switch (*packet++)
    {
        case 1://afAddrGroup
        case 2://afAddr16Bit
            packet += 2;
            break;
        case 3://afAddr64Bit
            packet += 8;
            break;
    }

    packet += 2;//cluster
    packet++; //status
    packet += 2;//addr

    cae->rsp(0, cae->addr, packet + 1, packet[0]);
    os_free(cae);
}

void zigbee_active_ep(uint16_t addr, void (*rsp)(int ret, uint16_t addr, uint8_t *eps, uint8_t count))
{
	ContextActiveEndpoint *ctx = 0;

    if (!rsp)
        return;
    if (_state == ZIGBEE_STATE_INIT)
    {
        rsp(-1, addr, 0, 0);
        return;
    }

    ctx = (ContextActiveEndpoint *)os_malloc(sizeof(ContextActiveEndpoint));
    if (!ctx)
    {
        LogError("out of memory");
        rsp(-1, addr, 0, 0);
        return;
    }
    ctx->addr = addr;
    ctx->rsp = rsp;

    packet_active_ep(0, response_active_ep, ctx);
}

typedef struct
{
    uint16_t addr;
    uint8_t ep;
    void (*rsp)(int ret, 
        uint16_t addr, uint8_t ep, 
        uint16_t profile, uint16_t type, 
        uint8_t version, 
        uint8_t *ins, uint8_t in_count, 
        uint8_t *outs, uint8_t out_count);
}ContextSimpleDesc;

void packet_simple_desc(uint8_t retry, ZigbeeResponse response, ContextSimpleDesc *ctx)
{
    uint8_t session = 0;
    uint8_t buffer[10];
    uint8_t *packet = buffer;

    *packet++ = afAddr16Bit;
    *(uint16_t *)packet = ctx->addr;
    packet += 2;
    *packet++ = 0;
    *(uint16_t *)packet = Simple_Desc_req;
    packet += 2;
    *packet++ = session = zigbee_session();
    *(uint16_t *)packet = ctx->addr;
    packet += 2;
    *packet++ = ctx->ep;
    zigbee_request(OPCODE_ZIGBEE_DATA_REQUEST, session, buffer, 10, retry, 5000, response, ctx);
}

static void response_simple_desc(void *ctx, uint8_t status, uint8_t retry, uint8_t *data, uint8_t size)
{
    uint16_t cluster;
    uint8_t *packet = data;
    ContextSimpleDesc *csd = (ContextSimpleDesc *)ctx;

    if (OPCODE_ZIGBEE_RESPONSE_SUCCESSED != status)
    {
        if (retry < 3)
        {
            packet_simple_desc(retry + 1, response_simple_desc, csd);
            return;
        }
        csd->rsp(-1, 
            csd->addr, csd->ep,
            0, 0,
            0,
            0, 0,
            0, 0);
        os_free(csd);
        return;
    }

    switch (*packet++)
    {
        case 1://afAddrGroup
        case 2://afAddr16Bit
            packet += 2;
            break;
        case 3://afAddr64Bit
            packet += 8;
            break;
    }

    cluster = *(uint16_t *)packet;
    packet += 2;

    if (0 != *packet++)
    {
        if (retry < 3)
        {
            packet_simple_desc(retry + 1, response_simple_desc, csd);
            return;
        }
        csd->rsp(-1,
            csd->addr, csd->ep,
            0, 0,
            0,
            0, 0,
            0, 0);
        os_free(csd);
        return;
    }

    packet += 2;//node addr
    packet++;//length

    packet++; //ep

    csd->rsp(0, 
        csd->addr, csd->ep,
        *(uint16_t *)packet, *(uint16_t *)(packet + 2),
        packet[4],
        packet + 6, packet[5],
        packet + 6 + packet[5] * 2 + 1, packet[6 + packet[5] * 2]);
    os_free(csd);
}

void zigbee_simple_desc(uint16_t addr, uint8_t ep, void (*rsp)(int ret, 
    uint16_t addr, uint8_t ep, 
    uint16_t profile, uint16_t type, 
    uint8_t version, 
    uint8_t *ins, uint8_t in_count, 
    uint8_t *outs, uint8_t out_count))
{
	ContextSimpleDesc *ctx = 0;

    if (!rsp)
        return;

    if (_state == ZIGBEE_STATE_INIT)
    {
        rsp(-1,
            addr, ep,
            0, 0,
            0,
            0, 0,
            0, 0);
        return;
    }

    ctx = (ContextSimpleDesc *)os_malloc(sizeof(ContextSimpleDesc));
    if (!ctx)
    {
        LogError("out of memory");
        rsp(-1,
            addr, ep,
            0, 0,
            0,
            0, 0,
            0, 0);
        return;
    }
    ctx->addr = addr;
    ctx->ep = ep;
    ctx->rsp = rsp;

    packet_simple_desc(0, response_simple_desc, ctx);
}

typedef struct
{
	void (*rsp)(int ret,
        uint16_t addr, uint8_t ep,
        uint16_t cluster, uint16_t attribute,
        uint8_t *attrs, uint8_t count);
    uint16_t addr;
    uint8_t ep;
    uint16_t cluster;
    uint16_t attribute;
}ContextAttributeRead;

void packet_attribute_read(uint8_t retry, ZigbeeResponse response, ContextAttributeRead *ctx)
{
    uint8_t buffer[11];
    uint8_t *packet = buffer;

    *packet++ = afAddr16Bit;
    *(uint16_t *)packet = ctx->addr;
    packet += 2;
    *packet++ = ctx->ep;
    *(uint16_t *)packet = ctx->cluster;
    packet += 2;
    *packet++ = ZCL_CMD_READ;
    *packet++ = 0;//specific
    *packet++ = ZCL_FRAME_CLIENT_SERVER_DIR;//direction
    *(uint16_t *)packet = ctx->attribute;
    packet += 2;
    zigbee_request(OPCODE_ZIGBEE_ZCL_SEND, zigbee_session(), buffer, 11, retry, 5000ul, response, ctx);
}

static void response_attribute_read(void *ctx, uint8_t status, uint8_t retry, uint8_t *data, uint8_t size)
{
    uint8_t *packet = data;
    ContextAttributeRead *car = (ContextAttributeRead *)ctx;

    if (OPCODE_ZIGBEE_RESPONSE_SUCCESSED != status)
    {
        if (retry < 3)
        {
            packet_attribute_read(retry + 1, response_attribute_read, car);
            return;
        }
        car->rsp(-1,
            car->addr, car->ep,
            car->cluster, car->attribute,
            0, 0);
        os_free(car);
        return;
    }

    switch (*packet++)
    {
        case 1://afAddrGroup
        case 2://afAddr16Bit
            packet += 2;
            break;
        case 3://afAddr64Bit
            packet += 8;
            break;
    }
    packet++;//ep

    packet += 2;//cluster

    car->rsp(0,
        car->addr, car->ep,
        car->cluster, car->attribute,
        packet + 1, packet[0]);
    os_free(car);
}

void zigbee_attribute_read(uint16_t addr, uint8_t ep, uint16_t cluster, uint16_t attribute, void (*rsp)(int ret, 
    uint16_t addr, uint8_t ep,
    uint16_t cluster, uint16_t attribute,
    uint8_t *attrs, uint8_t count))
{
	ContextAttributeRead *ctx = 0;
    if (!rsp)
        return;
    if (_state == ZIGBEE_STATE_INIT)
    {
        rsp(-1, addr, ep, cluster, attribute, 0, 0);
        return;
    }

    ctx = (ContextAttributeRead *)os_malloc(sizeof(ContextAttributeRead));
    if (!ctx)
    {
        LogError("out of memory");
        rsp(-1, addr, ep, cluster, attribute, 0, 0);
        return;
    }
    ctx->addr = addr;
    ctx->ep = ep;
    ctx->cluster = cluster;
    ctx->attribute = attribute;
    ctx->rsp = rsp;

    packet_attribute_read(0, response_attribute_read, ctx);
}

typedef struct _context_zcl_command
{
	void (*rsp)(int ret, void *ctx, uint16_t addr, uint8_t ep, uint16_t cluster, uint8_t command, uint8_t *data, uint8_t size);
	
	uint8_t size;
	uint8_t mode;
	uint16_t addr;
	uint16_t cluster;
	uint8_t ep;
	uint8_t command;
	uint8_t type;
	uint8_t buffer[];
}ContextZCLCommand;

void packet_zcl_command(uint8_t retry, ZigbeeResponse response, ContextZCLCommand *ctx)
{
	uint8_t *buffer = 0;
    uint8_t *packet = (uint8_t *)os_malloc(9 + ctx->size);
    if (!packet)
        return;
    buffer = packet;

    *packet++ = ctx->mode;
    *(uint16_t *)packet = ctx->addr;
    packet += 2;
    *packet++ = ctx->ep;
    *(uint16_t *)packet = ctx->cluster;
    packet += 2;
    *packet++ = ctx->command;
    *packet++ = ctx->type;//specific
    *packet++ = ZCL_FRAME_CLIENT_SERVER_DIR;//direction
    if (ctx->size)
        os_memcpy(packet, ctx->buffer, ctx->size);
    zigbee_request(OPCODE_ZIGBEE_ZCL_SEND, zigbee_session(), buffer, 9 + ctx->size, retry, 5000ul, response, ctx);
    os_free(buffer);
}

static void response_zcl_command(void *ctx, uint8_t status, uint8_t retry, uint8_t *data, uint8_t size)
{
    uint8_t *packet = data;
    ContextZCLCommand *car = (ContextZCLCommand *)ctx;

    if (OPCODE_ZIGBEE_RESPONSE_FAILED == status ||
        OPCODE_ZIGBEE_RESPONSE_TIMEOUT == status)
    {
        if (retry < 3)
        {
            packet_zcl_command(retry + 1, response_zcl_command, car);
            return;
        }
        car->rsp(-1,
            car->buffer + car->size,
            car->addr, car->ep,
            car->cluster, car->command, 0, 0);
        os_free(car);
        return;
    }

    size--;
    switch (*packet++)
    {
        case 1://afAddrGroup
        case 2://afAddr16Bit
            packet += 2;
            size -= 2;
            break;
        case 3://afAddr64Bit
            packet += 8;
            size -= 8;
            break;
    }
    packet++;//ep
    size--;
    
    packet += 2;//cluster
    size -= 2;

    if (OPCODE_ZIGBEE_EVENT_GROUP_RSP == status || OPCODE_ZIGBEE_EVENT_SCENE_RSP == status)
    {
        if (packet[0] != ZCL_STATUS_SUCCESS && 
            packet[0] != ZCL_STATUS_DUPLICATE_EXISTS)
        {
            LogError("status:0x%02x", packet[0]);

            car->rsp(-1,
                car->buffer + car->size,
                car->addr, car->ep,
                car->cluster, car->command, packet, 1);
            os_free(car);
            return;
        }
    }

    car->rsp(0,
        car->buffer + car->size,
        car->addr, car->ep,
        car->cluster, car->command, packet, size);
    os_free(car);
}

void *zigbee_zcl_command(
    uint16_t addr, uint8_t ep, 
    uint16_t cluster, uint8_t command, 
    uint8_t frame_type,
    const void *payload, uint8_t size, 
    void (*rsp)(int ret, void *ctx, uint16_t addr, uint8_t ep, uint16_t cluster, uint8_t command, uint8_t *data, uint8_t size), uint8_t ctx_size)
{
	ContextZCLCommand *ctx = 0;
    if (_state == ZIGBEE_STATE_INIT)
        return 0;

    ctx = (ContextZCLCommand *)os_malloc(sizeof(ContextZCLCommand) + size + ctx_size);
    if (!ctx)
    {
        LogError("out of memory");
        return 0;
    }
    ctx->mode = afAddr16Bit;
    ctx->addr = addr;
    ctx->ep = ep;
    ctx->cluster = cluster;
    ctx->command = command;
    ctx->type = frame_type;
    ctx->size = size;
    if (ctx->size)
        os_memcpy(ctx->buffer, payload, ctx->size);
    ctx->rsp = rsp;

    packet_zcl_command(0, rsp ? response_zcl_command : 0, ctx);
    if (rsp)
        return ctx->buffer + ctx->size;
    os_free(ctx);
    return 0;
}

void *zigbee_zcl_command_mcast(
    uint16_t addr, uint8_t ep, 
    uint16_t cluster, uint8_t command, 
    uint8_t frame_type,
    const void *payload, uint8_t size, 
    void (*rsp)(int ret, void *ctx, uint16_t addr, uint8_t ep, uint16_t cluster, uint8_t command, uint8_t *data, uint8_t size), uint8_t ctx_size)
{
	ContextZCLCommand *ctx = 0;
    if (_state == ZIGBEE_STATE_INIT)
        return 0;

    ctx = (ContextZCLCommand *)os_malloc(sizeof(ContextZCLCommand) + size + ctx_size);
    if (!ctx)
    {
        LogError("out of memory");
        return 0;
    }
    ctx->mode = afAddrGroup;
    ctx->addr = addr;
    ctx->ep = ep;
    ctx->type = frame_type;
    ctx->cluster = cluster;
    ctx->command = command;
    ctx->size = size;
    if (ctx->size)
        os_memcpy(ctx->buffer, payload, ctx->size);
    ctx->rsp = rsp;

    packet_zcl_command(0, rsp ? response_zcl_command : 0, ctx);
    if (rsp)
        return ctx->buffer + ctx->size;
    os_free(ctx);
    return 0;
}
