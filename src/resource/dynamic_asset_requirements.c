#include "ab/dynamic_asset_requirements.h"

#include <string.h>

static bool constant_is(const AbLuaPrototype *prototype, size_t index,
                        const char *text) {
    if (index >= prototype->constant_count) {
        return false;
    }
    const AbLuaConstant *constant = prototype->constants + index;
    return constant->kind == AB_LUA_STRING && constant->string.size == strlen(text) &&
           !memcmp(constant->string.data, text, constant->string.size);
}

static bool registration(const AbLuaPrototype *prototype, size_t *literal_words,
                         AbError *error) {
    size_t count = prototype->instructions.size / 4;
    if (count < 6) {
        return ab_fail(error, "Dynamic requirements lack literal registration");
    }
    const uint8_t *tail = prototype->instructions.data + (count - 5) * 4;
    uint32_t global = ab_u32le(tail);
    uint32_t field = ab_u32le(tail + 4);
    /* The resource registers one literal table with dynamic.addreq. Recognize
     * operands, but never execute the registration call or trailing return. */
    if ((global & UINT32_C(0x3fff)) != ((1u << 6) | 5u) ||
        !constant_is(prototype, global >> 14, "dynamic") ||
        (field & UINT32_C(0xff803fff)) != ((1u << 23) | (1u << 6) | 6u) ||
        !((field >> 14) & 256) ||
        !constant_is(prototype, (field >> 14) & 255, "addreq") ||
        ab_u32le(tail + 8) != (2u << 6) ||
        ab_u32le(tail + 12) != ((2u << 23) | (1u << 14) | (1u << 6) | 28u) ||
        ab_u32le(tail + 16) != ((1u << 23) | 30u)) {
        return ab_fail(error, "Unsupported dynamic requirement registration shape");
    }
    *literal_words = count - 5;
    return true;
}

static bool string_value(AbDataValue value) {
    return value.kind == AB_DATA_STRING && value.string.data && value.string.size &&
           !memchr(value.string.data, 0, value.string.size);
}

static bool validate(const AbDataTable *requirements, AbError *error) {
    for (size_t index = 0; index < requirements->count; ++index) {
        const AbDataEntry *entry = requirements->entries + index;
        if (!string_value(entry->key) || entry->value.kind != AB_DATA_TABLE ||
            !entry->value.table || entry->value.table->count >= UINT32_C(16777216)) {
            return ab_fail(error, "Invalid dynamic asset requirement entry");
        }
        const AbDataTable *groups = entry->value.table;
        for (size_t ordinal = 1; ordinal <= groups->count; ++ordinal) {
            float numeric = (float)ordinal;
            AbDataValue key = {.kind = AB_DATA_NUMBER};
            memcpy(&key.bits, &numeric, sizeof(numeric));
            const AbDataValue *group = ab_data_get(groups, key);
            if (!group || !string_value(*group)) {
                return ab_fail(error, "Dynamic asset groups are not dense strings");
            }
        }
    }
    return true;
}

bool ab_dynamic_asset_requirements_open(const AbZip *archive, AbSpan key,
                                        size_t allocation_limit,
                                        AbDynamicAssetRequirements *output,
                                        AbError *error) {
    if (!output) {
        return ab_fail(error, "Missing dynamic asset requirements output");
    }
    *output = (AbDynamicAssetRequirements){0};
    AbTargetData *source = &output->source;
    if (!ab_target_packed_read(archive, "assets/data/scripts/dynamic/default.lua", key,
                               allocation_limit, &source->bytes, error)) {
        return false;
    }
    size_t remaining = allocation_limit - source->bytes.size;
    if (!ab_lua_read((AbSpan){source->bytes.data, source->bytes.size}, remaining,
                     &source->chunk, error)) {
        goto release_requirements;
    }
    size_t words = 0;
    if (source->chunk.allocated_bytes > remaining) {
        ab_fail(error, "Dynamic requirement metadata exceeds allocation limit");
        goto release_requirements;
    }
    if (!registration(&source->chunk.root, &words, error)) {
        goto release_requirements;
    }
    remaining -= source->chunk.allocated_bytes;
    if (!ab_lua_data_literal_table(&source->chunk, words, 0, remaining, &source->data,
                                   error) ||
        !validate(source->data.returned.table, error)) {
        goto release_requirements;
    }
    output->requirements = source->data.returned.table;
    return true;

release_requirements:
    ab_dynamic_asset_requirements_close(output);
    return false;
}

const AbDataTable *
ab_dynamic_asset_requirements_get(const AbDynamicAssetRequirements *requirements,
                                  AbSpan name) {
    if (!requirements || !name.data || !name.size) {
        return NULL;
    }
    const AbDataValue *entry =
        ab_data_get(requirements->requirements,
                    (AbDataValue){.kind = AB_DATA_STRING, .string = name});
    return entry && entry->kind == AB_DATA_TABLE ? entry->table : NULL;
}

void ab_dynamic_asset_requirements_close(AbDynamicAssetRequirements *requirements) {
    if (!requirements) {
        return;
    }
    ab_target_data_close(&requirements->source);
    *requirements = (AbDynamicAssetRequirements){0};
}
