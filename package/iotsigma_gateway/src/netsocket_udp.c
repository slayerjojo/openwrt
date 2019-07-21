#include "netsocket_udp.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include "log.h"

typedef struct _udp_send_packet
{
    struct sockaddr_in addr;
    uint32_t size;
    uint8_t buffer[];
}UdpSendPacket;

static int udp_send(NetSocket *net)
{
    UdpSocket *udp = STRUCT_REFERENCE(UdpSocket, net, net);

    while (udp->packets.size)
    {
        UdpSendPacket *packet = (UdpSendPacket *)array_at(&udp->packets, 0);
        int ret = sendto(net->fp, packet->buffer, packet->size, 0, (const struct sockaddr *)&packet->addr, (socklen_t)sizeof(struct sockaddr_in));
        if (!ret)
        {
            LogAction("%d closed.", net->fp);
            udp_socket_close(udp);
            return 0;
        }
        if (ret < 0)
        {
            if (EAGAIN == errno || EWOULDBLOCK == errno)
                return 1;
            LogError("%d closed(%s).", net->fp, strerror(errno));
            udp_socket_close(udp);
            return 0;
        }
        free(packet);
        array_remove(&udp->packets, 0);
    }
    return udp->packets.size;
}

static void udp_recv(NetSocket *net)
{
    UdpSocket *udp = STRUCT_REFERENCE(UdpSocket, net, net);

    while (1)
    {
        static unsigned char buffer[1024 * 100] = {0};
        struct sockaddr_in sa;
        socklen_t len = sizeof(struct sockaddr_in);
        ssize_t ret = recvfrom(net->fp, buffer, 1024 * 100, MSG_DONTWAIT, (struct sockaddr *)&sa, &len);
        if (!ret)
        {
            LogAction("%d closed", net->fp);
            udp_socket_close(udp);
            return;
        }
        if (ret < 0)
        {
            if (EAGAIN == errno || EWOULDBLOCK == errno)
                return;
            LogError("%d closed(%s).", net->fp, strerror(errno));
            udp_socket_close(udp);
            return;
        }

        if (udp->recv)
            udp->recv(udp, buffer, ret, (struct sockaddr *)&sa);
    }
}

static void udp_exception(NetSocket *net)
{
    UdpSocket *udp = STRUCT_REFERENCE(UdpSocket, net, net);

    LogAction("%d exception", net->fp);
    udp_socket_close(udp);
}

UdpSocket *udp_server_create(UdpSocket *udp, const char *ip, uint16_t port, UdpRecvHandler receiver, UdpCloseHandler closer)
{
    int fp = socket(AF_INET, SOCK_DGRAM, 0);
    if (fp < 0)
    {
        LogError("failed(%s).", strerror(errno));
        return 0;
    }

    int sock_opts = 1;
    setsockopt(fp, SOL_SOCKET, SO_REUSEADDR, (void*)&sock_opts, sizeof(sock_opts));
    sock_opts = fcntl(fp, F_GETFL);
    sock_opts |= O_NONBLOCK;
    fcntl(fp, F_SETFL, sock_opts);

    struct sockaddr_in saLocal;
    saLocal.sin_family = AF_INET;
    saLocal.sin_port = htons(port);
    saLocal.sin_addr.s_addr = inet_addr(ip);

    if (bind(fp, (struct sockaddr *)&saLocal, sizeof(struct sockaddr_in)) < 0)
    {
        close(fp);
        return 0;
    }

    if (!udp)
    {
        udp = (UdpSocket *)calloc(1, sizeof(UdpSocket));
        if (!udp)
            LogHalt("out of memory");
        udp->net.alloc = 1;
    }
    else
    {
        udp->net.alloc = 0;
    }
    udp->net.fp = fp;
    array_init(&(udp->packets));
    udp->recv = receiver;
    udp->close = closer;
    udp->net.recv = udp_recv;
    udp->net.send = udp_send;
    udp->net.exception = udp_exception;
    net_add(&(udp->net));

    return udp;
}

UdpSocket *udp_client_create(UdpSocket *udp, UdpRecvHandler receiver, UdpCloseHandler closer)
{
    int fp = socket(AF_INET, SOCK_DGRAM, 0);
    if (fp < 0)
        return 0;

    int sock_opts = 1;
    setsockopt(fp, SOL_SOCKET, SO_REUSEADDR, (void*)&sock_opts, sizeof(sock_opts));
    sock_opts = fcntl(fp, F_GETFL);
    sock_opts |= O_NONBLOCK;
    fcntl(fp, F_SETFL, sock_opts);

    if (!udp)
    {
        udp = (UdpSocket *)calloc(1, sizeof(UdpSocket));
        if (!udp)
            LogHalt("out of memory");
        udp->net.alloc = 1;
    }
    else
    {
        udp->net.alloc = 0;
    }
    udp->net.fp = fp;
    array_init(&udp->packets);
    udp->recv = receiver;
    udp->close = closer;
    udp->net.recv = udp_recv;
    udp->net.send = udp_send;
    udp->net.exception = udp_exception;
    net_add(&(udp->net));

    return udp;
}

void udp_socket_close(UdpSocket *udp)
{
    net_remove(&(udp->net));
    close(udp->net.fp);

    ArrayIterator *it = array_iterator_create(&udp->packets, ARRAY_ORDER_SEQUENCE);
    while (array_iterator_next(it))
    {
        free(((UdpSendPacket *)array_iterator_value(it))->buffer);
    }
    array_iterator_release(it);
    array_clear(&(udp->packets));

    if (udp->close)
        udp->close(udp);
    if (udp->net.alloc)
        free(udp);
}

void udp_socket_send(UdpSocket *udp, struct sockaddr *to, uint8_t *buffer, uint32_t size)
{
    UdpSendPacket *packet = (UdpSendPacket *)malloc(sizeof(UdpSendPacket) + size);
    if (!packet)
        LogHalt("out of memory");
    packet->size = size;
    memcpy(&packet->addr, to, sizeof(struct sockaddr_in));
    memcpy(packet->buffer, buffer, size);
    array_push(&(udp->packets), packet);

    net_send(&(udp->net));
}

void udp_socket_transmit(UdpSocket *udp, const char *ip, uint16_t port, uint8_t *buffer, uint32_t size)
{
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(struct sockaddr_in));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    sa.sin_addr.s_addr = inet_addr(ip);

    udp_socket_send(udp, (struct sockaddr *)&sa, buffer, size);
}

void udp_multicast_ttl(UdpSocket *udp, uint8_t ttl)
{
    setsockopt(udp->net.fp, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(uint8_t));
}

void udp_multicast_loop(UdpSocket *udp, uint8_t loop)
{
    setsockopt(udp->net.fp, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(uint8_t));
}

void udp_multicast_if(UdpSocket *udp, struct in_addr *addr)
{
    setsockopt(udp->net.fp, IPPROTO_IP, IP_MULTICAST_IF, addr, sizeof(struct in_addr));
}

void udp_multicast_join(UdpSocket *udp, struct in_addr *addr, struct in_addr *interface)
{
    struct ip_mreq mreq;
    memcpy(&mreq.imr_multiaddr, addr, sizeof(struct in_addr));
    memcpy(&mreq.imr_interface, interface, sizeof(struct in_addr));
    
    setsockopt(udp->net.fp, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(struct ip_mreq));
}

void udp_multicast_leave(UdpSocket *udp, struct in_addr *addr, struct in_addr *interface)
{
    struct ip_mreq mreq;
    memcpy(&mreq.imr_multiaddr, addr, sizeof(struct in_addr));
    memcpy(&mreq.imr_interface, interface, sizeof(struct in_addr));
    
    setsockopt(udp->net.fp, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(struct ip_mreq));
}
