#include "ab/identity_map.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Identity map failed at %d: %s\n", __LINE__, #condition);  \
            okay = false;                                                              \
            goto close_map;                                                            \
        }                                                                              \
    } while (0)

static bool edge_checks(void) {
    bool okay = true, present;
    uintptr_t value;
    AbIdentityMap *map = NULL;
    CHECK(ab_identity_map_open(0, 8, 65536, &map, NULL));
    CHECK(ab_identity_map_set(map, 1, 0, true, NULL) &&
          ab_identity_map_get(map, 1, &value, &present, NULL) && present && !value);
    CHECK(ab_identity_map_set(map, 1, 19, true, NULL) &&
          ab_identity_map_get(map, 1, &value, &present, NULL) && value == 19);
    CHECK(ab_identity_map_set(map, 1, 0, false, NULL));
    for (size_t index = 0; index < 8; ++index)
        CHECK(ab_identity_map_set(map, 1 + index * 7, index, true, NULL));
    AbIdentityMapSlot before[8], after;
    AbIdentityMapLayout layout;
    CHECK(ab_identity_map_layout(map, &layout) && layout.capacity == 8 &&
          layout.count == 8);
    for (size_t index = 0; index < 8; ++index)
        CHECK(ab_identity_map_slot(map, index, before + index));
    CHECK(!ab_identity_map_set(map, 999, 99, true, NULL));
    for (size_t index = 0; index < 8; ++index) {
        CHECK(ab_identity_map_slot(map, index, &after));
        CHECK(after.entry.key == before[index].entry.key &&
              after.entry.value == before[index].entry.value &&
              after.entry.present == before[index].entry.present &&
              after.key_present == before[index].key_present &&
              after.next == before[index].next);
    }
    size_t cursor = 0, seen = 0;
    for (;;) {
        AbIdentityMapEntry entry;
        CHECK(ab_identity_map_next(map, &cursor, &entry, NULL));
        if (!entry.present)
            break;
        CHECK(entry.value < 8 && !(seen & ((size_t)1 << entry.value)));
        seen |= (size_t)1 << entry.value;
        CHECK(ab_identity_map_set(map, entry.key, 0, false, NULL) &&
              ab_identity_map_resume(map, entry.key, &cursor, NULL));
    }
    CHECK(seen == 255 && ab_identity_map_layout(map, &layout) && !layout.count);
    CHECK(!ab_identity_map_set(map, 0, 99, true, NULL) &&
          !ab_identity_map_reset(map, 9, NULL) && ab_identity_map_reset(map, 3, NULL));
#if UINTPTR_MAX > UINT32_MAX
    uintptr_t high = (UINT64_C(1) << 32) | 1;
    CHECK(ab_identity_map_set(map, 1, 11, true, NULL) &&
          ab_identity_map_set(map, high, 22, true, NULL) &&
          ab_identity_map_get(map, 1, &value, &present, NULL) && present &&
          value == 11 && ab_identity_map_get(map, high, &value, &present, NULL) &&
          present && value == 22);
#endif
    puts("Identity map: collisions, retained nil keys, present zero, capacity "
         "preservation, erase/resume and host identity equality pass.");
close_map:
    ab_identity_map_close(map);
    return okay;
}

#define COMPARE(condition)                                                             \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Native identity map failed at %d: %s\n", __LINE__,        \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static bool words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index)
        COMPARE(fscanf(file, "%" SCNx32, output + index) == 1 &&
                fgetc(file) == (index + 1 == count ? '\n' : ' '));
    return true;
}

static size_t key_id(uintptr_t key, const uintptr_t keys[64]) {
    for (size_t index = 0; index < 64; ++index)
        if (keys[index] == key)
            return index;
    return SIZE_MAX;
}

static bool snapshot(FILE *file, const AbIdentityMap *map, const uintptr_t keys[64],
                     size_t *retained) {
    uint32_t expected[3];
    AbIdentityMapLayout layout;
    COMPARE(words(file, expected, 3) && ab_identity_map_layout(map, &layout));
    if (layout.count != expected[0] || layout.capacity != expected[1] ||
        layout.last_free != expected[2]) {
        fprintf(stderr, "Identity layout %zu/%zu/%zu != %u/%u/%u\n", layout.count,
                layout.capacity, layout.last_free, expected[0], expected[1],
                expected[2]);
        return false;
    }
    for (size_t index = 0; index < layout.capacity; ++index) {
        uint32_t native[4];
        AbIdentityMapSlot slot;
        COMPARE(words(file, native, 4) && ab_identity_map_slot(map, index, &slot));
        size_t id = slot.key_present ? key_id(slot.entry.key, keys) : SIZE_MAX;
        COMPARE(!slot.key_present || id != SIZE_MAX);
        size_t key = slot.key_present ? id + 1 : 0;
        size_t next = slot.next == SIZE_MAX ? 0 : slot.next + 1;
        if (key != native[0] || slot.entry.present != (native[1] != 0) ||
            slot.entry.value != native[2] || next != native[3]) {
            fprintf(stderr, "Identity slot %zu: %zu/%u/%zu/%zu != %u/%u/%u/%u\n", index,
                    key, slot.entry.present, (size_t)slot.entry.value, next, native[0],
                    native[1], native[2], native[3]);
            return false;
        }
        *retained += slot.key_present && !slot.entry.present;
    }
    for (size_t index = 0; index < 64; ++index) {
        uint32_t native[2];
        uintptr_t value;
        bool present;
        COMPARE(words(file, native, 2) &&
                ab_identity_map_get(map, keys[index], &value, &present, NULL) &&
                present == (native[0] != 0) && value == native[1]);
    }
    size_t cursor = 0;
    AbIdentityMapEntry entry;
    for (size_t index = 0; index < layout.count; ++index) {
        uint32_t native[2];
        COMPARE(words(file, native, 2) &&
                ab_identity_map_next(map, &cursor, &entry, NULL) && entry.present &&
                key_id(entry.key, keys) == native[0] && entry.value == native[1]);
    }
    COMPARE(ab_identity_map_next(map, &cursor, &entry, NULL) && !entry.present);
    return true;
}

static bool compare_cases(FILE *file, AbIdentityMap *map, const uintptr_t keys[64]) {
    size_t retained = 0, erasures = 0;
    for (size_t instance = 0; instance < 16; ++instance) {
        uint32_t hint;
        COMPARE(words(file, &hint, 1) && ab_identity_map_reset(map, hint, NULL));
        for (size_t step = 0; step < 256; ++step) {
            uint32_t command[3], removed[64];
            COMPARE(words(file, command, 3) && command[0] < 6 && command[1] < 64);
            size_t removed_count = 0;
            if (command[0] == 5) {
                size_t cursor = 0, visited = 0;
                for (;;) {
                    AbIdentityMapEntry entry;
                    COMPARE(ab_identity_map_next(map, &cursor, &entry, NULL));
                    if (!entry.present)
                        break;
                    if (visited++ % 2 == 0) {
                        COMPARE(removed_count < 64);
                        removed[removed_count++] = (uint32_t)key_id(entry.key, keys);
                        COMPARE(ab_identity_map_set(map, entry.key, 0, false, NULL));
                    }
                }
            } else if (command[0] == 4) {
                COMPARE(ab_identity_map_reset(map, command[1], NULL));
            } else if (command[0] != 3) {
                COMPARE(ab_identity_map_set(map, keys[command[1]],
                                            command[0] == 0 ? command[2] : 0,
                                            command[0] != 1, NULL));
            }
            uint32_t native_count, native_removed[64];
            COMPARE(words(file, &native_count, 1) && native_count == removed_count);
            if (removed_count)
                COMPARE(
                    words(file, native_removed, removed_count) &&
                    !memcmp(native_removed, removed, removed_count * sizeof(*removed)));
            erasures += removed_count;
            if (!snapshot(file, map, keys, &retained)) {
                fprintf(stderr, "Identity map instance %zu step %zu op %u key %u\n",
                        instance, step, command[0], command[1]);
                return false;
            }
        }
    }
    char ending[80];
    COMPARE(fgets(ending, sizeof(ending), file) &&
            !strcmp(ending, "AB identity map reference complete\n") &&
            fgetc(file) == EOF && !ferror(file));
    printf("All 4096 native table-key map mutations, slots, collision links, "
           "lookups and next orders match; %zu retained nil keys and %zu "
           "erasures during traversal.\n",
           retained, erasures);
    return true;
}

static bool compare_native(FILE *file) {
    char header[80];
    COMPARE(fgets(header, sizeof(header), file) &&
            !strcmp(header, "AB_ANDROID_IDENTITY_MAP_V1 16 256 64\n"));
    uintptr_t keys[64];
    for (size_t index = 0; index < 64; ++index) {
        uint32_t pointer;
        COMPARE(words(file, &pointer, 1) && pointer);
        keys[index] = pointer;
        for (size_t previous = 0; previous < index; ++previous)
            COMPARE(keys[index] != keys[previous]);
    }
    AbIdentityMap *map = NULL;
    COMPARE(ab_identity_map_open(0, 128, 65536, &map, NULL));
    bool okay = compare_cases(file, map, keys);
    ab_identity_map_close(map);
    return okay;
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
