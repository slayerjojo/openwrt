#ifndef __SIGMA_LAYER_DEVICE_H__
#define __SIGMA_LAYER_DEVICE_H__

#include "sigma_layer_entity.h"

#define MAX_LINK_ID (MAX_TERMINAL_ID + 2)

void sld_init(void);
void sld_update(void);

int sld_node_create(SLEEntity *node);
int sld_device_create(SLEEntity *node, SLEEntity *device);

int sld_value_update(const SLEEntity *device, const SLEValue *value);

int sld_link(const SLEEntity *device, const uint8_t *link, );

#endif
