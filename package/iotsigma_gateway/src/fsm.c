#include "fsm.h"

void fsm_init(FSMState *fsm, uint8_t state)
{
    fsm->state = state;
    fsm->timer = os_ticks();
}

void fsm_update(FSMState *fsm, uint8_t state)
{
    fsm->state = state;
    fsm->timer = os_ticks();
}

uint8_t fsm_state(FSMState *fsm)
{
    return fsm->state;
}

uint32_t fsm_time(FSMState *fsm)
{
    return os_ticks_from(fsm->timer);
}
