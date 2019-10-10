#include "sigma_mission.h"
#include "interface_os.h"

typedef int (*SigmaMissionIterator)(void *ctx, SigmaMission **from, SigmaMission *prev, SigmaMission *mission);

static SigmaMission *_missions = 0;

void sigma_mission_init(void)
{
    _missions = 0;
}

void sigma_mission_update(void)
{
    SigmaMission *m = _missions, *prev = 0;
    while (m)
    {
        if (!m->deps)
            m->done = 1;
        else if (m->deps->handler && m->deps->handler(m, m->deps->ctx))
            m->done = 1;
        if (m->done)
        {
            SigmaMissionDepends *deps = m->deps;
            if (deps)
            {
                m->deps = m->deps->next;

                os_free(deps);
            }
            if (!m->deps)
            {
                if (prev)
                    prev->next = m->next;
                else
                    _missions = m->next;
                os_free(m);
                break;
            }
        }

        prev = m;
        m = m->next;
    }
}

SigmaMission *sigma_mission_create(SigmaMissionHandler handler, void *ctx, size_t extends)
{
    SigmaMission *m = (SigmaMission *)os_malloc(sizeof(SigmaMission) + extends);
    if (!m)
        return 0;
    os_memset(m, 0, sizeof(SigmaMission));
    m->done = 0;

    m->next = _missions;
    _missions = m;

    sigma_mission_depends(m, handler, ctx);
    return m;
}

void sigma_mission_release(SigmaMission *mission)
{
    SigmaMission *m = _missions, *prev = 0;
    while (m)
    {
        if (m == mission)
            break;
        prev = m;
        m = m->next;
    }
    if (!m)
        return;

    if (prev)
        prev->next = mission->next;
    else
        _missions = mission->next;

    while (mission->deps)
    {
        SigmaMissionDepends *deps = mission->deps;
        mission->deps = mission->deps->next;

        os_free(deps);
    }
    os_free(mission);
}

void sigma_mission_done(SigmaMission *mission)
{
    mission->done = 1;
}

void *sigma_mission_extends(SigmaMission *mission)
{
    return mission->extends;
}

int sigma_mission_depends(SigmaMission *mission, SigmaMissionHandler handler, void *ctx)
{
    SigmaMissionDepends *dep = (SigmaMissionDepends *)os_malloc(sizeof(SigmaMissionDepends));
    if (!dep)
    {
        LogHalt("out of memory!");
        return -1;
    }
    os_memset(dep, 0, sizeof(SigmaMissionDepends));
    dep->handler = handler;
    dep->ctx = ctx;
    dep->next = 0;

    SigmaMissionDepends *last = mission->deps;
    while (last && last->next)
        last = last->next;
    if (last)
        last->next = dep;
    else
        mission->deps = last;
    return 0;
}
