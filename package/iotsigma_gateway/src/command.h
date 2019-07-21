#ifndef __COMMAND_H__
#define __COMMAND_H__

#include "env.h"

void command_execute(uint16_t entity, uint8_t type, const uint8_t *command, uint16_t size, const uint8_t *link);

#endif
