#ifndef __SIGMA_EVENT_H__
#define __SIGMA_EVENT_H__

#include "env.h"

enum
{
    EVENT_SIGMA_TERMINAL_REGISTER = 0,
    EVENT_SIGMA_CLOUD_STARTED,
    EVENT_SIGMA_LANWORK_STARTED,
};

typedef struct
{
    uint8_t event;
    uint32_t seq;
    uint32_t size;
    uint8_t payload[];
}SigmaEvent;

void *sigma_event_dispatch(uint8_t event, uint32_t size);
void *sigma_event_fetch(uint8_t *event);

#endif
