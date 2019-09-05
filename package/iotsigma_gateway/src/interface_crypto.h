#ifndef __INTERFACE_CRYPTO_H__
#define __INTERFACE_CRYPTO_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "env.h"

#if defined(PLATFORM_LINUX)

#include "driver_linux.h"

#define MAX_CRYPTO_PUBLIC (16 * 2)
#define MAX_CRYPTO_PIN (16)

#define crypto_key_public linux_crypto_key_public 
#define crypto_key_shared linux_crypto_key_shared
#define crypto_encrypto linux_crypto_encrypto
#define crypto_decrypto linux_crypto_decrypto
#define crypto_auth linux_crypto_auth

#endif

#ifdef __cplusplus
}
#endif

#endif
