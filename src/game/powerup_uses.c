#include "ab/powerup_uses.h"

#include <stdlib.h>
#include <string.h>

struct AbPowerupUses {
    char *names;
    size_t capacity, maximum_name, count, bytes;
};

static unsigned char lower(unsigned char byte) {
    return byte >= 'A' && byte <= 'Z' ? (unsigned char)(byte + ('a' - 'A')) : byte;
}

static bool name_length(const AbPowerupUses *uses, const char *name, size_t *output,
                        AbError *error) {
    if (!uses || !name || !output)
        return ab_fail(error, "Invalid powerup use name query");
    for (size_t length = 0; length <= uses->maximum_name; ++length) {
        unsigned char byte = (unsigned char)name[length];
        if (!byte) {
            *output = length;
            return true;
        }
        if (byte >= 128)
            return ab_fail(error, "Unsupported non-ASCII powerup use name");
    }
    return ab_fail(error, "Powerup use name exceeds byte limit");
}

bool ab_powerup_uses_open(size_t capacity, size_t max_name_bytes, size_t storage_bytes,
                          AbPowerupUses **output, AbError *error) {
    if (!output)
        return ab_fail(error, "Missing powerup use list output");
    *output = NULL;
    if (max_name_bytes == SIZE_MAX || storage_bytes < sizeof(AbPowerupUses) ||
        capacity > (storage_bytes - sizeof(AbPowerupUses)) / (max_name_bytes + 1))
        return ab_fail(error, "Powerup use list exceeds storage limit");
    size_t names = capacity * (max_name_bytes + 1);
    AbPowerupUses *uses = calloc(1, sizeof(*uses));
    if (!uses)
        return ab_fail(error, "Cannot allocate powerup use list");
    if (names) {
        uses->names = calloc(names, 1);
        if (!uses->names) {
            ab_powerup_uses_close(uses);
            return ab_fail(error, "Cannot allocate powerup use names");
        }
    }
    uses->capacity = capacity;
    uses->maximum_name = max_name_bytes;
    uses->bytes = sizeof(*uses) + names;
    *output = uses;
    return true;
}

void ab_powerup_uses_close(AbPowerupUses *uses) {
    if (uses) {
        free(uses->names);
        free(uses);
    }
}

size_t ab_powerup_uses_bytes(const AbPowerupUses *uses) {
    return uses ? uses->bytes : 0;
}

size_t ab_powerup_uses_count(const AbPowerupUses *uses) {
    return uses ? uses->count : 0;
}

bool ab_powerup_uses_mark(AbPowerupUses *uses, const char *name, AbError *error) {
    size_t length = 0;
    if (!name_length(uses, name, &length, error))
        return false;
    if (uses->count == uses->capacity)
        return ab_fail(error, "Powerup use list capacity exhausted");
    char *destination = uses->names + uses->count * (uses->maximum_name + 1);
    for (size_t index = 0; index < length; ++index)
        destination[index] = (char)lower((unsigned char)name[index]);
    destination[length] = 0;
    ++uses->count;
    return true;
}

bool ab_powerup_uses_contains(const AbPowerupUses *uses, const char *name, bool *output,
                              AbError *error) {
    size_t length = 0;
    if (!output)
        return ab_fail(error, "Missing powerup use query output");
    if (!name_length(uses, name, &length, error))
        return false;
    for (size_t index = 0; index < uses->count; ++index) {
        const char *stored = uses->names + index * (uses->maximum_name + 1);
        bool equal = strlen(stored) == length;
        for (size_t byte = 0; equal && byte < length; ++byte)
            equal = (unsigned char)stored[byte] == lower((unsigned char)name[byte]);
        if (equal) {
            *output = true;
            return true;
        }
    }
    *output = false;
    return true;
}

bool ab_powerup_uses_clear(AbPowerupUses *uses) {
    if (!uses)
        return false;
    uses->count = 0;
    return true;
}
