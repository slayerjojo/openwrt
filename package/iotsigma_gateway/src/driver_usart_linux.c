#include "driver_usart_linux.h"

#if defined(PLATFORM_LINUX)

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include "interface_os.h"
#include "map.h"
#include "log.h"

//https://www.ibm.com/developerworks/cn/linux/l-serials/index.html

typedef struct 
{
    UsartHandler handler;
    uint8_t buffer[512];
    uint16_t pos;
    int fp;
    uint8_t usart;
}UsartRuntime;

static Map *_usart_runtimes = 0;

void linux_usart_init(void)
{
    _usart_runtimes = map_create(0);
}

void linux_usart_update(void)
{
    MapIterator *it = map_iterator_create(_usart_runtimes);
    while (map_iterator_next(it))
    {
        int fp = (int)map_iterator_key(it);
        UsartRuntime *runtime = (UsartRuntime *)map_iterator_value(it);

        int ret = read(fp, runtime->buffer + runtime->pos, 512 - runtime->pos);
        if (ret > 0 && runtime->handler)
        {
            LogAction("pos:%d ret:%d", runtime->pos, ret);
            runtime->pos += ret;
            ret = runtime->handler(runtime->buffer, runtime->pos);
            if (ret)
            {
                runtime->pos -= ret;
                if (runtime->pos)
                    memmove(runtime->buffer, runtime->buffer + ret, runtime->pos);
            }
        }
    }
    map_iterator_release(it);
}

int linux_usart_open(uint8_t usart, uint32_t baud, uint32_t databit, char parity, uint8_t stopbits, UsartHandler handler)
{
    int fp = -1;
    if (0 == usart)
    {
        fp = open("/dev/ttyS1", O_RDWR);
    }
    if (-1 == fp)
    {
        LogError("serial open error");
        return -1;
    }

    struct termios opt;
    if (tcgetattr(fp, &opt))
    {
        LogError("tcgetattr error");
        return -1;
    }
    if (115200 == baud)
    {
        cfsetispeed(&opt, B115200);
        cfsetospeed(&opt, B115200);
    }
    else
    {
        LogError("baud %u not support", baud);
        return -1;
    }
    tcflush(fp, TCIOFLUSH);

    opt.c_cflag &= ~CSIZE; 
    switch (databit)
    {   
        case 7:
            opt.c_cflag |= CS7; 
            break;
        case 8:
            opt.c_cflag |= CS8;
            break;   
        default:    
            LogError("databits %d not support", databit);
            return -1;  
    }
    switch (parity) 
    {   
        case 'n':
        case 'N':    
            opt.c_cflag &= ~PARENB;   /* Clear parity enable */
            opt.c_iflag &= ~INPCK;     /* Enable parity checking */ 
            break;  
        case 'o':   
        case 'O':     
            opt.c_cflag |= (PARODD | PARENB); /* 设置为奇效验*/  
            opt.c_iflag |= INPCK;             /* Disnable parity checking */ 
            break;  
        case 'e':  
        case 'E':   
            opt.c_cflag |= PARENB;     /* Enable parity */    
            opt.c_cflag &= ~PARODD;   /* 转换为偶效验*/     
            opt.c_iflag |= INPCK;       /* Disnable parity checking */
            break;
        case 'S': 
        case 's':  /*as no parity*/   
            opt.c_cflag &= ~PARENB;
            opt.c_cflag &= ~CSTOPB;
            break;  
        default:   
            LogError("parity '%d' Unsupported", parity);    
            return -1;  
    }
    /* 设置停止位*/  
    switch (stopbits)
    {   
        case 1:    
            opt.c_cflag &= ~CSTOPB;  
            break;  
        case 2:    
            opt.c_cflag |= CSTOPB;  
            break;
        default:    
            LogError("stopbits %d Unsupported", stopbits);  
            return -1; 
    }
    tcflush(fp, TCIFLUSH);
    opt.c_cc[VTIME] = 0; /* 设置超时15 seconds*/   
    opt.c_cc[VMIN] = 0; /* Update the opt and do it NOW */

    opt.c_cflag |= (CLOCAL | CREAD);
    opt.c_lflag &= ~(ECHO|ECHONL|ICANON|ISIG|IEXTEN);  /*Input*/
    opt.c_oflag &= ~OPOST;   /*Output*/
    opt.c_oflag &= ~(ONLCR | OCRNL); 
    opt.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|INLCR|IGNCR|ICRNL|IXON);
    if (tcsetattr(fp, TCSANOW, &opt))
    {
        LogError("tcsetattr error");
        return -1;
    }

    UsartRuntime *runtime = (UsartRuntime *)calloc(sizeof(UsartRuntime), 1);
    if (!runtime)
    {
        LogError("out of memory");
        return -1;
    }
    runtime->handler = handler;
    runtime->pos = 0;
    runtime->fp = fp;
    runtime->usart = usart;

    map_add(_usart_runtimes, (const void *)&(runtime->usart), runtime);

    int flags = fcntl(fp, F_GETFL);
    flags |= O_NONBLOCK;
    fcntl(fp, F_SETFL, flags);

    LogAction("open usart (baud:%u databit:%u) successed.", baud, databit);

    return 1;
}

void linux_usart_close(uint8_t usart)
{
    UsartRuntime *runtime = (UsartRuntime *)map_remove(_usart_runtimes, (void *)&usart);
    if (runtime)
    {
        close(runtime->fp);

        free(runtime);
    }
}

int linux_usart_write(uint8_t usart, const uint8_t *buffer, uint16_t size)
{
    UsartRuntime *runtime = (UsartRuntime *)map_remove(_usart_runtimes, (void *)&usart);
    if (!runtime)
        return 0;
    return write(runtime->fp, buffer, size);
}

#endif
