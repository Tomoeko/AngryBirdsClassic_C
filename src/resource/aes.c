#include "ab/aes.h"

#include <stdlib.h>
#include <string.h>

static uint8_t field_multiply(uint8_t left, uint8_t right) {
    uint8_t result = 0;
    for (unsigned bit = 0; bit < 8; ++bit) {
        if (right & 1) {
            result ^= left;
        }
        bool high = (left & 128) != 0;
        left = (uint8_t)(left << 1);
        if (high) {
            left ^= 0x1b;
        }
        right >>= 1;
    }
    return result;
}

static uint8_t rotate_byte(uint8_t value, unsigned bits) {
    return (uint8_t)((unsigned)value << bits | (unsigned)value >> (8 - bits));
}

/* Derive the substitution boxes from the field and affine map in FIPS 197. */
static void substitution_boxes(uint8_t forward[256], uint8_t inverse[256]) {
    for (unsigned value = 0; value < 256; ++value) {
        uint8_t reciprocal = 1;
        uint8_t power = (uint8_t)value;
        for (unsigned exponent = 254; exponent; exponent >>= 1) {
            if (exponent & 1) {
                reciprocal = field_multiply(reciprocal, power);
            }
            power = field_multiply(power, power);
        }
        uint8_t transformed = reciprocal ^ rotate_byte(reciprocal, 1) ^
                              rotate_byte(reciprocal, 2) ^ rotate_byte(reciprocal, 3) ^
                              rotate_byte(reciprocal, 4) ^ 0x63;
        forward[value] = transformed;
        inverse[transformed] = (uint8_t)value;
    }
}

static unsigned expand_key(AbSpan key, const uint8_t box[256], uint8_t expanded[240]) {
    unsigned rounds = (unsigned)(key.size / 4) + 6;
    size_t expanded_size = ((size_t)rounds + 1) * 16;
    memcpy(expanded, key.data, key.size);
    uint8_t round_constant = 1;
    for (size_t offset = key.size; offset < expanded_size; offset += 4) {
        uint8_t word[4];
        memcpy(word, expanded + offset - 4, sizeof(word));
        if (offset % key.size == 0) {
            uint8_t first = word[0];
            word[0] = box[word[1]] ^ round_constant;
            word[1] = box[word[2]];
            word[2] = box[word[3]];
            word[3] = box[first];
            round_constant = field_multiply(round_constant, 2);
        } else if (key.size == 32 && offset % key.size == 16) {
            for (size_t index = 0; index < 4; ++index) {
                word[index] = box[word[index]];
            }
        }
        for (size_t index = 0; index < 4; ++index) {
            expanded[offset + index] =
                expanded[offset + index - key.size] ^ word[index];
        }
    }
    return rounds;
}

static void add_round_key(uint8_t state[16], const uint8_t *key) {
    for (size_t index = 0; index < 16; ++index) {
        state[index] ^= key[index];
    }
}

static void inverse_rows_and_bytes(uint8_t state[16], const uint8_t inverse[256]) {
    uint8_t previous[16];
    memcpy(previous, state, sizeof(previous));
    for (size_t column = 0; column < 4; ++column) {
        for (size_t row = 0; row < 4; ++row) {
            state[column * 4 + row] =
                inverse[previous[((column + 4 - row) % 4) * 4 + row]];
        }
    }
}

static void mix_columns(uint8_t state[16], const uint8_t factors[4]) {
    for (size_t column = 0; column < 4; ++column) {
        uint8_t previous[4];
        memcpy(previous, state + column * 4, sizeof(previous));
        for (size_t row = 0; row < 4; ++row) {
            uint8_t result = 0;
            for (size_t term = 0; term < 4; ++term) {
                result ^= field_multiply(previous[(row + term) % 4], factors[term]);
            }
            state[column * 4 + row] = result;
        }
    }
}

static void decrypt_block(uint8_t state[16], const uint8_t expanded[240],
                          unsigned rounds, const uint8_t inverse[256]) {
    add_round_key(state, expanded + rounds * 16);
    for (unsigned round = rounds - 1; round > 0; --round) {
        inverse_rows_and_bytes(state, inverse);
        add_round_key(state, expanded + round * 16);
        mix_columns(state, (const uint8_t[4]){14, 11, 13, 9});
    }
    inverse_rows_and_bytes(state, inverse);
    add_round_key(state, expanded);
}

static void forward_rows_and_bytes(uint8_t state[16], const uint8_t box[256]) {
    uint8_t previous[16];
    memcpy(previous, state, sizeof(previous));
    for (size_t column = 0; column < 4; ++column) {
        for (size_t row = 0; row < 4; ++row) {
            state[column * 4 + row] = box[previous[((column + row) % 4) * 4 + row]];
        }
    }
}

static void encrypt_block(uint8_t state[16], const uint8_t expanded[240],
                          unsigned rounds, const uint8_t box[256]) {
    add_round_key(state, expanded);
    for (unsigned round = 1; round < rounds; ++round) {
        forward_rows_and_bytes(state, box);
        mix_columns(state, (const uint8_t[4]){2, 3, 1, 1});
        add_round_key(state, expanded + round * 16);
    }
    forward_rows_and_bytes(state, box);
    add_round_key(state, expanded + rounds * 16);
}

bool ab_aes_cbc_encrypt(AbSpan plaintext, AbSpan key, const uint8_t iv[16],
                        uint8_t *output, AbError *error) {
    if (!key.data || (key.size != 16 && key.size != 24 && key.size != 32) || !iv ||
        plaintext.size % 16 || (!plaintext.data && plaintext.size) ||
        (!output && plaintext.size)) {
        return ab_fail(error, "Invalid AES key, block length, IV, or output buffer");
    }
    uint8_t box[256], inverse[256], expanded[240], previous[16];
    substitution_boxes(box, inverse);
    unsigned rounds = expand_key(key, box, expanded);
    memcpy(previous, iv, sizeof(previous));
    for (size_t offset = 0; offset < plaintext.size; offset += 16) {
        for (size_t index = 0; index < 16; ++index) {
            previous[index] ^= plaintext.data[offset + index];
        }
        encrypt_block(previous, expanded, rounds, box);
        memcpy(output + offset, previous, sizeof(previous));
    }
    return true;
}

bool ab_aes_cbc_decrypt(AbSpan ciphertext, AbSpan key, const uint8_t iv[16],
                        uint8_t *output, AbError *error) {
    if (!key.data || (key.size != 16 && key.size != 24 && key.size != 32) || !iv ||
        ciphertext.size % 16 || (!ciphertext.data && ciphertext.size) ||
        (!output && ciphertext.size)) {
        return ab_fail(error, "Invalid AES key, block length, IV, or output buffer");
    }
    uint8_t box[256];
    uint8_t inverse[256];
    uint8_t expanded[240];
    uint8_t previous[16];
    substitution_boxes(box, inverse);
    unsigned rounds = expand_key(key, box, expanded);
    memcpy(previous, iv, sizeof(previous));
    for (size_t offset = 0; offset < ciphertext.size; offset += 16) {
        uint8_t encrypted[16];
        uint8_t state[16];
        memcpy(encrypted, ciphertext.data + offset, 16);
        memcpy(state, encrypted, sizeof(state));
        decrypt_block(state, expanded, rounds, inverse);
        for (size_t index = 0; index < 16; ++index) {
            output[offset + index] = state[index] ^ previous[index];
        }
        memcpy(previous, encrypted, sizeof(previous));
    }
    return true;
}

bool ab_asset_decrypt(AbSpan ciphertext, AbSpan key, AbBuffer *output, AbError *error) {
    if (!output) {
        return ab_fail(error, "Missing asset-decryption output");
    }
    *output = (AbBuffer){0};
    if (!ciphertext.size) {
        return ab_aes_cbc_decrypt(ciphertext, key, (const uint8_t[16]){0}, NULL, error);
    }
    if (ciphertext.size % 16) {
        return ab_fail(error, "Encrypted asset length is not a multiple of 16");
    }
    uint8_t *bytes = malloc(ciphertext.size);
    if (!bytes) {
        return ab_fail(error, "Cannot allocate decrypted asset buffer");
    }
    if (!ab_aes_cbc_decrypt(ciphertext, key, (const uint8_t[16]){0}, bytes, error)) {
        free(bytes);
        return false;
    }
    size_t padding = bytes[ciphertext.size - 1];
    /* Target util::AES checks only the final count, not every padding byte. */
    if (padding > 16 || padding > ciphertext.size) {
        free(bytes);
        return ab_fail(error, "Encrypted asset has an invalid padding count");
    }
    *output = (AbBuffer){bytes, ciphertext.size - padding};
    return true;
}

bool ab_asset_encrypt(AbSpan plaintext, AbSpan key, AbBuffer *output, AbError *error) {
    if (!output) {
        return ab_fail(error, "Missing asset-encryption output");
    }
    *output = (AbBuffer){0};
    size_t padding = 16 - plaintext.size % 16;
    if ((!plaintext.data && plaintext.size) || plaintext.size > SIZE_MAX - padding ||
        !key.data || (key.size != 16 && key.size != 24 && key.size != 32)) {
        return ab_fail(error, "Invalid asset input size or encryption key");
    }
    size_t length = plaintext.size + padding;
    uint8_t *bytes = malloc(length);
    if (!bytes) {
        return ab_fail(error, "Cannot allocate encrypted asset buffer");
    }
    if (plaintext.size) {
        memcpy(bytes, plaintext.data, plaintext.size);
    }
    memset(bytes + plaintext.size, (int)padding, padding);
    if (!ab_aes_cbc_encrypt((AbSpan){bytes, length}, key, (const uint8_t[16]){0}, bytes,
                            error)) {
        free(bytes);
        return false;
    }
    *output = (AbBuffer){bytes, length};
    return true;
}
