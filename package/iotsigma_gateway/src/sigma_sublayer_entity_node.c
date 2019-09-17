#include "sigma_sublayer_entity_node.h"
#include "sigma_layer_entity.h"
#include "entity_type.h"
#include "entity.h"

#define FSM_SSEN_CREATE_INIT 0

void ssen_init(void)
{
}

void ssen_update(void)
{
}

static int ssen_create_handler(SigmaMission *mission, void *ctx)
{
    return !!((SigmaEntityInstance *)ctx)->entity;
}

static int ssen_create_response(void *ctx, int event, const SLEEntity *entity, uint8_t property, const SLEValue *value)
{
    UNUSED(property);
    UNUSED(value);

    SigmaEntityInstance *inst = (SigmaEntityInstance *)ctx;

    if (event == EVENT_ENTITY_RESULT)
    {
        inst->entity = entity->entity;
        return 1;
    }
    return 0;
}

SigmaEntityInstance *ssen_create(const SigmaDomain *domain)
{
    SigmaEntityInstance *inst = sei_create(domain);
    if (!inst)
        return 0;
    inst->mission = sigma_mission_create(ssen_create_handler, inst);
    if (!inst->mission)
    {
        sei_release(inst);
        return 0;
    }
    sle_create(domain, ENTITY_TYPE_NODE, 0, 0, ssen_create_response, inst);
    return inst;
}
