#ifndef __SIGMA_ENTITY_INSTANCE_H__
#define __SIGMA_ENTITY_INSTANCE_H__

#include "sigma_layer_entity.h"
#include "sigma_mission.h"
#include "fsm.h"

typedef struct
{
    SigmaDomain domain;
    uint16_t entity;

    SigmaMission *mission;
}SigmaEntityInstance;

void sei_set(SigmaMission *instance, uint8_t property, const SLEValue *value);

#endif
