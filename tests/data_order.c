#include "ab/data_order.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Failed at line %d: %s\n", __LINE__, #condition);          \
            return false;                                                              \
        }                                                                              \
    } while (0)

static bool edge_checks(void) {
    AbDataTable table = {0};
    AbStringOrderWorkspace workspace = {0};
    AbError error = {0};
    CHECK(ab_data_string_order(&table, 0, &workspace, &error));
    CHECK(!ab_data_string_order(NULL, 0, &workspace, &error));
    CHECK(!ab_data_string_order(&table, 0, NULL, &error));
    AbDataEntry entries[2] = {
        {.key = {.kind = AB_DATA_STRING}, .value = {.kind = AB_DATA_BOOLEAN}},
        {.key = {.kind = AB_DATA_STRING}, .value = {.kind = AB_DATA_BOOLEAN}},
    };
    AbStringOrderNode nodes[2];
    size_t indices[] = {91, 92};
    workspace = (AbStringOrderWorkspace){indices, 2, nodes, 2};
    table = (AbDataTable){entries, 1, 2, NULL};
    CHECK(ab_data_string_order(&table, 1, &workspace, &error));
    CHECK(indices[0] == 0 && indices[1] == 92);
    indices[0] = 91;
    table.count = 2;
    CHECK(!ab_data_string_order(&table, 2, &workspace, &error));
    CHECK(indices[0] == 91 && indices[1] == 92);
    CHECK(!ab_data_string_order(&table, 1, &workspace, &error));
    CHECK(!ab_data_string_order(&table, 3, &workspace, &error));
    table.count = 1;
    entries[0].key.kind = AB_DATA_NUMBER;
    CHECK(!ab_data_string_order(&table, 2, &workspace, &error));
    entries[0].key.kind = AB_DATA_STRING;
    entries[0].key.string.size = 1;
    CHECK(!ab_data_string_order(&table, 2, &workspace, &error));
    entries[0].key.string.size = 0;
    entries[0].value.kind = AB_DATA_NIL;
    CHECK(!ab_data_string_order(&table, 2, &workspace, &error));
    CHECK(indices[0] == 91 && indices[1] == 92);
    size_t final_capacity = 93;
    table.count = 1;
    entries[0].value = (AbDataValue){.kind = AB_DATA_BOOLEAN};
    CHECK(ab_data_string_order_growing(&table, &workspace, &final_capacity, &error));
    CHECK(indices[0] == 0 && final_capacity == 1);
    table.count = 2;
    CHECK(!ab_data_string_order_growing(&table, &workspace, &final_capacity, &error));
    CHECK(final_capacity == 1);
    table.count = 0;
    CHECK(ab_data_string_order_growing(&table, &workspace, &final_capacity, &error));
    CHECK(final_capacity == 0);
    CHECK(!ab_data_string_order_growing(&table, &workspace, NULL, &error));
    CHECK(!ab_data_string_order_growing(NULL, &workspace, &final_capacity, &error));
    table.count = 1;
    workspace.node_capacity = 0;
    CHECK(!ab_data_string_order_growing(&table, &workspace, &final_capacity, &error));
    return true;
}

static void put_word(uint8_t *output, uint32_t word) {
    for (size_t index = 0; index < 4; ++index) {
        output[index] = (uint8_t)(word >> (index * 8));
    }
}

static bool layout_checks(void) {
    uint8_t instructions[20];
    const uint32_t words[] = {
        (UINT32_C(1) << 14) | 10,
        UINT32_C(0x80000000) | (UINT32_C(257) << 14) | 9,
        (UINT32_C(1) << 6) | (UINT32_C(1) << 23) | 3,
        UINT32_C(0x80000000) | (UINT32_C(1) << 14) | 9,
        (UINT32_C(2) << 23) | 30,
    };
    for (size_t index = 0; index < 5; ++index) {
        put_word(instructions + index * 4, words[index]);
    }
    AbLuaConstant constants[] = {
        {.kind = AB_LUA_STRING, .string = {(const uint8_t *)"x", 1}},
        {.kind = AB_LUA_NUMBER, .bits = UINT32_C(0x3f800000)},
    };
    AbLuaChunk chunk = {
        .input = {instructions, sizeof(instructions)},
        .root = {.instructions = {instructions, sizeof(instructions)},
                 .stack_size = 2,
                 .constants = constants,
                 .constant_count = 2},
    };
    AbLuaData data = {0};
    AbError error = {0};
    CHECK(ab_lua_data(&chunk, 4096, &data, &error));
    CHECK(data.returned.kind == AB_DATA_TABLE && data.returned.table->count == 0);
    const AbDataTableLayout *layout = ab_lua_table_layout(&data, data.returned.table);
    CHECK(layout && layout->hash_capacity == 1 && !layout->no_nil_assignments);
    CHECK(!ab_lua_table_layout(NULL, data.returned.table));
    CHECK(!ab_lua_table_layout(&data, data.globals));
    ab_lua_data_close(&data);
    put_word(instructions + 12, words[1]);
    CHECK(ab_lua_data(&chunk, 4096, &data, &error));
    CHECK(data.returned.table->count == 1);
    layout = ab_lua_table_layout(&data, data.returned.table);
    CHECK(layout && layout->no_nil_assignments);
    ab_lua_data_close(&data);
    CHECK(!ab_lua_data(&chunk, 2 * sizeof(AbDataTable), &data, &error));
    CHECK(!data.owned && !data.layouts);
    put_word(instructions, (UINT32_C(511) << 14) | 10);
    CHECK(!ab_lua_data(&chunk, 4096, &data, &error));
    CHECK(!data.owned && !data.layouts);
    return true;
}

static bool native_checks(FILE *file) {
    char header[80];
    CHECK(fgets(header, sizeof(header), file));
    bool growing = strcmp(header, "AB_ANDROID_STRING_GROWTH_V1 4096\n") == 0;
    CHECK(growing || strcmp(header, "AB_ANDROID_STRING_ORDER_V1 4096\n") == 0);
    size_t total = 0;
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t capacity, count;
        CHECK(fscanf(file, "%8" SCNx32 "%8" SCNx32, &capacity, &count) == 2);
        CHECK(fgetc(file) == '\n' && capacity <= 64 && count <= capacity);
        uint8_t strings[64][256];
        AbDataEntry entries[64];
        for (size_t entry = 0; entry < count; ++entry) {
            uint32_t length;
            CHECK(fscanf(file, "%8" SCNx32, &length) == 1 && length <= 256);
            for (size_t offset = 0; offset < length; offset += 4) {
                uint32_t word;
                CHECK(fscanf(file, "%8" SCNx32, &word) == 1);
                for (size_t byte = 0; byte < 4 && offset + byte < length; ++byte) {
                    strings[entry][offset + byte] = (uint8_t)(word >> (byte * 8));
                }
            }
            CHECK(fgetc(file) == '\n');
            entries[entry] = (AbDataEntry){
                .key = {.kind = AB_DATA_STRING, .string = {strings[entry], length}},
                .value = {.kind = AB_DATA_BOOLEAN, .bits = 1},
            };
        }
        AbDataTable table = {entries, count, count, NULL};
        size_t indices[64];
        AbStringOrderNode nodes[64];
        AbStringOrderWorkspace workspace = {indices, 64, nodes, 64};
        AbError error = {0};
        if (growing) {
            size_t final_capacity;
            uint32_t expected_capacity;
            CHECK(ab_data_string_order_growing(&table, &workspace, &final_capacity,
                                               &error));
            CHECK(fscanf(file, "%8" SCNx32, &expected_capacity) == 1);
            CHECK(fgetc(file) == '\n' && final_capacity == expected_capacity);
        } else {
            CHECK(ab_data_string_order(&table, capacity, &workspace, &error));
        }
        for (size_t entry = 0; entry < count; ++entry) {
            uint32_t expected;
            CHECK(fscanf(file, "%8" SCNx32, &expected) == 1);
            if (indices[entry] != expected) {
                fprintf(stderr, "Native string order case %zu entry %zu: %zu != %u\n",
                        item, entry, indices[entry], expected);
                return false;
            }
        }
        if (count) {
            CHECK(fgetc(file) == '\n');
        }
        total += count;
    }
    CHECK(fgets(header, sizeof(header), file));
    CHECK(strcmp(header, growing ? "AB string growth reference complete\n"
                                 : "AB string order reference complete\n") == 0);
    CHECK(fgetc(file) == EOF && !ferror(file));
    printf("All %zu string entries across 4096 native table orders match.\n", total);
    return true;
}

static bool compare_level_order(FILE *file, const AbLuaData *data, bool created) {
    char header[64];
    size_t count, capacity;
    CHECK(fscanf(file, "%63s%zu%zu", header, &count, &capacity) == 3);
    CHECK(strcmp(header, created ? "AB_LEVEL_CREATED_ORDER_V1"
                                 : "AB_LEVEL_OBJECT_ORDER_V1") == 0 &&
          capacity <= 512);
    const AbDataValue *world = ab_data_get(
        data->globals,
        (AbDataValue){.kind = AB_DATA_STRING, .string = {(const uint8_t *)"world", 5}});
    CHECK(world && world->kind == AB_DATA_TABLE && world->table &&
          world->table->count == count);
    const AbDataTableLayout *layout = ab_lua_table_layout(data, world->table);
    CHECK(layout && layout->array_hint == 0 && layout->hash_capacity <= 512 &&
          layout->no_nil_assignments && (created || layout->hash_capacity == capacity));
    size_t indices[512];
    AbStringOrderNode nodes[512];
    AbStringOrderWorkspace workspace = {indices, 512, nodes, 512};
    AbError error = {0};
    CHECK(
        ab_data_string_order(world->table, layout->hash_capacity, &workspace, &error));
    const AbDataTable *ordered = world->table;
    AbDataEntry entries[512];
    AbDataTable object_table = {entries, 0, 512, NULL};
    if (created) {
        const AbDataEntry *ground = NULL;
        for (size_t cursor = 0; cursor < count; ++cursor) {
            const AbDataEntry *entry = world->table->entries + indices[cursor];
            if (entry->key.string.size == 6 &&
                memcmp(entry->key.string.data, "ground", 6) == 0) {
                ground = entry;
            } else {
                entries[object_table.count++] = *entry;
            }
        }
        CHECK(ground && object_table.count < 512);
        entries[object_table.count++] = *ground;
        size_t final_capacity;
        CHECK(ab_data_string_order_growing(&object_table, &workspace, &final_capacity,
                                           &error));
        CHECK(final_capacity == capacity);
        ordered = &object_table;
    }
    for (size_t index = 0; index < count; ++index) {
        char expected[128];
        CHECK(fscanf(file, "%127s", expected) == 1);
        AbSpan actual = ordered->entries[indices[index]].key.string;
        CHECK(actual.size == strlen(expected));
        CHECK(memcmp(actual.data, expected, actual.size) == 0);
    }
    CHECK(fgetc(file) == '\n' && fgetc(file) == EOF && !ferror(file));
    printf("All %zu native level object keys match %s order.\n", count,
           created ? "the created runtime table" : "constructor iteration");
    return true;
}

static bool level_checks(const char *bytecode, const char *reference, bool created) {
    AbBuffer input = {0};
    AbLuaChunk chunk = {0};
    AbLuaData data = {0};
    AbError error = {0};
    FILE *file = NULL;
    bool okay = false;
    if (!ab_file_read(bytecode, 8 * 1024 * 1024, &input, &error) ||
        !ab_lua_read((AbSpan){input.data, input.size}, 8 * 1024 * 1024, &chunk,
                     &error) ||
        !ab_lua_data(&chunk, 8 * 1024 * 1024, &data, &error)) {
        fprintf(stderr, "Level order input: %s\n", error.message);
        goto release_data;
    }
    file = fopen(reference, "rb");
    if (!file) {
        perror(reference);
        goto release_data;
    }
    okay = compare_level_order(file, &data, created);
release_data:
    if (file && fclose(file) != 0) {
        okay = false;
    }
    ab_lua_data_close(&data);
    ab_lua_close(&chunk);
    ab_buffer_free(&input);
    return okay;
}

static bool compare_constructor_keys(FILE *file, const AbDataTable *world,
                                     const size_t *indices, size_t item) {
    for (size_t entry = 0; entry < world->count; ++entry) {
        uint32_t length;
        uint8_t expected[128];
        CHECK(fscanf(file, "%8" SCNx32, &length) == 1 && length <= sizeof(expected));
        for (size_t offset = 0; offset < length; offset += 4) {
            uint32_t word;
            CHECK(fscanf(file, "%8" SCNx32, &word) == 1);
            for (size_t byte = 0; byte < 4 && offset + byte < length; ++byte) {
                expected[offset + byte] = (uint8_t)(word >> (byte * 8));
            }
        }
        CHECK(fgetc(file) == '\n');
        AbSpan actual = world->entries[indices[entry]].key.string;
        if (actual.size != length ||
            (length && memcmp(actual.data, expected, length))) {
            fprintf(stderr, "Native level %zu constructor key %zu differs\n", item,
                    entry);
            return false;
        }
    }
    return true;
}

static bool compare_constructor(FILE *file, const AbLuaData *data,
                                const uint32_t controls[5], size_t item) {
    const AbDataValue *world = ab_data_get(
        data->globals,
        (AbDataValue){.kind = AB_DATA_STRING, .string = {(const uint8_t *)"world", 5}});
    if (!controls[1]) {
        CHECK(!world || world->kind == AB_DATA_NIL);
        CHECK(!controls[2] && !controls[3] && !controls[4]);
        return true;
    }
    CHECK(world && world->kind == AB_DATA_TABLE && world->table &&
          world->table->count == controls[4]);
    const AbDataTableLayout *layout = ab_lua_table_layout(data, world->table);
    if (!layout || layout->hash_capacity != controls[2] ||
        layout->array_hint != controls[3] || !layout->no_nil_assignments) {
        fprintf(stderr,
                "Level %zu layout: hash %zu/%u array %zu/%u no-nil-assignments %u\n",
                item, layout ? layout->hash_capacity : 0, controls[2],
                layout ? layout->array_hint : 0, controls[3],
                layout ? layout->no_nil_assignments : false);
        return false;
    }
    size_t *indices = controls[4] ? malloc(controls[4] * sizeof(*indices)) : NULL;
    AbStringOrderNode *nodes =
        controls[2] ? malloc(controls[2] * sizeof(*nodes)) : NULL;
    bool okay = (!controls[4] || indices) && (!controls[2] || nodes);
    if (okay) {
        AbStringOrderWorkspace workspace = {indices, controls[4], nodes, controls[2]};
        AbError error = {0};
        okay = ab_data_string_order(world->table, layout->hash_capacity, &workspace,
                                    &error) &&
               compare_constructor_keys(file, world->table, indices, item);
        if (!okay && error.message[0]) {
            fprintf(stderr, "Level %zu: %s\n", item, error.message);
        }
    }
    free(indices);
    free(nodes);
    return okay;
}

static bool check_level_constructor(FILE *file, const char *directory, size_t item,
                                    size_t *total) {
    uint32_t controls[5];
    for (size_t field = 0; field < 5; ++field) {
        CHECK(fscanf(file, "%8" SCNx32, controls + field) == 1);
    }
    CHECK(fgetc(file) == '\n' && controls[0] == item && controls[1] <= 1 &&
          controls[2] <= 65536 && controls[4] <= 2048);
    char path[2048];
    int length = snprintf(path, sizeof(path), "%s/%03zu.lua", directory, item);
    CHECK(length >= 0 && (size_t)length < sizeof(path));
    AbBuffer input = {0};
    AbLuaChunk chunk = {0};
    AbLuaData data = {0};
    AbError error = {0};
    bool okay = ab_file_read(path, 8 * 1024 * 1024, &input, &error) &&
                ab_lua_read((AbSpan){input.data, input.size}, 8 * 1024 * 1024, &chunk,
                            &error) &&
                ab_lua_data(&chunk, 8 * 1024 * 1024, &data, &error);
    if (okay) {
        okay = compare_constructor(file, &data, controls, item);
    }
    if (!okay) {
        fprintf(stderr, "Level constructor order case %zu: %s\n", item, error.message);
    }
    ab_lua_data_close(&data);
    ab_lua_close(&chunk);
    ab_buffer_free(&input);
    *total += controls[4];
    return okay;
}

static bool native_level_checks(FILE *file, const char *directory) {
    char header[80];
    CHECK(fgets(header, sizeof(header), file));
    CHECK(strcmp(header, "AB_ANDROID_LEVEL_ORDER_V1 655\n") == 0);
    size_t total = 0;
    for (size_t item = 0; item < 655; ++item) {
        CHECK(check_level_constructor(file, directory, item, &total));
    }
    CHECK(fgets(header, sizeof(header), file));
    CHECK(strcmp(header, "AB level order reference complete\n") == 0);
    CHECK(fgetc(file) == EOF && !ferror(file));
    printf("All %zu native world keys and allocation hints across 655 levels match.\n",
           total);
    return true;
}

int main(int argc, char **argv) {
    bool created = argc == 4 && strcmp(argv[1], "--created-level") == 0;
    bool level = created || (argc == 4 && strcmp(argv[1], "--level") == 0);
    bool levels = argc == 4 && strcmp(argv[1], "--levels") == 0;
    if (argc > 2 && !level && !levels) {
        fprintf(
            stderr,
            "Usage: ab_data_order_tests [NATIVE_TRACE]\n"
            "       ab_data_order_tests --level LEVEL_BYTECODE NATIVE_ORDER\n"
            "       ab_data_order_tests --created-level LEVEL_BYTECODE RUNTIME_ORDER\n"
            "       ab_data_order_tests --levels BYTECODE_DIRECTORY NATIVE_ORDERS\n");
        return 2;
    }
    bool okay = edge_checks() && layout_checks();
    if (okay && level) {
        return level_checks(argv[2], argv[3], created) ? 0 : 1;
    }
    if (okay && (argc == 2 || levels)) {
        const char *path = levels ? argv[3] : argv[1];
        FILE *file = fopen(path, "rb");
        if (!file) {
            perror(path);
            return 1;
        }
        okay = levels ? native_level_checks(file, argv[2]) : native_checks(file);
        if (fclose(file) != 0) {
            okay = false;
        }
    }
    return okay ? 0 : 1;
}
