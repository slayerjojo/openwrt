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
        FILE *fp = fopen("iotsigma.terminal", "rb"); 
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

    FILE *fp = fopen("iotsigma.terminal", "wb"); 
    if (!fp)
    {
        LogError("open iotsigma failed.");
        return;
    }

    fwrite(_terminal, MAX_TERMINAL_ID, 1ul, fp);
    fclose(fp);
}

const uint8_t *cluster_id(void)
{
    if (!_load_cluster)
    {
        FILE *fp = fopen("iotsigma.cluster", "rb"); 
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
        FILE *fp = fopen("iotsigma.cluster", "rb"); 
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
    
    FILE *fp = fopen("iotsigma.cluster", "wb"); 
    if (!fp)
    {
        LogError("open iotsigma failed.");
        return;
    }

    fwrite(_cluster, MAX_CLUSTER_ID, 1ul, fp);
    fwrite(_pin, MAX_CLUSTER_PIN, 1ul, fp);
    fclose(fp);
}

#endif
