#include "ab/audio_catalog.h"
#include "ab/hash.h"
#include "ab/target_resources.h"

#include "data_internal.h"

#include <string.h>

static void adopt_data(AbLuaData *destination, AbLuaData *source) {
    AbDataTable *table = source->owned;
    while (table) {
        AbDataTable *next = table->next_owned;
        table->next_owned = destination->owned;
        destination->owned = table;
        table = next;
    }
    AbDataTableLayout *layout = source->layouts;
    while (layout) {
        AbDataTableLayout *next = layout->next;
        layout->next = destination->layouts;
        destination->layouts = layout;
        layout = next;
    }
    destination->allocated_bytes += source->allocated_bytes;
    destination->table_count += source->table_count;
    *source = (AbLuaData){0};
}

static bool read_registration(AbAudioCatalog *catalog, const AbLuaPrototype *prototype,
                              size_t first, size_t count, AbDataTable *registrations,
                              AbError *error) {
    if (!count || count >= 32) {
        return ab_fail(error, "Unexpected audio registration constructor size");
    }
    uint8_t instructions[32 * 4];
    memcpy(instructions, prototype->instructions.data + first * 4, count * 4);
    /* Return the property table instead of invoking createAudio. All copied
     * instructions are handled by the existing restricted data reader. */
    uint32_t return_word = 30 | (1 << 6) | (2 << 23);
    for (size_t byte = 0; byte < 4; ++byte) {
        instructions[count * 4 + byte] = (uint8_t)(return_word >> (byte * 8));
    }
    AbLuaChunk constructor = catalog->chunk;
    constructor.root = *prototype;
    constructor.root.instructions = (AbSpan){instructions, (count + 1) * 4};
    AbLuaData property = {0};
    size_t remaining = catalog->data.memory_limit - catalog->data.allocated_bytes;
    if (!ab_lua_data(&constructor, remaining, &property, error)) {
        return false;
    }
    if (property.returned.kind != AB_DATA_TABLE || !property.returned.table) {
        ab_lua_data_close(&property);
        return ab_fail(error, "Expected a constant audio property table");
    }
    AbDataValue value = property.returned;
    adopt_data(&catalog->data, &property);
    float index = (float)(registrations->count + 1);
    uint32_t bits;
    memcpy(&bits, &index, sizeof(bits));
    return ab_data_table_put(&catalog->data, registrations,
                             (AbDataValue){.kind = AB_DATA_NUMBER, .bits = bits}, value,
                             error);
}

static bool read_registrations(AbAudioCatalog *catalog, AbError *error) {
    if (catalog->chunk.root.child_count != 2) {
        return ab_fail(error, "Unexpected audio catalog function inventory");
    }
    const AbLuaPrototype *prototype = catalog->chunk.root.children;
    if (prototype->file_offset != 7057 || prototype->child_count ||
        prototype->upvalue_count || prototype->parameter_count ||
        prototype->instructions.size != 2441 * 4) {
        return ab_fail(error, "Unexpected audio registration data structure");
    }
    AbDataTable *registrations = ab_data_table_new(&catalog->data, error);
    if (!registrations) {
        return false;
    }
    size_t words = prototype->instructions.size / 4;
    for (size_t cursor = 0; cursor < words; ++cursor) {
        uint32_t word = ab_u32le(prototype->instructions.data + cursor * 4);
        /* In the pinned resource, GETGLOBAL R0,K0 names createAudio. Other
         * globals belong to the separate channel configuration interval. */
        if (word != 5) {
            continue;
        }
        size_t first = ++cursor;
        while (cursor < words && ab_u32le(prototype->instructions.data + cursor * 4) !=
                                     UINT32_C(0x0100401c)) {
            if (cursor - first >= 31) {
                return ab_fail(error, "Unbounded audio registration constructor");
            }
            ++cursor;
        }
        if (cursor == words || registrations->count >= 340) {
            return ab_fail(error, "Unexpected audio registration call inventory");
        }
        if (!read_registration(catalog, prototype, first, cursor - first, registrations,
                               error)) {
            return false;
        }
    }
    if (registrations->count != 340) {
        return ab_fail(error, "Incomplete audio registration catalog");
    }
    catalog->registrations = registrations;
    return true;
}

void ab_audio_catalog_close(AbAudioCatalog *catalog) {
    if (!catalog) {
        return;
    }
    ab_lua_data_close(&catalog->data);
    ab_lua_close(&catalog->chunk);
    ab_buffer_free(&catalog->owned_bytes);
    *catalog = (AbAudioCatalog){0};
}

bool ab_audio_catalog_read(AbSpan decoded, size_t memory_limit, AbAudioCatalog *output,
                           AbError *error) {
    if (!output) {
        return ab_fail(error, "Missing audio catalog output");
    }
    *output = (AbAudioCatalog){0};
    if (!decoded.data || decoded.size != 33920) {
        return ab_fail(error, "Expected pinned decoded audio catalog size");
    }
    uint8_t digest[32];
    char hash[65];
    if (!ab_sha256(decoded, digest, error)) {
        return false;
    }
    ab_digest_hex(digest, hash);
    if (strcmp(hash,
               "5d1c057fdfdeea7e15bcda6d435848f27a5baf20cf6bb05ac591a822a6c214aa") !=
        0) {
        return ab_fail(error, "Expected pinned decoded audio catalog hash");
    }
    if (!ab_lua_read(decoded, memory_limit, &output->chunk, error)) {
        goto release_catalog;
    }
    if (output->chunk.allocated_bytes > memory_limit ||
        output->chunk.root.instructions.size != 354 * 4) {
        ab_fail(error, "Invalid audio catalog structure or allocation bound");
        goto release_catalog;
    }
    /* The preceding deviceModel branch writes only g_useMp3Sound. The following
     * closures define resource-loading functions. Neither belongs to this
     * independent constant constructor. Reuse its original no-value return. */
    uint8_t instructions[337 * 4];
    memcpy(instructions, output->chunk.root.instructions.data + 13 * 4, 336 * 4);
    memcpy(instructions + 336 * 4, output->chunk.root.instructions.data + 353 * 4, 4);
    AbLuaChunk constructor = output->chunk;
    constructor.root.children = NULL;
    constructor.root.child_count = 0;
    constructor.root.instructions = (AbSpan){instructions, sizeof(instructions)};
    size_t remaining = memory_limit - output->chunk.allocated_bytes;
    if (!ab_lua_data(&constructor, remaining, &output->data, error)) {
        goto release_catalog;
    }
    const AbDataValue *groups =
        ab_data_get(output->data.globals,
                    (AbDataValue){.kind = AB_DATA_STRING,
                                  .string = {(const uint8_t *)"audioGroups", 11}});
    if (!groups || groups->kind != AB_DATA_TABLE || !groups->table ||
        groups->table->count != 46) {
        ab_fail(error, "Invalid recovered audio-group catalog");
        goto release_catalog;
    }
    output->groups = groups->table;
    if (!read_registrations(output, error)) {
        goto release_catalog;
    }
    return true;

release_catalog:
    ab_audio_catalog_close(output);
    return false;
}

bool ab_audio_catalog_open(const AbZip *archive, AbSpan key, size_t allocation_limit,
                           AbAudioCatalog *output, AbError *error) {
    if (!output) {
        return ab_fail(error, "Missing audio catalog output");
    }
    *output = (AbAudioCatalog){0};
    AbBuffer decoded = {0};
    if (!ab_target_packed_read(archive, "assets/data/scripts/audio.lua", key,
                               allocation_limit, &decoded, error)) {
        return false;
    }
    if (!ab_audio_catalog_read((AbSpan){decoded.data, decoded.size},
                               allocation_limit - decoded.size, output, error)) {
        ab_buffer_free(&decoded);
        return false;
    }
    output->owned_bytes = decoded;
    return true;
}
