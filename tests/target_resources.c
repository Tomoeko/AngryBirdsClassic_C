#include "ab/target_resources.h"
#include "ab/target_data.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Target resource check failed at line %d: %s\n", __LINE__, \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static bool edge_checks(void) {
    AbZip empty = {0};
    uint8_t output[32], preserved[32];
    memset(output, 0x5a, sizeof(output));
    memcpy(preserved, output, sizeof(output));
    CHECK(!ab_target_resource_key(NULL, 16384, output, NULL));
    CHECK(!ab_target_resource_key(&empty, 16384, NULL, NULL));
    CHECK(!ab_target_resource_key(&empty, 16384, output, NULL));
    CHECK(memcmp(output, preserved, sizeof(output)) == 0);
    AbBuffer packed = {0};
    CHECK(!ab_target_packed_read(NULL, "missing", (AbSpan){output, 32}, 16384, &packed,
                                 NULL));
    CHECK(!ab_target_packed_read(&empty, "missing", (AbSpan){output, 31}, 16384,
                                 &packed, NULL));
    CHECK(!ab_target_packed_read(&empty, "missing", (AbSpan){output, 32}, 16384,
                                 &packed, NULL));
    CHECK(!packed.data && packed.size == 0);
    AbTargetData data;
    CHECK(!ab_target_data_open(&empty, "missing", (AbSpan){output, 32}, 16384, NULL,
                               &data, NULL));
    CHECK(!data.bytes.data && !data.chunk.allocated_bytes && !data.data.globals);
    ab_target_data_close(&data);
    ab_target_data_close(NULL);
    return true;
}

static bool compare_reference(const char *apk_path, const char *key_path) {
    AbBuffer apk = {0}, reference = {0};
    AbZip archive = {0};
    AbTargetData level = {0}, catalog = {0};
    AbError error = {0};
    uint8_t output[32], preserved[32];
    bool okay = false;
    if (!ab_file_read(apk_path, 128 * 1024 * 1024, &apk, &error) ||
        !ab_file_read(key_path, 32, &reference, &error) || reference.size != 32 ||
        !ab_zip_open((AbSpan){apk.data, apk.size}, &archive, &error)) {
        goto release_reference;
    }
    memset(output, 0x5a, sizeof(output));
    memcpy(preserved, output, sizeof(output));
    if (ab_target_resource_key(&archive, 1, output, &error) ||
        memcmp(output, preserved, sizeof(output)) != 0 ||
        !ab_target_resource_key(&archive, 16 * 1024 * 1024, output, &error) ||
        memcmp(output, reference.data, sizeof(output)) != 0) {
        goto release_reference;
    }
    if (!ab_target_data_open(&archive, "assets/data/levels/pack1/Level1.lua",
                             (AbSpan){output, sizeof(output)}, 8 * 1024 * 1024, NULL,
                             &level, &error) ||
        !ab_target_data_open(&archive, "assets/data/scripts/blocks.lua",
                             (AbSpan){output, sizeof(output)}, 8 * 1024 * 1024, NULL,
                             &catalog, &error)) {
        goto release_reference;
    }
    AbDataValue world_key = {.kind = AB_DATA_STRING,
                             .string = {(const uint8_t *)"world", 5}};
    const AbDataValue *world = ab_data_get(level.data.globals, world_key);
    if (!world || world->kind != AB_DATA_TABLE || !world->table ||
        world->table->count != 23 || !catalog.data.globals) {
        ab_fail(&error, "Direct APK data does not contain the expected level/catalog");
        goto release_reference;
    }
    puts(
        "Matched private resource key and loaded level/catalog data directly from APK");
    okay = true;

release_reference:
    if (!okay && error.message[0]) {
        fprintf(stderr, "%s\n", error.message);
    }
    ab_target_data_close(&catalog);
    ab_target_data_close(&level);
    ab_zip_close(&archive);
    ab_buffer_free(&reference);
    ab_buffer_free(&apk);
    return okay;
}

int main(int argc, char **argv) {
    if (!edge_checks()) {
        return 1;
    }
    if (argc == 1) {
        return 0;
    }
    return argc == 3 && compare_reference(argv[1], argv[2]) ? 0 : 1;
}
