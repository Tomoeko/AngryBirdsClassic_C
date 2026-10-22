#include "ab/string_map.h"
#include "../tools/oracle/reference_world_iteration.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "World iteration check failed at %d: %s\n", __LINE__,      \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static AbSpan span(const char *name) {
    return (AbSpan){(const uint8_t *)name, strlen(name)};
}

static bool read_words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index)
        if (fscanf(file, "%" SCNx32, output + index) != 1)
            return false;
    return true;
}

static bool guards(void) {
    AbStringMap *map = NULL;
    CHECK(ab_string_map_open(2, 8, 4096, &map, NULL) &&
          ab_string_map_set(map, span("first"), 1, true, NULL) &&
          ab_string_map_set(map, span("second"), 2, true, NULL));
    size_t cursor = 0;
    AbStringMapEntry entry;
    CHECK(ab_string_map_next(map, &cursor, &entry, NULL) && entry.present &&
          ab_string_map_set(map, entry.key, 0, false, NULL) &&
          ab_string_map_resume(map, entry.key, &cursor, NULL) &&
          ab_string_map_collect_nil_keys(map) &&
          ab_string_map_resume(map, entry.key, &cursor, NULL));
    char copied[16];
    CHECK(entry.key.size < sizeof(copied));
    memcpy(copied, entry.key.data, entry.key.size);
    size_t unchanged = cursor;
    CHECK(!ab_string_map_resume(map, (AbSpan){(const uint8_t *)copied, entry.key.size},
                                &cursor, NULL) &&
          cursor == unchanged && ab_string_map_reset(map, 0, NULL) &&
          !ab_string_map_resume(map, entry.key, &cursor, NULL) && cursor == unchanged);
    ab_string_map_close(map);
    return true;
}

static bool compare(FILE *file) {
    char marker[96];
    CHECK(fgets(marker, sizeof(marker), file) &&
          !strcmp(marker, "AB_ANDROID_WORLD_ITERATION_V1 24 2\n"));
    size_t words = 0, visits = 0, invalid = 0;
    for (unsigned profile = 0; profile < WI_PROFILES; ++profile) {
        uint32_t header[3];
        CHECK(read_words(file, header, 3) && header[0] == profile &&
              header[1] == wi_hint(profile) && header[2] <= 2 * WI_MAX_STEPS);
        AbStringMap *map = NULL;
        CHECK(ab_string_map_open(wi_hint(profile), 64, 16384, &map, NULL));
        for (unsigned order = 0; order < wi_initial_count(profile); ++order) {
            unsigned key = wi_initial_key(profile, order);
            CHECK(ab_string_map_set(map, span(wi_keys[key]), key + 1, true, NULL));
        }
        unsigned count = 0;
        for (unsigned phase = 0; phase < 2; ++phase) {
            AbSpan previous = {0};
            bool started = false;
            for (unsigned step = 0; step < WI_MAX_STEPS; ++step) {
                uint32_t actual[WI_RECORD_WORDS] = {phase, step, 0, 0, 0, 0};
                size_t cursor = 0;
                bool okay =
                    !started || ab_string_map_resume(map, previous, &cursor, NULL);
                AbStringMapEntry entry = {0};
                if (okay)
                    CHECK(ab_string_map_next(map, &cursor, &entry, NULL));
                if (!okay) {
                    actual[2] = 2;
                    ++invalid;
                } else if (entry.present) {
                    unsigned key = 0;
                    while (key < WI_KEYS &&
                           (entry.key.size != strlen(wi_keys[key]) ||
                            memcmp(entry.key.data, wi_keys[key], entry.key.size)))
                        ++key;
                    CHECK(key < WI_KEYS && entry.value <= UINT32_MAX);
                    actual[2] = 1;
                    actual[3] = key + 1;
                    actual[4] = (uint32_t)entry.value;
                    ++visits;
                    if (!phase) {
                        WiAction actions[16];
                        unsigned count_actions =
                            wi_actions(profile, step, key, actions);
                        for (unsigned action = 0; action < count_actions; ++action) {
                            WiAction current = actions[action];
                            CHECK(current.kind == 1
                                      ? ab_string_map_collect_nil_keys(map)
                                      : ab_string_map_set(
                                            map, span(wi_keys[current.key]),
                                            current.value, current.value != 0, NULL));
                        }
                    }
                    CHECK(entry.value == actual[4]);
                    uintptr_t current = 0;
                    bool present = false;
                    CHECK(ab_string_map_get(map, entry.key, &current, &present, NULL) &&
                          current <= UINT32_MAX);
                    actual[5] = present ? (uint32_t)current : 0;
                    previous = entry.key;
                    started = true;
                }
                uint32_t expected[WI_RECORD_WORDS];
                CHECK(count < header[2] && read_words(file, expected, WI_RECORD_WORDS));
                for (size_t word = 0; word < WI_RECORD_WORDS; ++word) {
                    if (actual[word] != expected[word]) {
                        fprintf(stderr,
                                "World %u/%u/%u[%zu]: %08" PRIx32 " != %08" PRIx32 "\n",
                                profile, phase, step, word, actual[word],
                                expected[word]);
                        ab_string_map_close(map);
                        return false;
                    }
                }
                ++count;
                words += WI_RECORD_WORDS;
                if (!actual[2] || actual[2] == 2)
                    break;
                CHECK(step + 1 < WI_MAX_STEPS);
            }
        }
        CHECK(count == header[2]);
        words += 3;
        ab_string_map_close(map);
    }
    CHECK(fgetc(file) == '\n' && fgets(marker, sizeof(marker), file) &&
          !strcmp(marker, "AB world iteration reference complete\n") &&
          fgetc(file) == EOF);
    printf("Native world next:24 profiles/two passes, %zu visits/%zu invalid-key "
           "boundaries/%zu words match\n",
           visits, invalid, words);
    return true;
}

int main(int argc, char **argv) {
    if (!guards())
        return 1;
    if (argc == 1)
        return 0;
    if (argc != 2)
        return 2;
    FILE *file = fopen(argv[1], "r");
    if (!file)
        return 1;
    bool okay = compare(file);
    if (fclose(file))
        okay = false;
    return okay ? 0 : 1;
}
