#ifndef __NETSOCKET_UDP_H__
#define __NETSOCKET_UDP_H__

#include "env.h"
#include <stdint.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "net.h"

struct _udp_socket;

typedef void (*UdpRecvHandler)(struct _udp_socket *udp, uint8_t *buffer, uint32_t size, struct sockaddr *from);
typedef void (*UdpCloseHandler)(struct _udp_socket *udp);

typedef struct _udp_socket
{
    NetSocket net;

    UdpRecvHandler recv;
    UdpCloseHandler close;

    Array packets;
}UdpSocket;

UdpSocket *udp_server_create(UdpSocket *udp, const char *ip, uint16_t port, UdpRecvHandler receiver, UdpCloseHandler closer);
UdpSocket *udp_client_create(UdpSocket *udp, UdpRecvHandler receiver, UdpCloseHandler closer);
void udp_socket_close(UdpSocket *udp);

void udp_socket_send(UdpSocket *udp, struct sockaddr *to, uint8_t *buffer, uint32_t size);
void udp_socket_transmit(UdpSocket *udp, const char *ip, uint16_t port, uint8_t *buffer, uint32_t size);

void udp_multicast_ttl(UdpSocket *udp, uint8_t ttl);
void udp_multicast_loop(UdpSocket *udp, uint8_t loop);
void udp_multicast_if(UdpSocket *udp, struct in_addr *addr);
void udp_multicast_join(UdpSocket *udp, struct in_addr *addr, struct in_addr *interface);
void udp_multicast_leave(UdpSocket *udp, struct in_addr *addr, struct in_addr *interface);

#endif
