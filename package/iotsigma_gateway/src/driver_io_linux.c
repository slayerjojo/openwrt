#include "driver_io_linux.h"
#include <unistd.h>
#include <fcntl.h>
#include "log.h"

#if defined(PLATFORM_OPENWRT)

struct {
    const char *io;
    const char *direction;
    const char *value;
} _gpios[] = {
    {
        "42",
        "out",
        "0"
    },
    {
        "44",
        "out",
        "0"
    },
    {
        "41",
        "out",
        "0"
    }
};

void linux_io_init(void)
{
    int io = 0;
    int fp = -1;

    for (io = 0; io < sizeof(_gpios) / sizeof(_gpios[0]); io++)
    {
        char path[64] = {0};
        
        sprintf(path, "/sys/class/gpio/gpio%s/direction", _gpios[io].io);
        fp = open(path, O_WRONLY);
        if (fp >= 0)
        {
            LogAction("gpio%s opened already.", _gpios[io].io);
            continue;
        }
        close(fp);

        fp = open("/sys/class/gpio/export", O_WRONLY);
        if (fp < 0)
        {
            LogError("gpio%s open failed!", _gpios[io].io);
            close(fp);
            continue;
        }
        write(fp, _gpios[io].io, 2);
        close(fp);

        sprintf(path, "/sys/class/gpio/gpio%s/direction", _gpios[io].io);
        fp = open(path, O_WRONLY);
        if (fp < 0)
        {
            LogError("gpio%s open direction failed!", _gpios[io].io);
            close(fp);
            continue;
        }
        write(fp, _gpios[io].direction, strlen(_gpios[io].direction));
        close(fp);

        if (!strcmp(_gpios[io].direction, "out"))
        {
            sprintf(path, "/sys/class/gpio/gpio%s/value", _gpios[io].io);
            fp = open(path, O_RDWR);
            if (fp < 0)
            {
                LogError("gpio%s open value failed.", _gpios[io].io);
                close(fp);
                continue;
            }
            write(fp, _gpios[io].value, strlen(_gpios[io].value));
            close(fp);
        }
        LogError("gpio%s open value failed.", _gpios[io].io);
    }
}

uint8_t linux_io_direction_get(uint8_t io)
{
    int fp = -1;
    
    char path[64] = {0};
        
    sprintf(path, "/sys/class/gpio/gpio%s/direction", _gpios[io].io);
    fp = open(path, O_RDONLY);
    if (fp < 0)
    {
        LogError("gpio%s open direction failed!", _gpios[io].io);
        close(fp);
        return 0;
    }
    read(fp, path, 64);
    close(fp);

    return !!strcmp(path, "out");
}

void linux_io_direction_set(uint8_t io, uint8_t v)
{
    int fp = -1;
    
    char path[64] = {0};
        
    sprintf(path, "/sys/class/gpio/gpio%s/direction", _gpios[io].io);
    fp = open(path, O_WRONLY);
    if (fp < 0)
    {
        LogError("gpio%s open direction failed!", _gpios[io].io);
        close(fp);
        return;
    }
    write(fp, v ? "in" : "out", v ? 2 : 3);
    close(fp);
}

uint8_t linux_io_get(uint8_t io)
{
    int fp = -1;
    
    char path[64] = {0};
        
    sprintf(path, "/sys/class/gpio/gpio%s/value", _gpios[io].io);
    fp = open(path, O_RDONLY);
    if (fp < 0)
    {
        LogError("gpio%s open value failed!", _gpios[io].io);
        close(fp);
        return 0;
    }
    read(fp, path, 64);
    close(fp);

    return !!strcmp(path, "0");
}

void linux_io_set(uint8_t io, uint8_t v)
{
    int fp = -1;
    
    char path[64] = {0};
        
    sprintf(path, "/sys/class/gpio/gpio%s/value", _gpios[io].io);
    fp = open(path, O_WRONLY);
    if (fp < 0)
    {
        LogError("gpio%s open value failed!", _gpios[io].io);
        close(fp);
        return;
    }
    write(fp, v ? "1" : "0", 1);
    close(fp);
}

#endif
