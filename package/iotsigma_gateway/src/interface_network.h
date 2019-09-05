#ifndef __INTERFACE_NETWORK_H__
#define __INTERFACE_NETWORK_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "env.h"
#include "endian.h"
#include "driver_network_linux.h"

#define NETWORK_MAC_LENGTH (6ul)

#define network_ntohll endian64
#define network_htonll endian64
#define network_ntohl ntohl
#define network_htonl htonl
#define network_ntohs ntohs
#define network_htons htons

#if defined(PLATFORM_LINUX)

#define network_init linux_network_init
#define network_tcp_server linux_network_tcp_server
#define network_tcp_accept linux_network_tcp_accept
#define network_tcp_client linux_network_tcp_client
#define network_tcp_recv linux_network_tcp_recv
#define network_tcp_send linux_network_tcp_send
#define network_tcp_close linux_network_close
#define network_udp_create linux_network_udp_create
#define network_udp_recv linux_network_udp_recv
#define network_udp_send linux_network_udp_send
#define network_udp_close linux_network_close
#define network_net_ip linux_network_ip
#define network_net_mask() linux_network_mask
#define network_net_dns_pri() "\0\0\0\0"
#define network_net_dns_sec() "\0\0\0\0"
#define network_net_mac linux_network_mac

#endif

#ifdef __cplusplus
}
#endif

#endif
