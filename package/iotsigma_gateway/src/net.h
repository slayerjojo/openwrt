#ifndef __NET_H__
#define __NET_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "array.h"
#include <arpa/inet.h>

#define NETIO_EPOLL

typedef struct _net_socket
{
    int fp;

    uint8_t alloc;

    void (*recv)(struct _net_socket *net);
    int (*send)(struct _net_socket *net);
    void (*exception)(struct _net_socket *net);
}NetSocket;

typedef struct _net_io
{
    void (*init)(void);
    void (*create)(NetSocket *net);
    void (*close)(NetSocket *net);
    void (*send)(NetSocket *net);

    int (*select)(int *socket, int **recvs, int **sends, int **exceptions);
}NetIO;

void net_init(NetIO *io);
void net_slice(void);

void net_add(NetSocket *socket);
void net_remove(NetSocket *socket);

void net_send(NetSocket *socket);

#ifdef __cplusplus
}
#endif

#endif
