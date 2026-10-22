#ifndef AB_TARGET_RESOURCES_H
#define AB_TARGET_RESOURCES_H

#include "ab/zip.h"

/* Obtain the resource key from data operands in the pinned APK's ARMv7 library.
 * The library is read and verified as input data; no native code is executed or
 * linked. Output is caller-owned and unchanged on failure. No key bytes are
 * embedded in project source or written to a file. allocation_limit bounds the
 * temporary uncompressed library, which is released before return. */
bool ab_target_resource_key(const AbZip *archive, size_t allocation_limit,
                            uint8_t output[32], AbError *error);
/* Corresponding saved-table key, from getVariableKey's verified data operands.
 * The same pinned-library, ownership and no-embedded-key rules apply. */
bool ab_target_save_key(const AbZip *archive, size_t allocation_limit,
                        uint8_t output[32], AbError *error);

/* Read an encrypted, compressed resource such as level bytecode or config JSON.
 * The key borrows 32 bytes. allocation_limit bounds the combined encrypted,
 * decrypted and unpacked payload buffers. Decoders use their own bounded
 * workspace. Output starts empty and is owned by the caller on success. */
bool ab_target_packed_read(const AbZip *archive, const char *path, AbSpan key,
                           size_t allocation_limit, AbBuffer *output, AbError *error);

#endif
