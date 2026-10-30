#include "ab/string_map.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "String map failed at %d: %s\n", __LINE__, #condition);    \
            okay = false;                                                              \
            goto close_map;                                                            \
        }                                                                              \
    } while (0)

static AbSpan key(const char *text) {
    return (AbSpan){(const uint8_t *)text, strlen(text)};
}

static bool edge_checks(void) {
    const char *const keys[] = {"root",  "overlay", "download", "dialog",
                                "pause", "input",   "loading",  "extra"};
    bool okay = true;
    AbStringMap *map = NULL;
    CHECK(ab_string_map_open(0, 8, 65536, &map, NULL));
    uintptr_t value;
    bool present;
    CHECK(ab_string_map_set(map, key(keys[0]), 0, true, NULL) &&
          ab_string_map_get(map, key(keys[0]), &value, &present, NULL) && present &&
          value == 0);
    CHECK(ab_string_map_set(map, key(keys[0]), 19, true, NULL) &&
          ab_string_map_get(map, key(keys[0]), &value, &present, NULL) && present &&
          value == 19);
    CHECK(ab_string_map_set(map, key(keys[0]), 0, false, NULL) &&
          ab_string_map_get(map, key(keys[0]), &value, &present, NULL) && !present);
    for (size_t index = 0; index < 8; ++index) {
        CHECK(ab_string_map_set(map, key(keys[index]), index, true, NULL));
    }
    AbStringMapLayout before, after;
    CHECK(ab_string_map_layout(map, &before) && before.count == 8 &&
          !ab_string_map_set(map, key("ninth"), 9, true, NULL) &&
          ab_string_map_layout(map, &after) && before.count == after.count &&
          before.capacity == after.capacity && before.last_free == after.last_free);
    for (size_t index = 0; index < 8; ++index) {
        CHECK(ab_string_map_get(map, key(keys[index]), &value, &present, NULL) &&
              present && value == index);
    }
    size_t cursor = 0, seen = 0;
    AbStringMapEntry entry;
    for (;;) {
        CHECK(ab_string_map_next(map, &cursor, &entry, NULL));
        if (!entry.present) {
            break;
        }
        CHECK(entry.value < 8 && !(seen & ((size_t)1 << entry.value)));
        seen |= (size_t)1 << entry.value;
        CHECK(ab_string_map_set(map, entry.key, 0, false, NULL));
    }
    CHECK(seen == 255 && ab_string_map_layout(map, &after) && !after.count);
    size_t retained = 0;
    for (size_t index = 0; index < after.capacity; ++index) {
        AbStringMapSlot slot;
        CHECK(ab_string_map_slot(map, index, &slot));
        retained += slot.key_present;
    }
    CHECK(retained == 8 && ab_string_map_reset(map, 3, NULL) &&
          ab_string_map_layout(map, &after) && after.capacity == 4 && !after.count);
    const uint8_t bytes[] = {'a', 0, 'b'};
    CHECK(ab_string_map_set(map, (AbSpan){bytes, sizeof(bytes)}, 31, true, NULL) &&
          ab_string_map_get(map, (AbSpan){bytes, sizeof(bytes)}, &value, &present,
                            NULL) &&
          present && value == 31 &&
          ab_string_map_get(map, key("a"), &value, &present, NULL) && !present);
    CHECK(!ab_string_map_set(map, (AbSpan){NULL, 1}, 1, true, NULL) &&
          !ab_string_map_reset(map, 9, NULL) &&
          !ab_string_map_get(map, key("root"), NULL, &present, NULL));
    puts("String map replace/delete/reinsert, present zero, erase-during-iteration, "
         "pinned keys, NUL bytes and capacity preservation pass.");
close_map:
    ab_string_map_close(map);
    return okay;
}

#define COMPARE(condition)                                                             \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Native string map failed at %d: %s\n", __LINE__,          \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static bool words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        COMPARE(fscanf(file, "%" SCNx32, output + index) == 1 &&
                fgetc(file) == (index + 1 == count ? '\n' : ' '));
    }
    return true;
}

static size_t key_id(AbSpan input, const AbSpan keys[64]) {
    for (size_t index = 0; index < 64; ++index) {
        if (input.size == keys[index].size &&
            (!input.size || memcmp(input.data, keys[index].data, input.size) == 0)) {
            return index;
        }
    }
    return SIZE_MAX;
}

static bool compare_snapshot(FILE *file, const AbStringMap *map, const AbSpan keys[64],
                             size_t *retained, size_t *dead, bool gc_mode) {
    uint32_t expected[3];
    AbStringMapLayout layout;
    COMPARE(words(file, expected, 3) && ab_string_map_layout(map, &layout));
    if (layout.count != expected[0] || layout.capacity != expected[1] ||
        layout.last_free != expected[2]) {
        fprintf(stderr, "String map layout %zu/%zu/%zu != %u/%u/%u\n", layout.count,
                layout.capacity, layout.last_free, expected[0], expected[1],
                expected[2]);
        return false;
    }
    for (size_t index = 0; index < layout.capacity; ++index) {
        uint32_t native[5] = {0};
        AbStringMapSlot slot;
        COMPARE(words(file, native, gc_mode ? 5 : 4) &&
                ab_string_map_slot(map, index, &slot));
        if (!gc_mode) {
            native[4] = native[0] ? 4 : 0;
        }
        size_t id = slot.key_present ? key_id(slot.entry.key, keys) : SIZE_MAX;
        COMPARE(!slot.key_present || id != SIZE_MAX);
        size_t key = slot.key_present ? id + 1 : 0;
        size_t next = slot.next == SIZE_MAX ? 0 : slot.next + 1;
        uint32_t kind = slot.key_present ? (slot.dead_key ? 11u : 4u) : 0;
        COMPARE(kind == native[4]);
        if (key != native[0] || slot.entry.present != (native[1] != 0) ||
            slot.entry.value != native[2] || next != native[3]) {
            fprintf(stderr, "String map slot %zu: %zu/%u/%zu/%zu != %u/%u/%u/%u\n",
                    index, key, slot.entry.present, (size_t)slot.entry.value, next,
                    native[0], native[1], native[2], native[3]);
            return false;
        }
        *retained += slot.key_present && !slot.entry.present;
        *dead += slot.dead_key;
    }
    for (size_t index = 0; index < 64; ++index) {
        uint32_t native[2];
        uintptr_t value;
        bool present;
        COMPARE(words(file, native, 2) &&
                ab_string_map_get(map, keys[index], &value, &present, NULL) &&
                present == (native[0] != 0) && value == native[1]);
    }
    size_t cursor = 0;
    AbStringMapEntry entry;
    for (size_t index = 0; index < layout.count; ++index) {
        uint32_t native[2];
        COMPARE(words(file, native, 2) &&
                ab_string_map_next(map, &cursor, &entry, NULL) && entry.present &&
                key_id(entry.key, keys) == native[0] && entry.value == native[1]);
    }
    COMPARE(ab_string_map_next(map, &cursor, &entry, NULL) && !entry.present);
    return true;
}

static bool compare_cases(FILE *file, AbStringMap *map, const AbSpan keys[64],
                          bool gc_mode) {
    size_t operations[8] = {0}, retained = 0, dead = 0, removed_total = 0;
    for (size_t instance = 0; instance < 16; ++instance) {
        uint32_t hint;
        COMPARE(words(file, &hint, 1) && ab_string_map_reset(map, hint, NULL));
        for (size_t step = 0; step < 256; ++step) {
            uint32_t command[3];
            COMPARE(words(file, command, 3) && command[0] < (gc_mode ? 8u : 6u) &&
                    command[1] < 64);
            uint32_t removed[64];
            size_t removed_count = 0;
            if (command[0] == 6) {
                COMPARE(ab_string_map_collect_nil_keys(map));
            } else if (command[0] == 5 || command[0] == 7) {
                size_t cursor = 0, visited = 0;
                AbStringMapEntry entry;
                for (;;) {
                    COMPARE(ab_string_map_next(map, &cursor, &entry, NULL));
                    if (!entry.present) {
                        break;
                    }
                    if (visited++ % 2 == 0) {
                        COMPARE(removed_count < 64);
                        removed[removed_count++] = (uint32_t)key_id(entry.key, keys);
                        COMPARE(ab_string_map_set(map, entry.key, 0, false, NULL));
                        if (command[0] == 7) {
                            COMPARE(ab_string_map_collect_nil_keys(map));
                        }
                    }
                }
            } else if (command[0] == 4) {
                COMPARE(ab_string_map_reset(map, command[1], NULL));
            } else if (command[0] != 3) {
                COMPARE(ab_string_map_set(map, keys[command[1]],
                                          command[0] == 0 ? command[2] : 0,
                                          command[0] != 1, NULL));
            }
            uint32_t native_removed, native_keys[64];
            COMPARE(words(file, &native_removed, 1) && native_removed == removed_count);
            if (removed_count) {
                COMPARE(words(file, native_keys, removed_count) &&
                        memcmp(native_keys, removed,
                               removed_count * sizeof(*removed)) == 0);
            }
            removed_total += removed_count;
            if (!compare_snapshot(file, map, keys, &retained, &dead, gc_mode)) {
                fprintf(stderr, "String map instance %zu step %zu op %u key %u\n",
                        instance, step, command[0], command[1]);
                return false;
            }
            ++operations[command[0]];
        }
    }
    char ending[80];
    COMPARE(fgets(ending, sizeof(ending), file) &&
            strcmp(ending, gc_mode ? "AB string map GC reference complete\n"
                                   : "AB string map reference complete\n") == 0 &&
            fgetc(file) == EOF && !ferror(file));
    printf("All 4096 native string map mutations, slot/link layouts, lookups and "
           "next orders match: %zu/%zu/%zu/%zu/%zu/%zu/%zu/%zu operations, %zu "
           "retained nil keys, %zu dead keys, %zu erasures during iteration.\n",
           operations[0], operations[1], operations[2], operations[3], operations[4],
           operations[5], operations[6], operations[7], retained, dead, removed_total);
    return true;
}

static bool compare_native(FILE *file) {
    char header[80];
    COMPARE(fgets(header, sizeof(header), file));
    bool gc_mode = strcmp(header, "AB_ANDROID_STRING_MAP_GC_V1 16 256 64\n") == 0;
    COMPARE(gc_mode || strcmp(header, "AB_ANDROID_STRING_MAP_V2 16 256 64\n") == 0);
    uint8_t bytes[64][256];
    AbSpan keys[64];
    for (size_t index = 0; index < 64; ++index) {
        uint32_t length, input[64];
        COMPARE(fscanf(file, "%" SCNx32, &length) == 1 && length <= 256 &&
                fgetc(file) == (length ? ' ' : '\n'));
        size_t count = (length + 3) / 4;
        if (count) {
            COMPARE(words(file, input, count));
        }
        for (size_t offset = 0; offset < length; ++offset) {
            bytes[index][offset] = (uint8_t)(input[offset / 4] >> ((offset % 4) * 8));
        }
        keys[index] = (AbSpan){bytes[index], length};
    }
    AbStringMap *map = NULL;
    COMPARE(ab_string_map_open(0, 128, 65536, &map, NULL));
    bool okay = compare_cases(file, map, keys, gc_mode);
    ab_string_map_close(map);
    return okay;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        return edge_checks() ? 0 : 1;
    }
    if (argc != 2) {
        return 2;
    }
    FILE *file = fopen(argv[1], "rb");
    if (!file) {
        return 1;
    }
    bool okay = compare_native(file);
    return fclose(file) == 0 && okay ? 0 : 1;
}
