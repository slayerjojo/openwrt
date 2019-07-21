#ifndef __AES_H__
#define __AES_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "env.h"

int AESEncrypt(const uint8_t *in, uint8_t *out, int len, const uint8_t *key, int keylen);
int AESDecrypt(const uint8_t *in, uint8_t *out, int len, const uint8_t *key, int keylen);

int AES128CBCEncrypt(const uint8_t *in, uint8_t *out, int len, const uint8_t*key, const uint8_t *iv);
int AES128CBCDecrypt(const uint8_t *in, uint8_t *out, int len, const uint8_t *key, const uint8_t *iv);

#ifdef __cplusplus
}
#endif

#endif // AES_H
