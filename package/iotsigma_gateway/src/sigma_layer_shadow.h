#ifndef __SIGMA_LAYER_SHADOW_H__
#define __SIGMA_LAYER_SHADOW_H__

#include "sigma_layer_entity.h"

void sls_init(void);
void sls_update(void);

int sls_read(const SLEEntity *entity, uint8_t property, SLEValue *value, SigmaEntityListener resp, void *ctx);
void sls_iterator(const SigmaDomain *domain, uint8_t type, SigmaEntityListener resp, void *ctx);

#endif
