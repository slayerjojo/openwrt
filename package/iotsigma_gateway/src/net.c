#include "net.h"
#include <stdlib.h>
#include "map.h"

static Map *_nets = 0;
static NetIO *_io = 0;

static int compare(const void *v1, const void *v2)
{
    return (size_t)v1 - (size_t)v2;
}

void net_init(NetIO *io)
{
    _io = io;
    
    _nets = map_create(compare);

    if (_io && _io->init)
        _io->init();
}

void net_add(NetSocket *socket)
{
    map_add(_nets, (void *)socket->fp, socket);

    if (_io && _io->create)
        _io->create(socket);
}

void net_remove(NetSocket *socket)
{
    int64_t fp = socket->fp;
    map_remove(_nets, (const void **)&fp);

    if (_io && _io->close)
        _io->close(socket);
}

void net_send(NetSocket *socket)
{
    if (_io && _io->send)
        _io->send(socket);
}

void net_slice(void)
{
    if (_io && _io->select)
    {
        int *recvs = 0, *sends = 0, *exceptions = 0;
        int *socket = (int *)malloc(sizeof(int) * (_nets->size + 1));
        int count = 0;
        MapIterator *it = map_iterator_create(_nets);
        while (map_iterator_next(it))
        {
            *socket = (size_t)map_iterator_key(it);
        }
        map_iterator_release(it);

        int ret = _io->select(socket, &recvs, &sends, &exceptions);
        if (ret > 0)
        {
            int *psk = recvs;
            while (*psk)
            {
                NetSocket *net = (NetSocket *)map_get(_nets, (const void *)*psk);
                if (net && net->recv)
                    net->recv(net);
                psk++;
            }
            psk = sends;
            while (*psk)
            {
                NetSocket *net = (NetSocket *)map_get(_nets, (const void *)*psk);
                if (net && net->send && net->send(net))
                    net_send(net);
                psk++;
            }
            psk = exceptions;
            while (*psk)
            {
                NetSocket *net = (NetSocket *)map_get(_nets, (const void *)*psk);
                if (net && net->exception)
                    net->exception(net);
                psk++;
            }
        }
        free(socket);
        free(recvs);
        free(sends);
        free(exceptions);
    }
}
