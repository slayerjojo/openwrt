#ifndef __MISSION_H__
#define __MISSION_H__

#include "env.h"

void mission_init(void);
void mission_update(void);

void build_mission_device_simple_desc(uint32_t *addr);
void build_mission_sync_device_value(uint16_t device);
void build_mission_link_execute(const uint8_t *link, uint16_t source, const uint8_t *command, uint16_t size);
void build_mission_master_manual(uint16_t entity, uint8_t command, const uint8_t *link);
void build_mission_master_status(uint32_t addr);
void build_mission_device_related(uint16_t device);
void build_mission_terminal_update(void);
void build_mission_entity_update(const uint8_t *cluster, const uint8_t *terminal, uint8_t session, uint32_t addr, uint8_t entity);
void build_mission_entity_dirty(uint32_t *addr);
void build_mission_slave_apply(uint16_t entity);

#endif
