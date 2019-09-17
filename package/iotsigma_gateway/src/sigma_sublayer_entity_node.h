#ifndef __SIGMA_SUBLAYER_ENTITY_NODE_H__
#define __SIGMA_SUBLAYER_ENTITY_NODE_H__

#include "sigma_layer_entity.h"
#include "sigma_entity_instance.h"

void ssen_init(void);
void ssen_update(void);

SigmaEntityInstance *ssen_create(const SigmaDomain *domain);

#endif
