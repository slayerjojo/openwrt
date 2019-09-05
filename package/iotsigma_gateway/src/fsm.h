#ifndef __FSM_H__
#define __FSM_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "env.h"

typedef struct
{
    uint8_t state;
    uint32_t timer;
}FSMState;

void fsm_init(FSMState *fsm, uint8_t state);
void fsm_update(FSMState *fsm, uint8_t state);
uint8_t fsm_state(FSMState *fsm);
uint32_t fsm_time(FSMState *fsm);

#ifdef __cplusplus
}
#endif

#endif
