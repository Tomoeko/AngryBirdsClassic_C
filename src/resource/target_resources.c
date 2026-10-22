#include "ab/target_resources.h"
#include "ab/hash.h"
#include "ab/aes.h"
#include "ab/pack.h"

#include <string.h>

static const uint32_t key_fields[32] = {
    0x2c5ebc, 0x2c5ee4, 0x2c5ee8, 0x2c5eec, 0x2c5ec0, 0x2c5ec4, 0x2c5ecc, 0x2c5f38,
    0x2c5ed4, 0x2c5ef4, 0x2c5ee4, 0x2c5efc, 0x2c5f04, 0x2c5f78, 0x2c5f88, 0x2c5f08,
    0x2c5f20, 0x2c5f2c, 0x2c5f40, 0x2c5ee4, 0x2c5f48, 0x2c5f40, 0x2c5ebc, 0x2c5ee8,
    0x2c5f50, 0x2c5f58, 0x2c5f60, 0x2c5f68, 0x2c5f38, 0x2c5f70, 0x2c5f98, 0x2c5f80,
};

static const uint32_t save_key_fields[32] = {
    0x2c60a0, 0x2c60a0, 0x2c60a4, 0x2c60a8, 0x2c60cc, 0x2c60e0, 0x2c60ec, 0x2c6078,
    0x2c607c, 0x2c60f4, 0x2c60ec, 0x2c60cc, 0x2c6080, 0x2c60fc, 0x2c6088, 0x2c6090,
    0x2c60f4, 0x2c60ec, 0x2c60b0, 0x2c60b8, 0x2c60f4, 0x2c60ec, 0x2c60c0, 0x2c60c4,
    0x2c611c, 0x2c6128, 0x2c60f4, 0x2c60fc, 0x2c6104, 0x2c60e0, 0x2c60a4, 0x2c610c,
};

static bool read_key_fields(AbSpan library, const uint32_t fields[32],
                            uint8_t output[32], AbError *error) {
    uint8_t key[32];
    for (size_t index = 0; index < sizeof(key); ++index) {
        size_t offset = fields[index];
        if (!ab_range(library.size, offset, 4)) {
            return ab_fail(error, "Truncated target resource-key data field");
        }
        uint32_t instruction = ab_u32le(library.data + offset);
        if ((instruction & UINT32_C(0xffe00000)) != UINT32_C(0xe3a00000)) {
            return ab_fail(error, "Unexpected target resource-key data encoding");
        }
        uint32_t value = instruction & 255;
        unsigned rotation = ((instruction >> 8) & 15) * 2;
        if (rotation) {
            value = (value >> rotation) | (value << (32 - rotation));
        }
        if (value > 255) {
            return ab_fail(error, "Invalid target resource-key byte field");
        }
        key[index] = (uint8_t)value;
    }
    memcpy(output, key, sizeof(key));
    return true;
}

static bool target_key(const AbZip *archive, size_t allocation_limit,
                       const uint32_t fields[32], uint8_t output[32], AbError *error) {
    if (!archive || !output) {
        return ab_fail(error, "Missing target resource archive or key output");
    }
    const AbZipEntry *entry =
        ab_zip_find(archive, "lib/armeabi-v7a/libAngryBirdsClassic.so");
    if (!entry || entry->size != UINT32_C(12170760)) {
        return ab_fail(error, "Missing pinned ARMv7 resource-key input");
    }
    AbBuffer library = {0};
    if (!ab_zip_read(archive, (size_t)(entry - archive->entries), allocation_limit,
                     &library, error)) {
        return false;
    }
    uint8_t digest[32];
    char hash[65];
    bool okay = ab_sha256((AbSpan){library.data, library.size}, digest, error);
    if (okay) {
        ab_digest_hex(digest, hash);
        if (strcmp(
                hash,
                "ada9bc1a1469c30a9bf906c4293df3ff1a75485b40ddbfb146debbc41f14d38c") !=
            0) {
            okay =
                ab_fail(error, "Resource-key reader requires the pinned ARMv7 library");
        }
    }
    if (okay) {
        okay = read_key_fields((AbSpan){library.data, library.size}, fields, output,
                               error);
    }
    ab_buffer_free(&library);
    return okay;
}

bool ab_target_resource_key(const AbZip *archive, size_t allocation_limit,
                            uint8_t output[32], AbError *error) {
    return target_key(archive, allocation_limit, key_fields, output, error);
}

bool ab_target_save_key(const AbZip *archive, size_t allocation_limit,
                        uint8_t output[32], AbError *error) {
    return target_key(archive, allocation_limit, save_key_fields, output, error);
}

bool ab_target_packed_read(const AbZip *archive, const char *path, AbSpan key,
                           size_t allocation_limit, AbBuffer *output, AbError *error) {
    if (!output) {
        return ab_fail(error, "Missing packed target resource output");
    }
    *output = (AbBuffer){0};
    if (!archive || !path || !key.data || key.size != 32) {
        return ab_fail(error, "Invalid packed target resource input");
    }
    const AbZipEntry *entry = ab_zip_find(archive, path);
    if (!entry || entry->size > allocation_limit / 2) {
        return ab_fail(error,
                       "Packed target resource is absent or exceeds payload limit");
    }
    AbBuffer encrypted = {0}, decrypted = {0};
    bool okay = false;
    if (!ab_zip_read(archive, (size_t)(entry - archive->entries), allocation_limit / 2,
                     &encrypted, error) ||
        !ab_asset_decrypt((AbSpan){encrypted.data, encrypted.size}, key, &decrypted,
                          error)) {
        goto release_payloads;
    }
    if (encrypted.size > allocation_limit ||
        decrypted.size > allocation_limit - encrypted.size) {
        ab_fail(error, "Packed target resource exceeds payload limit");
        goto release_payloads;
    }
    size_t remaining = allocation_limit - encrypted.size - decrypted.size;
    okay = ab_asset_unpack((AbSpan){decrypted.data, decrypted.size}, remaining, output,
                           error);

release_payloads:
    ab_buffer_free(&decrypted);
    ab_buffer_free(&encrypted);
    return okay;
}
