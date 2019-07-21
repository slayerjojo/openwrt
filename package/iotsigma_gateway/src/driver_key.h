#ifndef __DRIVER_IMPL_KEY_H__
#define __DRIVER_IMPL_KEY_H__

#include "env.h"

typedef void (*KeyHandler)(uint8_t key, uint8_t press);

void driver_key_init(KeyHandler handler);
void driver_key_update(void);

#endif
