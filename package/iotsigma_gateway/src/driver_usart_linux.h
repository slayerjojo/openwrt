#ifndef __DRIVER_USART_LINUX_H__
#define __DRIVER_USART_LINUX_H__

#include "env.h"
#include "interface_usart.h"

#if defined(PLATFORM_LINUX)

void linux_usart_init(void);
void linux_usart_update(void);

int linux_usart_open(uint8_t usart, uint32_t baud, uint32_t databit, char parity, uint8_t stopbits, UsartHandler handler);
void linux_usart_close(uint8_t usart);
int linux_usart_write(uint8_t usart, const uint8_t *buffer, uint16_t size);

#endif

#endif
