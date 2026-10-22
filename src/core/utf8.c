#include "ab/utf8.h"

#include <string.h>

bool ab_utf8_decode(AbSpan input, size_t *consumed, uint32_t *code) {
    if (!consumed || !code || (input.size && !input.data)) {
        return false;
    }
    *consumed = input.size ? 1 : 0;
    if (!input.size) {
        return false;
    }
    uint8_t first = input.data[0];
    size_t count;
    uint32_t value;
    if (first < 0x80) {
        count = 1;
        value = first;
    } else if (first >= 0xc0 && first < 0xe0) {
        count = 2;
        value = first & 0x1fu;
    } else if (first >= 0xe0 && first < 0xf0) {
        count = 3;
        value = first & 0x0fu;
    } else if (first >= 0xf0 && first < 0xf8) {
        count = 4;
        value = first & 0x07u;
    } else {
        return false;
    }
    if (input.size < count) {
        return false;
    }
    for (size_t index = 1; index < count; ++index) {
        value = (value << 6) | (input.data[index] & 0x3fu);
    }
    *consumed = count;
    *code = value;
    return true;
}

bool ab_utf8_to_codes(AbSpan input, uint32_t *output, size_t capacity,
                      size_t *output_count) {
    if (!output_count || (input.size && !input.data) || (capacity && !output)) {
        return false;
    }
    size_t offset = 0, count = 0;
    while (offset < input.size) {
        size_t consumed = 1;
        uint32_t code = 0;
        bool decoded = ab_utf8_decode(
            (AbSpan){input.data + offset, input.size - offset}, &consumed, &code);
        if (decoded) {
            if (count == capacity) {
                return false;
            }
            output[count++] = code;
        }
        offset += consumed;
    }
    *output_count = count;
    return true;
}

bool ab_utf8_encode(uint32_t code, uint8_t *bytes, size_t capacity, size_t *written) {
    if (!written || (capacity && !bytes)) {
        return false;
    }
    *written = 0;
    size_t count;
    if (code < 0x80 || code >= UINT32_C(0x80000000)) {
        count = 1;
    } else if (code < 0x800) {
        count = 2;
    } else if (code < 0x10000) {
        count = 3;
    } else if (code < 0x200000) {
        count = 4;
    } else {
        return false;
    }
    if (count > capacity) {
        return false;
    }
    if (count == 1) {
        bytes[0] = (uint8_t)code;
    } else {
        uint32_t value = code;
        for (size_t index = count - 1; index; --index) {
            bytes[index] = (uint8_t)(0x80u | (value & 0x3fu));
            value >>= 6;
        }
        static const uint8_t prefix[] = {0, 0, 0xc0, 0xe0, 0xf0};
        bytes[0] = (uint8_t)(prefix[count] | value);
    }
    *written = count;
    return true;
}

bool ab_utf8_from_codes(const uint32_t *codes, size_t count, uint8_t *output,
                        size_t capacity, size_t *output_count) {
    if (!output_count || (count && !codes) || (capacity && !output)) {
        return false;
    }
    size_t written = 0;
    for (size_t index = 0; index < count; ++index) {
        uint8_t bytes[4];
        size_t length;
        if (!ab_utf8_encode(codes[index], bytes, sizeof(bytes), &length)) {
            continue;
        }
        if (length > capacity - written) {
            return false;
        }
        memcpy(output + written, bytes, length);
        written += length;
    }
    *output_count = written;
    return true;
}
