#include "console.h"
#include "interface_network.h"
#include "interface_os.h"
#include "interface_io.h"
#include "driver_network_linux.h"
#include "buddha_heap.h"
#include "command.h"
#include "entity.h"
#include "zigbee.h"
#include "hex.h"
#include "log.h"

#define CONSOLE_PORT 7600

#define CONSOLE_MISSION_INPUT 0
#define CONSOLE_MISSION_KEY_CLICK 1

typedef struct _console_client
{
    struct _console_client *_next;

    int fp;

    char *command;
    int size;

    uint8_t state;
    union
    {
        struct
        {
            uint32_t start;
        }key_click;
    }ctx;
}ConsoleClient;

static int _fp = -1;
static ConsoleClient *_clients = 0;

static void response_console_simple_desc(int ret, 
    uint16_t addr, uint8_t ep, 
    uint16_t profile, uint16_t type,
    uint8_t version,
    uint8_t *ins, uint8_t in_count,
    uint8_t *outs, uint8_t out_count)
{
    LogAction("simple_desc ret:%d, addr:%04x ep:%u profile:%04x type:%04x version:%02x", ret, 
        addr, ep, 
        profile, type,
        version);
}

static int console_input(ConsoleClient *client)
{
    char info[256] = {0};
    LogAction("%d console command:%s", client->fp, client->command);
    
    if (!os_memcmp(client->command, "quit", 4ul))
    {
        LogAction("%d console client quit", client->fp);
        return -1;
    }
    else if (!os_memcmp(client->command, "click", 5ul))
    {
        /*
        io_set(BUTTON, 0);
        client->state = CONSOLE_MISSION_KEY_CLICK;
        client->ctx.key_click.start = os_ticks();
        */
    }
    else if (!os_memcmp(client->command, "start", 5ul))
    {
        zigbee_discover_start();
    }
    else if (!os_memcmp(client->command, "stop", 4ul))
    {
        zigbee_discover_stop();
    }
    else if (!os_memcmp(client->command, "simple", 6ul))
    {
        char *ep = 0;
        char *addr = 0;
        addr = strstr(client->command, " ");
        if (!addr)
        {
            sprintf(info, "syntax error\r\n");
            network_tcp_send(client->fp, info, strlen(info));
            return 0;
        }
        *addr++ = 0;
        
        ep = strstr(addr, " ");
        if (!ep)
        {
            sprintf(info, "syntax error\r\n");
            network_tcp_send(client->fp, info, strlen(info));
            return 0;
        }
        *ep++ = 0;

        zigbee_simple_desc(atoi(addr), atoi(ep), response_console_simple_desc);
    }
    else if (!os_memcmp(client->command, "find", 4ul))
    {
        uint32_t addr = 0;
        char *type = 0;
        type = strstr(client->command, " ");
        if (!type)
        {
            sprintf(info, "syntax error\r\n");
            network_tcp_send(client->fp, info, strlen(info));
            return 0;
        }
        *type++ = 0;
        while ((addr = entity_find(atoi(type), addr)))
        {
            uint16_t entity;
            property_get(addr, PROPERTY_ENTITY_ID, 0, &entity, 2ul);
            sprintf(info, "entity 0x%08x:%d\r\n", addr, entity);
            network_tcp_send(client->fp, info, strlen(info));
        }
    }
    else if (!os_memcmp(client->command, "command", 7))
    {
        uint8_t type;
        uint32_t addr = 0;
        uint8_t command[255] = {0};
        uint8_t link[255] = {0};
        char *entity = 0, *cmd = 0, *lnk = 0;

        entity = strstr(client->command, " ");
        if (!entity)
        {
            char info[256] = {0};
            sprintf(info, "syntax error(entity not found)\r\n");
            network_tcp_send(client->fp, info, strlen(info));
            return 0;
        }
        *entity++ = 0;

        cmd = strstr(entity, " ");
        if (cmd)
            *cmd++ = 0;

        lnk = strstr(cmd, " ");
        if (lnk)
            *lnk++ = 0;

        if (cmd)
            hex2bin(command, cmd, strlen(cmd));
        if (lnk)
            hex2bin(link, lnk, strlen(lnk));

        addr = entity_get(atoi(entity));
        if (!addr)
        {
            char info[256] = {0};
            sprintf(info, "entity %u not found\r\n", atoi(entity));
            network_tcp_send(client->fp, info, strlen(info));
            return 0;
        }
        property_get(addr, PROPERTY_ENTITY_TYPE, 0, &type, 1ul);

        command_execute(atoi(entity), type, command, strlen(cmd) / 2, lnk ? link : 0);
    }
    else if (!os_memcmp(client->command, "heap_check", 10))
    {
        buddha_heap_check();
    }
    else
    {
        LogAction("%d unknown console command:%s", client->fp, client->command);
    }
    return 0;
}

static int console_mission(ConsoleClient *client)
{
    if (CONSOLE_MISSION_INPUT == client->state)
    {
        if (!client->command)
            client->command = (char *)os_malloc(1024ul);
        if (client->command)
        {
            int ret = network_tcp_recv(client->fp, client->command + client->size, 1024 - client->size);
            if (ret < 0)
                return -1;
            if (ret)
            {
                client->size += ret;
                for (ret = 0; ret < client->size; ret++)
                {
                    if (client->command[ret] == '\r' || client->command[ret] == '\n')
                        break;
                }
                if (ret < client->size)
                {
                    client->command[ret] = 0;

                    if (console_input(client) < 0)
                        return -1;
                    client->size = 0;
                    os_free(client->command);
                    client->command = 0;
                }
            }
        }
    }
    else if (CONSOLE_MISSION_KEY_CLICK == client->state)
    {
        if (os_ticks_from(client->ctx.key_click.start) > os_ticks_ms(300))
        {
            client->state = CONSOLE_MISSION_INPUT;
            //io_set(BUTTON, 1);
        }
    }
    return 0;
}

void console_init(void)
{
}

void console_update(void)
{
    int fp;
    uint8_t ip[4] = {0};
    uint16_t port;
    ConsoleClient *prev, *client = 0;

    if (_fp < 0)
        _fp = network_tcp_server(CONSOLE_PORT);

    fp = network_tcp_accept(_fp, ip, &port);
    if (fp < 0)
    {
        if (fp < -1)
        {
            network_tcp_close(_fp);
            _fp = -1;
            return;
        }
    }
    else
    {
        client = (ConsoleClient *)os_malloc(sizeof(ConsoleClient));
        if (!client)
        {
            network_tcp_close(fp);
        }
        else
        {
            memset(client, 0, sizeof(ConsoleClient));
            client->fp = fp;
            client->_next = _clients;
            _clients = client;
            network_tcp_send(client->fp, "welcome\r\n", 7ul);
            LogAction("%d new console client", fp);
        }
    }

    prev = 0;
    client = _clients;
    while (client)
    {
        if (console_mission(client) < 0)
            break;
        prev = client;
        client = client->_next;
    }
    if (client)
    {
        if (prev)
            prev->_next = client->_next;
        else
            _clients = client->_next;
        network_tcp_close(client->fp);
        if (client->command)
            os_free(client->command);
        free(client);
    }
}
