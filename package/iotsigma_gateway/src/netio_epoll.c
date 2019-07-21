#include "netio_epoll.h"
#ifdef NETIO_EPOLL
#include <sys/epoll.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include "log.h"

int _epoll = 0;

static void netio_event_init(void)
{
    if(!_epoll)
        _epoll  = epoll_create(1024);
}

static void netio_event_create(NetSocket *net)
{
    struct epoll_event ev = {0};
    ev.data.fd  = net->fp;
    ev.events   = EPOLLIN | EPOLLET;
    epoll_ctl(_epoll, EPOLL_CTL_ADD, net->fp, &ev);
}

static void netio_event_close(NetSocket *net)
{
    struct epoll_event ev = {0};
    ev.data.fd  = net->fp;
    ev.events   = EPOLLIN | EPOLLET | EPOLLOUT;
    epoll_ctl(_epoll, EPOLL_CTL_DEL, net->fp, &ev);
}

static void netio_event_send(NetSocket *net)
{
    struct epoll_event ev = {0};
    ev.data.fd  = net->fp;
    ev.events   = EPOLLIN | EPOLLET | EPOLLOUT;
    epoll_ctl(_epoll, EPOLL_CTL_MOD, net->fp, &ev);
}

static int netio_selector(int *socket, int **recvs, int **sends, int **exceptions)
{
    static struct epoll_event events[1024];
    int ret = epoll_wait(_epoll, events, 1024, 10);
    if (ret < 0)
    {
        LogError("error(%s).", strerror(errno));
        return 0;
    }
    if (!ret)
        return 0;
    int *pr = (int *)malloc(sizeof(int) * (ret + 1));
    if (!pr)
        LogHalt("out of memory!");
    int *ps = (int *)malloc(sizeof(int) * (ret + 1));
    if (!ps)
        LogHalt("out of memory!");
    int *pe = (int *)malloc(sizeof(int) * (ret + 1));
    if (!pe)
        LogHalt("out of memory!");

    *recvs = pr;
    *sends = ps;
    *exceptions = pe;

    struct epoll_event *ev  = events;
    while (ret--)
    {
        if (ev->data.fd < 0)
        {
            ev++;
            continue;
        }
        if (ev->events & EPOLLERR || ev->events & EPOLLHUP)
        {
            *pe = ev->data.fd;
            pe++;
        }
        else
        {
            if (ev->events & EPOLLIN)
            {
                *pr = ev->data.fd;
                pr++;
            }
            if (ev->events & EPOLLOUT)
            {
                *ps = ev->data.fd;
                ps++;

                struct epoll_event evs = {0};
                evs.data.fd  = ev->data.fd;
                evs.events   = EPOLLIN | EPOLLET;
                epoll_ctl(_epoll, EPOLL_CTL_MOD, ev->data.fd, &evs);
            }
        }
        ev++;
    }
    *ps = *pr = *pe = 0;
    return 1;
}

NetIO *netio_create(void)
{
    NetIO *model = (NetIO *)malloc(sizeof(NetIO));
    if (!model)
        LogHalt("out of memory");
    model->init = netio_event_init;
    model->create = netio_event_create;
    model->close = netio_event_close;
    model->send = netio_event_send;
    model->select = netio_selector;
    return model;
}

#endif
