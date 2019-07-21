#ifndef __INTERFACE_USART_H__
#define __INTERFACE_USART_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "env.h"

#if defined(PLATFORM_LINUX)

typedef uint16_t (*UsartHandler)(uint8_t *buffer, uint16_t size);

#define usart_init linux_usart_init
#define usart_update linux_usart_update
#define usart_open linux_usart_open
#define usart_close linux_usart_close
#define usart_write linux_usart_write

#endif

#ifdef __cplusplus
}
#endif

#endif
