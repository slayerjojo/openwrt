#include "driver_led.h"
#include "interface_os.h"
#include "interface_io.h"
#include "driver_linux.h"

static uint32_t _timer = 0;
static struct
{
    uint8_t pulse;
    uint8_t counter;
} _settings[] = {
    {0, 0x10},
    {0, 0x10}
};

void driver_led_init(void)
{
    /*
    io_direction_set(LED_NET_RED, 0);
    io_direction_set(LED_APP, 0);
    */

    _timer = 1;
}

static void onLed(uint8_t led, uint8_t v)
{
    /*
    if (led == LED_ID_NET_RED)
        io_set(LED_NET_RED, !v);
    else if (led == LED_ID_APP)
        io_set(LED_APP, !v);
        */
}

void driver_led_update(void)
{
    if (os_ticks_from(_timer) > os_ticks_ms(100))
    {
	    uint8_t led = 0;

        _timer = os_ticks();

        for (led = 0; led < (sizeof(_settings)/sizeof(_settings[0])); led++)
        {
            uint8_t repeat = (_settings[led].counter & 0xf0) >> 4;
            uint8_t counter = _settings[led].counter & 0x0f;
            if (repeat)
            {
                onLed(led, _settings[led].pulse & (0x01 << counter));

                counter++;
                if (counter >= 8)
                {
                    counter = 0;
                    if (repeat != 0x0f)
                        repeat--;
                }
                _settings[led].counter = (repeat << 4) | counter;
            }
        }
    }
}

void led_set(uint8_t led, uint8_t pulse, uint8_t repeat)
{
    if (repeat > 0x0f)
        repeat = 0x0f;
    if (0 == repeat)
        repeat = 1;
    _settings[led].pulse = pulse;
    _settings[led].counter = repeat << 4;
    onLed(led, _settings[led].pulse & 0x01);
}
