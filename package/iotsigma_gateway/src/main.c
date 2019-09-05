#define THIS_IS_STACK_APPLICATION

#include <signal.h>
#include <string.h>
#include "env.h"
#include "core.h"
#include "cluster.h"
#include "storage.h"
#include "entity_type.h"
#include "interface_usart.h"
#include "interface_io.h"
#include "interface_os.h"
#include "interface_flash.h"
#include "interface_network.h"
#include "driver_network_linux.h"
#include "driver_usart_linux.h"
#include "driver_flash_file.h"
#include "driver_led.h"
#include "driver_io_linux.h"
#include "buddha_heap.h"
#include "block.h"
#include "log.h"

#define MAX_HEAP_SIZE 10240

static uint8_t _heap[MAX_HEAP_SIZE] = {0};

static void sigroutine(int dunno)
{
    if (SIGUSR1 == dunno)
        cluster_declare(TERMINAL_TYPE_GATEWAY, 1);
}

void main(void)
{
    signal(SIGUSR1, sigroutine);

    buddha_heap_init(_heap, MAX_HEAP_SIZE);
    
    block_init(64);

    srand(time(0));
    
    io_init();
    
    driver_led_init();

    net_init(netio_create());
    
    network_init();

    flash_init("iotsigma.");

    console_init();
    
    usart_init();
    os_init();
    
    core_init();

    while (1) 
    {
        console_update();

        net_slice();

        driver_led_update();

        usart_update();

        core_update();
    }
}
