#ifndef AB_HASH_H
#define AB_HASH_H

#include "ab/base.h"

uint32_t ab_crc32(AbSpan input);
bool ab_sha256(AbSpan input, uint8_t output[32], AbError *error);
void ab_digest_hex(const uint8_t digest[32], char output[65]);

#endif
