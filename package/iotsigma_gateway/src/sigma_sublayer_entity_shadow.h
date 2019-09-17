#ifndef __SIGMA_SUBLAYER_ENTITY_SHADOW_H__
#define __SIGMA_SUBLAYER_ENTITY_SHADOW_H__

#include "sigma_layer_entity.h"

void sses_init(void);
void sses_update(void);

int sses_read(const SLEEntity *entity, uint8_t property, SLEValue *value, SigmaEntityListener resp, void *ctx);
void sses_iterator(const SigmaDomain *domain, uint8_t type, SigmaEntityListener resp, void *ctx);

#endif
