#ifndef __SIGMA_MISSION_H__
#define __SIGMA_MISSION_H__

#include "env.h"

struct _sigma_mission;

typedef int (*SigmaMissionHandler)(struct _sigma_mission *mission, void *ctx);

typedef struct _sigma_mission_depends
{
    struct _sigma_mission_depends *next;

    SigmaMissionHandler handler;
    void *ctx;
}SigmaMissionDepends;

typedef struct _sigma_mission
{
    struct _sigma_mission *next;

    SigmaMissionDepends *deps;

    uint8_t done:1;

    uint8_t extends[];
}SigmaMission;

void sigma_mission_init(void);
void sigma_mission_update(void);

SigmaMission *sigma_mission_create(SigmaMissionHandler handler, void *ctx, size_t extends);
void sigma_mission_release(SigmaMission *mission);

void sigma_mission_done(SigmaMission *mission);

void *sigma_mission_extends(SigmaMission *mission);

int sigma_mission_depends(SigmaMission *mission, SigmaMissionHandler handler, void *ctx);

#endif
