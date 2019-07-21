#ifndef __STORAGE_H__
#define __STORAGE_H__

#include "env.h"

uint8_t dhcp_enabled(void);

const uint8_t *terminal_id(void);
void terminal_save(const uint8_t *terminal);

const uint8_t *cluster_id(void);
const uint8_t *cluster_pin(void);
void cluster_save(const uint8_t *cluster, const uint8_t *pin);

#endif
