#ifndef __DRIVER_NETWORK_LINUX_H__
#define __DRIVER_NETWORK_LINUX_H__

#include "env.h"

#ifdef __cplusplus
extern "C"
{
#endif

#if defined(PLATFORM_OPENWRT) || defined(PLATFORM_LINUX)

void linux_network_init(void);

int linux_network_tcp_server(uint16_t port);
int linux_network_tcp_accept(int fp, uint8_t *ip, uint16_t *port);
int linux_network_tcp_client(const char *host, uint16_t port);

int linux_network_tcp_recv(int fp, void *buffer, uint32_t size);
int linux_network_tcp_send(int fp, const void *buffer, uint32_t size);

int linux_network_udp_create(uint16_t port);
int linux_network_udp_recv(int fp, void *buffer, uint32_t size, uint8_t *ip, uint16_t *port);
int linux_network_udp_send(int fp, const void *buffer, uint32_t size, const uint8_t *ip, uint16_t port);

int linux_network_close(int fp);

const uint8_t *linux_network_ip(void);
const uint8_t *linux_network_mask(void);
const uint8_t *linux_network_mac(void);

#endif

#ifdef __cplusplus
}
#endif

#endif
