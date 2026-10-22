#include "ab/physics_broadphase.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Failed at line %d: %s\n", __LINE__, #condition);          \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    uint32_t pairs[2016][2];
    size_t count;
    bool invalid;
} PairResults;

static void record_pair(void *context, AbBroadPair pair, uintptr_t first,
                        uintptr_t second) {
    PairResults *results = context;
    if (results->count >= 2016 || pair.first < 0 || pair.first >= pair.second ||
        !first || first > 64 || !second || second > 64) {
        results->invalid = true;
        return;
    }
    results->pairs[results->count][0] = (uint32_t)first;
    results->pairs[results->count][1] = (uint32_t)second;
    ++results->count;
}

static bool edge_checks(void) {
    AbBroadPhase phase = {0};
    AbError error = {0};
    AbAabb box = {{0, 0}, {1, 1}};
    CHECK(!ab_broadphase_open((AbBroadLimits){16, 15, 16}, &phase, &error));
    CHECK(ab_broadphase_open((AbBroadLimits){16, 16, 16}, &phase, &error));
    int32_t proxies[7];
    CHECK(ab_broadphase_create(&phase, box, 1, proxies, &error));
    CHECK(ab_broadphase_create(&phase, box, 2, proxies + 1, &error));
    for (size_t index = 0; index < 14; ++index) {
        CHECK(ab_broadphase_touch(&phase, proxies[0], &error));
    }
    CHECK(phase.move_count == 16);
    CHECK(!ab_broadphase_touch(&phase, proxies[0], &error));
    CHECK(ab_broadphase_move(&phase, proxies[0], box, (AbVec2){10, 10}, &error));
    AbAabb previous = phase.tree.nodes[proxies[0]].bounds;
    CHECK(!ab_broadphase_move(&phase, proxies[0], (AbAabb){{10, 10}, {11, 11}},
                              (AbVec2){10, 10}, &error));
    CHECK(memcmp(&previous, &phase.tree.nodes[proxies[0]].bounds, sizeof(previous)) ==
          0);
    CHECK(ab_broadphase_destroy(&phase, proxies[0]));
    CHECK(phase.move_count == 16 && phase.proxy_count == 1);
    for (size_t index = 0; index < phase.move_count; ++index) {
        CHECK(phase.moves[index] == (index == 1 ? proxies[1] : -1));
    }
    PairResults results = {0};
    CHECK(ab_broadphase_update(&phase, record_pair, &results, &error));
    CHECK(phase.move_count == 0 && phase.pair_count == 0 && results.count == 0);
    CHECK(!ab_broadphase_destroy(&phase, proxies[0]));
    CHECK(!ab_broadphase_touch(&phase, -1, &error));
    CHECK(!ab_broadphase_update(&phase, NULL, &results, &error));
    ab_broadphase_close(&phase);
    CHECK(ab_broadphase_open((AbBroadLimits){16, 16, 16}, &phase, &error));
    for (size_t index = 0; index < 7; ++index) {
        CHECK(ab_broadphase_create(&phase, box, index + 1, proxies + index, &error));
    }
    CHECK(!ab_broadphase_update(&phase, record_pair, &results, &error));
    CHECK(phase.move_count == 7 && phase.pair_count == 16 && results.count == 0);
    for (size_t index = 3; index < 7; ++index) {
        CHECK(ab_broadphase_destroy(&phase, proxies[index]));
    }
    CHECK(ab_broadphase_update(&phase, record_pair, &results, &error));
    CHECK(phase.move_count == 0 && phase.pair_count == 6 && results.count == 3 &&
          !results.invalid);
    const uint32_t expected[][2] = {{1, 2}, {1, 3}, {2, 3}};
    CHECK(memcmp(results.pairs, expected, sizeof(expected)) == 0);
    ab_broadphase_close(&phase);
    ab_broadphase_close(&phase);
    return true;
}

static float from_bits(uint32_t bits) {
    float number;
    memcpy(&number, &bits, sizeof(number));
    return number;
}

static bool read_words(FILE *file, uint32_t *words, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%8" SCNx32, words + index) == 1);
    }
    CHECK(fgetc(file) == '\n');
    return true;
}

static bool compare_words(const uint32_t *actual, const uint32_t *expected,
                          size_t count, size_t operation, const char *section) {
    for (size_t field = 0; field < count; ++field) {
        if (actual[field] != expected[field]) {
            fprintf(stderr,
                    "Broadphase operation %zu %s field %zu: "
                    "%08" PRIx32 " != %08" PRIx32 "\n",
                    operation, section, field, actual[field], expected[field]);
            return false;
        }
    }
    return true;
}

static bool compare_state(FILE *file, const AbBroadPhase *phase,
                          const PairResults *results, size_t operation) {
    uint32_t expected[12];
    CHECK(read_words(file, expected, 12));
    const uint32_t actual[] = {
        (uint32_t)phase->tree.root,     (uint32_t)phase->tree.count,
        (uint32_t)phase->tree.capacity, (uint32_t)phase->tree.free_head,
        phase->tree.insertion_count,    (uint32_t)phase->proxy_count,
        (uint32_t)phase->move_capacity, (uint32_t)phase->move_count,
        (uint32_t)phase->pair_capacity, (uint32_t)phase->pair_count,
        (uint32_t)phase->query_proxy,   (uint32_t)results->count,
    };
    CHECK(compare_words(actual, expected, 12, operation, "state"));
    if (phase->move_count) {
        uint32_t moves[256], current[256];
        CHECK(phase->move_count <= 256 && read_words(file, moves, phase->move_count));
        for (size_t index = 0; index < phase->move_count; ++index) {
            current[index] = (uint32_t)phase->moves[index];
        }
        CHECK(compare_words(current, moves, phase->move_count, operation, "moves"));
    }
    for (size_t index = 0; index < phase->pair_count; ++index) {
        uint32_t pair[2];
        CHECK(read_words(file, pair, 2));
        const uint32_t current[] = {(uint32_t)phase->pairs[index].first,
                                    (uint32_t)phase->pairs[index].second};
        CHECK(compare_words(current, pair, 2, operation, "pairs"));
    }
    for (size_t index = 0; index < results->count; ++index) {
        uint32_t pair[2];
        CHECK(read_words(file, pair, 2));
        CHECK(
            compare_words(results->pairs[index], pair, 2, operation, "notifications"));
    }
    return true;
}

static bool native_checks(FILE *file, AbBroadPhase *phase) {
    char header[128];
    CHECK(fgets(header, sizeof(header), file));
    CHECK(strcmp(header, "AB_ANDROID_BROADPHASE_V1 4096\n") == 0);
    AbError error = {0};
    CHECK(ab_broadphase_open((AbBroadLimits){128, 256, 16384}, phase, &error));
    int32_t proxies[64];
    for (size_t index = 0; index < 64; ++index) {
        proxies[index] = -1;
    }
    size_t notifications = 0;
    for (size_t operation = 0; operation < 4096; ++operation) {
        uint32_t input[8], expected_result;
        CHECK(read_words(file, input, 8) && input[0] <= 6 && input[1] < 64);
        CHECK(read_words(file, &expected_result, 1));
        AbAabb bounds = {{from_bits(input[2]), from_bits(input[3])},
                         {from_bits(input[4]), from_bits(input[5])}};
        AbVec2 displacement = {from_bits(input[6]), from_bits(input[7])};
        size_t slot = input[1];
        uint32_t result = 0;
        PairResults results = {0};
        switch (input[0]) {
            case 0:
                CHECK(proxies[slot] == -1 &&
                      ab_broadphase_create(phase, bounds, slot + 1, proxies + slot,
                                           &error));
                result = (uint32_t)proxies[slot];
                break;
            case 1:
                CHECK(ab_broadphase_move(phase, proxies[slot], bounds, displacement,
                                         &error));
                break;
            case 2:
                CHECK(ab_broadphase_destroy(phase, proxies[slot]));
                proxies[slot] = -1;
                break;
            case 3:
                CHECK(ab_broadphase_touch(phase, proxies[slot], &error));
                break;
            case 4:
                CHECK(ab_broadphase_update(phase, record_pair, &results, &error));
                break;
            case 5:
                CHECK(ab_broadphase_shift(phase, displacement));
                break;
            default:
                break;
        }
        CHECK(!results.invalid);
        CHECK(compare_words(&result, &expected_result, 1, operation, "result"));
        CHECK(compare_state(file, phase, &results, operation));
        notifications += results.count;
    }
    CHECK(fgets(header, sizeof(header), file));
    CHECK(strcmp(header, "AB broadphase reference complete\n") == 0);
    CHECK(fgetc(file) == EOF && !ferror(file));
    printf("All 4096 broadphase states match; %zu ordered notifications.\n",
           notifications);
    return true;
}

int main(int argc, char **argv) {
    if (argc > 2) {
        fprintf(stderr, "Usage: ab_broadphase_tests [NATIVE_BROADPHASE_TRACE]\n");
        return 2;
    }
    if (!edge_checks()) {
        return 1;
    }
    if (argc == 1) {
        return 0;
    }
    FILE *file = fopen(argv[1], "rb");
    if (!file) {
        fprintf(stderr, "Cannot open native broadphase trace\n");
        return 1;
    }
    AbBroadPhase phase = {0};
    bool okay = native_checks(file, &phase);
    ab_broadphase_close(&phase);
    if (fclose(file) != 0) {
        okay = false;
    }
    return okay ? 0 : 1;
}
