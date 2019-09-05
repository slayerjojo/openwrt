#include "sigma_event.h"

static uint32_t _sequence = 1;

void *sigma_event_dispatch(uint8_t event, uint32_t size)
{
    SigmaEvent *e = (SigmaEvent *)buddha_heap_talloc(sizeof(SigmaEvent) + size, HEAP_MARK_EVENT);
    if (!e)
        return 0;
    e->size = size;
    e->seq = _sequence++;
    e->event = event;
    return e + 1;
}

void *sigma_event_fetch(uint8_t *event, uint32_t *seq)
{
    SigmaEvent *e = 0, *min = 0;
    while ((e = (SigmaEvent *)buddha_heap_find(e, HEAP_MARK_EVENT)))
    {
        if (event && (*event != e->event))
            continue;
        if (seq && (e->seq <= *seq))
            continue;
        if (!min)
            min = e;
        if (e->seq < min->seq)
            min = e;
    }
    if (min)
    {
        if (event)
            *event = min->event;
        if (seq)
            *seq = min->seq;
        return e->payload;
    }
    return 0;
}
