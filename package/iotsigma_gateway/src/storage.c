#include "storage.h"
#include "interface_io.h"
#include "interface_network.h"
#include "driver_led.h"
#include "driver_network_linux.h"
#include "cluster.h"
#include "log.h"

#if defined(PLATFORM_LINUX)

#include <stdio.h>

static uint8_t _load_terminal = 0;
static uint8_t _terminal[MAX_TERMINAL_ID] = {0};

static uint8_t _load_cluster = 0;
static uint8_t _cluster[MAX_CLUSTER_ID] = {0};
static uint8_t _pin[MAX_CLUSTER_PIN] = {0};

uint8_t dhcp_enabled(void)
{
    return 1;
}

const uint8_t *terminal_id(void)
{
    if (!_load_terminal)
    {
        char path[32] = {0};
        sprintf(path, "%02x%02x%02x%02x%02x%02x.terminal", 
            *(network_net_mac() + 0),
            *(network_net_mac() + 1),
            *(network_net_mac() + 2),
            *(network_net_mac() + 3),
            *(network_net_mac() + 4),
            *(network_net_mac() + 5));
        FILE *fp = fopen(path, "rb"); 
        if (fp)
        {
            fread(_terminal, MAX_TERMINAL_ID, 1ul, fp);
            fclose(fp);

            _load_terminal = 1;
        }
    }

    return _terminal;
}

void terminal_save(const uint8_t *terminal)
{
    memcpy(_terminal, terminal, 4);

    _load_terminal = 1;

    char path[32] = {0};
    sprintf(path, "%02x%02x%02x%02x%02x%02x.terminal", 
        *(network_net_mac() + 0),
        *(network_net_mac() + 1),
        *(network_net_mac() + 2),
        *(network_net_mac() + 3),
        *(network_net_mac() + 4),
        *(network_net_mac() + 5));
    FILE *fp = fopen(path, "wb"); 
    if (!fp)
    {
        LogError("open %s failed.", path);
        return;
    }

    fwrite(_terminal, MAX_TERMINAL_ID, 1ul, fp);
    fclose(fp);
}

const uint8_t *cluster_id(void)
{
    if (!_load_cluster)
    {
        char path[32] = {0};
        sprintf(path, "%02x%02x%02x%02x%02x%02x.cluster", 
            *(network_net_mac() + 0),
            *(network_net_mac() + 1),
            *(network_net_mac() + 2),
            *(network_net_mac() + 3),
            *(network_net_mac() + 4),
            *(network_net_mac() + 5));
        FILE *fp = fopen(path, "rb"); 
        if (fp)
        {
            fread(_cluster, MAX_CLUSTER_ID, 1ul, fp);
            fread(_pin, MAX_CLUSTER_PIN, 1ul, fp);
            fclose(fp);

            _load_cluster = 1;
        }
    }
    return _cluster;
}

const uint8_t *cluster_pin(void)
{
    if (!_load_cluster)
    {
        char path[32] = {0};
        sprintf(path, "%02x%02x%02x%02x%02x%02x.cluster", 
            *(network_net_mac() + 0),
            *(network_net_mac() + 1),
            *(network_net_mac() + 2),
            *(network_net_mac() + 3),
            *(network_net_mac() + 4),
            *(network_net_mac() + 5));
        FILE *fp = fopen(path, "rb"); 
        if (fp)
        {
            fread(_cluster, MAX_CLUSTER_ID, 1ul, fp);
            fread(_pin, MAX_CLUSTER_PIN, 1ul, fp);
            fclose(fp);

            _load_cluster = 1;
        }
    }
    return _pin;
}

void cluster_save(const uint8_t *cluster, const uint8_t *pin)
{
    _load_cluster = 1;

    memcpy(_cluster, cluster, MAX_CLUSTER_ID);
    memcpy(_pin, pin, MAX_CLUSTER_PIN);
    
    char path[32] = {0};
    sprintf(path, "%02x%02x%02x%02x%02x%02x.cluster", 
        *(network_net_mac() + 0),
        *(network_net_mac() + 1),
        *(network_net_mac() + 2),
        *(network_net_mac() + 3),
        *(network_net_mac() + 4),
        *(network_net_mac() + 5));
    FILE *fp = fopen(path, "wb"); 
    if (!fp)
    {
        LogError("open %s failed.", path);
        return;
    }

    fwrite(_cluster, MAX_CLUSTER_ID, 1ul, fp);
    fwrite(_pin, MAX_CLUSTER_PIN, 1ul, fp);
    fclose(fp);
}

#endif
