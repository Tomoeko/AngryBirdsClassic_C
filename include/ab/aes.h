#ifndef AB_AES_H
#define AB_AES_H

#include "ab/base.h"

/* AES-CBC with an explicit IV and no padding policy. Exact in-place use is valid. */
bool ab_aes_cbc_decrypt(AbSpan ciphertext, AbSpan key, const uint8_t iv[16],
                        uint8_t *output, AbError *error);
bool ab_aes_cbc_encrypt(AbSpan plaintext, AbSpan key, const uint8_t iv[16],
                        uint8_t *output, AbError *error);

/* Target util::AES padding policy, including its accepted zero padding count. */
bool ab_asset_decrypt(AbSpan ciphertext, AbSpan key, AbBuffer *output, AbError *error);
/* Target default encryption: zero IV and a full PKCS#7 padding block when aligned. */
bool ab_asset_encrypt(AbSpan plaintext, AbSpan key, AbBuffer *output, AbError *error);

#endif
