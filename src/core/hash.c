#include "ab/hash.h"

#include <string.h>

uint32_t ab_crc32(AbSpan input) {
    uint32_t table[256];
    for (uint32_t index = 0; index < 256; ++index) {
        uint32_t value = index;
        for (unsigned bit = 0; bit < 8; ++bit) {
            value = (value >> 1) ^ ((value & 1) ? UINT32_C(0xedb88320) : 0);
        }
        table[index] = value;
    }
    uint32_t value = UINT32_MAX;
    for (size_t index = 0; index < input.size; ++index) {
        value = table[(value ^ input.data[index]) & 255] ^ (value >> 8);
    }
    return value ^ UINT32_MAX;
}

static uint32_t rotate_right(uint32_t value, unsigned bits) {
    return (value >> bits) | (value << (32 - bits));
}

/* Round constants defined by FIPS 180-4, section 4.2.2. */
static const uint32_t round_constants[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4,
    0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe,
    0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f,
    0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc,
    0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
    0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116,
    0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7,
    0xc67178f2,
};

static void compress_block(uint32_t state[8], const uint8_t block[64]) {
    uint32_t words[64];
    for (size_t index = 0; index < 16; ++index) {
        words[index] = ab_u32be(block + index * 4);
    }
    for (size_t index = 16; index < 64; ++index) {
        uint32_t earlier = words[index - 15];
        uint32_t previous = words[index - 2];
        uint32_t small0 =
            rotate_right(earlier, 7) ^ rotate_right(earlier, 18) ^ (earlier >> 3);
        uint32_t small1 =
            rotate_right(previous, 17) ^ rotate_right(previous, 19) ^ (previous >> 10);
        words[index] = words[index - 16] + small0 + words[index - 7] + small1;
    }
    uint32_t a = state[0];
    uint32_t b = state[1];
    uint32_t c = state[2];
    uint32_t d = state[3];
    uint32_t e = state[4];
    uint32_t f = state[5];
    uint32_t g = state[6];
    uint32_t h = state[7];
    for (size_t index = 0; index < 64; ++index) {
        uint32_t sum1 = rotate_right(e, 6) ^ rotate_right(e, 11) ^ rotate_right(e, 25);
        uint32_t choose = (e & f) ^ (~e & g);
        uint32_t temp1 = h + sum1 + choose + round_constants[index] + words[index];
        uint32_t sum0 = rotate_right(a, 2) ^ rotate_right(a, 13) ^ rotate_right(a, 22);
        uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + sum0 + majority;
    }
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

bool ab_sha256(AbSpan input, uint8_t output[32], AbError *error) {
    if (!output || (!input.data && input.size)) {
        return ab_fail(error, "Invalid SHA-256 input");
    }
#if SIZE_MAX > UINT64_MAX / 8
    if (input.size > UINT64_MAX / 8) {
        return ab_fail(error, "Invalid SHA-256 input");
    }
#endif
    uint32_t state[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19,
    };
    size_t offset = 0;
    while (input.size - offset >= 64) {
        compress_block(state, input.data + offset);
        offset += 64;
    }
    uint8_t tail[128] = {0};
    size_t remaining = input.size - offset;
    if (remaining) {
        memcpy(tail, input.data + offset, remaining);
    }
    tail[remaining] = 0x80;
    size_t tail_size = remaining < 56 ? 64 : 128;
    uint64_t bit_length = (uint64_t)input.size * 8;
    for (unsigned index = 0; index < 8; ++index) {
        tail[tail_size - 1 - index] = (uint8_t)(bit_length >> (index * 8));
    }
    compress_block(state, tail);
    if (tail_size == 128) {
        compress_block(state, tail + 64);
    }
    for (size_t index = 0; index < 32; ++index) {
        output[index] = (uint8_t)(state[index / 4] >> ((3 - index % 4) * 8));
    }
    return true;
}

void ab_digest_hex(const uint8_t digest[32], char output[65]) {
    static const char digits[] = "0123456789abcdef";
    for (size_t index = 0; index < 32; ++index) {
        output[index * 2] = digits[digest[index] >> 4];
        output[index * 2 + 1] = digits[digest[index] & 15];
    }
    output[64] = '\0';
}
