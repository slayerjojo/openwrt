#include "driver_key.h"
#include "interface_os.h"
#include "interface_io.h"
#include "driver_linux.h"
#include "log.h"

static uint32_t _timer = 0;
static struct
{
    uint16_t counter;
} _settings[] = {
    {0}
};
static KeyHandler _handler = 0;

void driver_key_init(KeyHandler handler)
{
    //io_direction_set(BUTTON, 1);

    _handler = handler;

    _timer = os_ticks();
}

static int onKey(uint8_t key)
{
    /*
    if (0 == key)
        return !io_get(BUTTON);
        */
    return 0;
}

void driver_key_update(void)
{
	uint8_t key = 0;

    if (os_ticks_from(_timer) > os_ticks_ms(10))
    {
        _timer = os_ticks();

        for (key = 0; key < (sizeof(_settings)/sizeof(_settings[0])); key++)
        {
            if (onKey(key))
            {
                if (_settings[key].counter < 300)
                {
                    _settings[key].counter++;
                    if (_settings[key].counter >= 300)
                    {
                        if (_handler)
                            _handler(key, 1);
                    }
                }
            }
            else
            {
                if ((5 < _settings[key].counter) && (_settings[key].counter < 300))
                {
                    if (_handler)
                        _handler(key, 0);
                }
                _settings[key].counter = 0;
            }
        }
    }
}
