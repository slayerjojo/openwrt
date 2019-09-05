#ifndef __DRIVER_LINUX_H__
#define __DRIVER_LINUX_H__

#include "env.h"
#include "interface_network.h"
#include "interface_os.h"

#ifdef __cplusplus
extern "C"
{
#endif

#if defined(PLATFORM_LINUX)

uint32_t linux_ticks(void);
uint32_t linux_ticks_from(uint32_t start);

const uint8_t *linux_crypto_auth(void);
const uint8_t *linux_crypto_key_public(void);
const uint8_t *linux_crypto_key_shared(uint8_t *shared, const uint8_t *key);
int linux_crypto_encrypto(void *out, const void *in, uint32_t size, const uint8_t *shared);
int linux_crypto_decrypto(void *out, const void *in, uint32_t size, const uint8_t *shared);

#endif

#ifdef __cplusplus
}
#endif

#endif
