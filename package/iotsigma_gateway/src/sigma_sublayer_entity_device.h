#ifndef __SIGMA_SUBLAYER_ENTITY_DEVICE_H__
#define __SIGMA_SUBLAYER_ENTITY_DEVICE_H__

#include "sigma_layer_entity.h"
#include "sigma_sublayer_entity_node.h"
#include "sigma_entity_instance.h"

void ssed_init(void);
void ssed_update(void);

SigmaEntityInstance *ssed_create(const SigmaDomain *domain, SigmaEntityInstance *node, uint16_t device);
SigmaEntityInstance *ssed_find(const SigmaDomain *domain, uint16_t node, uint16_t device);

#endif
