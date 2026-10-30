#include "ab/integer_map.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Integer map failed at %d: %s\n", __LINE__, #condition);   \
            return false;                                                              \
        }                                                                              \
    } while (0)

static bool edge_checks(void) {
    AbIntegerMap *map = NULL;
    CHECK(!ab_integer_map_open(3, 4096, &map, NULL) && !map &&
          !ab_integer_map_open(4, 1, &map, NULL) && !map &&
          ab_integer_map_open(4, 4096, &map, NULL));
    CHECK(ab_integer_map_bytes(map) <= 4096);
    for (int32_t key = 1; key <= 4; ++key)
        CHECK(ab_integer_map_set(map, key, (uintptr_t)key, true, NULL));
    CHECK(!ab_integer_map_set(map, 5, 5, true, NULL));
    AbIntegerMapLayout layout;
    CHECK(ab_integer_map_layout(map, &layout) && layout.count == 4 &&
          layout.array_capacity == 4 && !layout.hash_capacity);
    for (int32_t key = 1; key <= 4; ++key) {
        uintptr_t value;
        bool present;
        CHECK(ab_integer_map_get(map, key, &value, &present, NULL) && present &&
              value == (uintptr_t)key);
    }
    CHECK(ab_integer_map_reset(map, NULL));
    int32_t keys[] = {-16777216, 0, 16777216, -1};
    for (size_t index = 0; index < 4; ++index)
        CHECK(ab_integer_map_set(map, keys[index], index, true, NULL));
    CHECK(!ab_integer_map_set(map, 16777217, 1, true, NULL));
    size_t cursor = 0, count = 0;
    AbIntegerMapEntry entry;
    while (true) {
        CHECK(ab_integer_map_next(map, &cursor, &entry, NULL));
        if (!entry.present)
            break;
        CHECK(ab_integer_map_set(map, entry.key, 0, false, NULL));
        ++count;
    }
    CHECK(count == 4 && ab_integer_map_layout(map, &layout) && !layout.count &&
          layout.hash_capacity == 4);
    CHECK(ab_integer_map_set(map, 8, 77, true, NULL));
    uintptr_t value;
    bool present;
    CHECK(ab_integer_map_get(map, 8, &value, &present, NULL) && present && value == 77);
    CHECK(ab_integer_map_reset(map, NULL) && ab_integer_map_layout(map, &layout) &&
          !layout.count && !layout.array_capacity && !layout.hash_capacity);
    ab_integer_map_close(map);
    puts("Integer table exact-key bounds, array/hash limits, nil retention, deletion "
         "during iteration and fresh reset pass.");
    return true;
}

static bool words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index)
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1 &&
              fgetc(file) == (index + 1 == count ? '\n' : ' '));
    return true;
}

static bool compare_snapshot(FILE *file, const AbIntegerMap *map) {
    uint32_t expected[3];
    AbIntegerMapLayout layout;
    CHECK(words(file, expected, 3) && ab_integer_map_layout(map, &layout));
    if (layout.array_capacity != expected[0] || layout.hash_capacity != expected[1] ||
        layout.count != expected[2]) {
        fprintf(stderr, "Layout %zu/%zu/%zu != %u/%u/%u\n", layout.array_capacity,
                layout.hash_capacity, layout.count, expected[0], expected[1],
                expected[2]);
        return false;
    }
    size_t cursor = 0;
    for (size_t index = 0; index < layout.count; ++index) {
        uint32_t pair[2];
        AbIntegerMapEntry entry;
        CHECK(fscanf(file, "%" SCNx32 " %" SCNx32, pair, pair + 1) == 2 &&
              fgetc(file) == (index + 1 == layout.count ? '\n' : ' ') &&
              ab_integer_map_next(map, &cursor, &entry, NULL));
        uint32_t key;
        memcpy(&key, &entry.key, sizeof(key));
        CHECK(entry.present && key == pair[0] && entry.value == pair[1]);
    }
    return true;
}

static bool compare_native(FILE *file) {
    char header[80];
    CHECK(fgets(header, sizeof(header), file) &&
          !strcmp(header, "AB_ANDROID_INTEGER_MAP_V1 64 128\n"));
    AbIntegerMap *map = NULL, *copy = NULL;
    CHECK(ab_integer_map_open(256, 65536, &map, NULL) &&
          ab_integer_map_open(256, 65536, &copy, NULL));
    for (size_t item = 0; item < 64; ++item) {
        CHECK(ab_integer_map_reset(map, NULL));
        for (size_t step = 0; step < 128; ++step) {
            uint32_t operation[3];
            CHECK(words(file, operation, 3));
            int32_t key;
            memcpy(&key, operation, sizeof(key));
            CHECK(ab_integer_map_set(map, key, operation[2], operation[1] != 0, NULL));
            if (!compare_snapshot(file, map)) {
                fprintf(stderr, "Native mutation case %zu step %zu\n", item, step);
                return false;
            }
            CHECK(ab_integer_map_reset(copy, NULL));
            size_t cursor = 0;
            AbIntegerMapEntry entry;
            while (true) {
                CHECK(ab_integer_map_next(map, &cursor, &entry, NULL));
                if (!entry.present)
                    break;
                CHECK(ab_integer_map_set(copy, entry.key, entry.value, true, NULL));
            }
            if (!compare_snapshot(file, copy)) {
                fprintf(stderr, "Native shallow copy case %zu step %zu\n", item, step);
                return false;
            }
        }
    }
    CHECK(fgets(header, sizeof(header), file) &&
          !strcmp(header, "AB integer map reference complete\n") && fgetc(file) == EOF);
    ab_integer_map_close(copy);
    ab_integer_map_close(map);
    puts("8192 native continuing integer mutations and 8192 fresh shallow copies "
         "match layout, live values and traversal order.");
    return true;
}

int main(int argc, char **argv) {
    if (argc == 1)
        return edge_checks() ? 0 : 1;
    if (argc != 2)
        return 2;
    FILE *file = fopen(argv[1], "rb");
    if (!file)
        return 1;
    bool okay = compare_native(file);
    return fclose(file) == 0 && okay ? 0 : 1;
}
