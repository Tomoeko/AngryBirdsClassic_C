#include "ab/level_flow.h"

#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Level bonus failed at line %d: %s\n", __LINE__,           \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static float number(uint32_t bits) {
    float result;
    memcpy(&result, &bits, sizeof(result));
    return result;
}

static uint32_t word(float value) {
    uint32_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static bool edge_checks(void) {
    AbLevelBonusState state = {
        .timer = 0, .bird_index = 2, .score_increment = 10000, .completed = true};
    AbLevelBonusGate output;
    CHECK(ab_level_bonus_gate(&state, 0, true, &output));
    CHECK(output.clear_eagle_darkness && !output.query_bird);
    state.timer = -0.1f;
    CHECK(ab_level_bonus_gate(&state, 0, false, &output));
    CHECK(output.query_bird && output.before_ending && output.update_score);
    CHECK(output.next.timer == state.timer);
    state.timer = NAN;
    output.next.timer = 19;
    CHECK(!ab_level_bonus_gate(&state, 0.1f, true, &output));
    CHECK(output.next.timer == 19);
    state.timer = -FLT_MAX;
    CHECK(!ab_level_bonus_gate(&state, FLT_MAX, true, &output));
    state.timer = -0.1f;
    state.bird_score = FLT_MAX;
    state.score_increment = FLT_MAX;
    CHECK(!ab_level_bonus_gate(&state, 0.1f, true, &output));
    state.eagle_launched_literal_true = true;
    CHECK(ab_level_bonus_gate(&state, 0.1f, true, &output));
    CHECK(!output.append_score && output.present_bird_effects);
    CHECK(!ab_level_bonus_gate(NULL, 0, true, &output));
    CHECK(!ab_level_bonus_gate(&state, 0, true, NULL));
    return true;
}

static bool read_words(FILE *file, uint32_t *output, size_t count) {
    for (size_t field = 0; field < count; ++field) {
        CHECK(fscanf(file, "%8" SCNx32, output + field) == 1);
    }
    CHECK(fgetc(file) == '\n');
    return true;
}

static bool native_checks(FILE *file) {
    char marker[80];
    CHECK(fgets(marker, sizeof(marker), file));
    CHECK(strcmp(marker, "AB_ANDROID_LEVEL_BONUS_V1 4096\n") == 0);
    size_t queries = 0, awards = 0, endings = 0, effects = 0;
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[9], expected[4], count;
        CHECK(read_words(file, input, 9) && read_words(file, expected, 4));
        for (size_t field = 5; field < 9; ++field) {
            CHECK(input[field] <= 4);
        }
        AbLevelBonusState state = {.timer = number(input[0]),
                                   .bird_index = number(input[1]),
                                   .bird_score = number(input[2]),
                                   .score_increment = number(input[3]),
                                   .skip_ending = input[5] >= 2,
                                   .completed = input[6] >= 2,
                                   .eagle_launched_literal_true = input[7] == 2};
        AbLevelBonusGate result;
        CHECK(ab_level_bonus_gate(&state, number(input[4]), input[8] != 0, &result));
        uint32_t actual[] = {word(result.next.timer), word(result.next.bird_index),
                             word(result.next.bird_score),
                             !result.clear_eagle_darkness};
        for (size_t field = 0; field < 4; ++field) {
            if (actual[field] != expected[field]) {
                fprintf(stderr,
                        "Native bonus %zu field %zu: %08" PRIx32 " != %08" PRIx32 "\n",
                        item, field, actual[field], expected[field]);
                return false;
            }
        }
        uint32_t events[8][2];
        size_t event_count = 0;
        if (result.query_bird) {
            events[event_count][0] = 1;
            events[event_count++][1] = word(result.queried_bird_index);
        }
        if (result.append_score) {
            events[event_count][0] = 2;
            events[event_count++][1] = input[3];
        }
        if (result.present_bird_effects) {
            for (uint32_t kind = 3; kind <= 6; ++kind) {
                events[event_count][0] = kind;
                events[event_count++][1] = kind == 6 ? input[3] : 0;
            }
        }
        if (result.before_ending) {
            events[event_count][0] = 7;
            events[event_count++][1] = input[4];
        }
        if (result.update_score) {
            events[event_count][0] = 8;
            events[event_count++][1] = input[4];
        }
        CHECK(read_words(file, &count, 1) && count == event_count);
        for (size_t event = 0; event < event_count; ++event) {
            uint32_t native[2];
            CHECK(read_words(file, native, 2));
            CHECK(memcmp(native, events[event], sizeof(native)) == 0);
        }
        queries += result.query_bird;
        awards += result.append_score;
        endings += result.before_ending;
        effects += result.present_bird_effects;
    }
    CHECK(fgets(marker, sizeof(marker), file));
    CHECK(strcmp(marker, "AB level bonus reference complete\n") == 0);
    CHECK(fgetc(file) == EOF && !ferror(file));
    printf("All 4096 original bonus prefixes match: %zu queries, %zu awards, "
           "%zu before-ending requests, %zu bird effects.\n",
           queries, awards, endings, effects);
    return true;
}

static bool live_checks(FILE *file) {
    char marker[80];
    CHECK(fgets(marker, sizeof(marker), file));
    CHECK(strcmp(marker, "AB_LEVEL_NATIVE_BONUS_V1 38 58\n") == 0);
    uint32_t seed[5];
    CHECK(read_words(file, seed, 5));
    AbLevelBonusState state = {.timer = number(seed[0]),
                               .bird_index = number(seed[1]),
                               .score_increment = number(seed[2]),
                               .completed = true};
    float base_score = number(seed[3]);
    float last_bird_index = number(seed[4]);
    size_t awards = 0;
    for (size_t frame = 38; frame < 96; ++frame) {
        uint32_t fields[5];
        CHECK(read_words(file, fields, 5));
        CHECK(fields[1] == 1 && fields[3] == 1);
        CHECK(word(base_score + state.bird_score) == fields[2]);
        AbLevelBonusGate result;
        CHECK(ab_level_bonus_gate(&state, number(fields[0]),
                                  state.bird_index <= last_bird_index, &result));
        state = result.next;
        if (result.append_score) {
            CHECK(frame == (awards == 0 ? 73u : 79u));
            ++awards;
        }
        CHECK(word(base_score + state.bird_score) == fields[4]);
    }
    CHECK(awards == 2 && base_score + state.bird_score == 31380);
    CHECK(fgets(marker, sizeof(marker), file));
    CHECK(strcmp(marker, "AB level native bonus complete\n") == 0);
    CHECK(fgetc(file) == EOF && !ferror(file));
    puts("All 58 native post-completion score boundaries match, including "
         "10,000-point awards at frames 73 and 79. Completion and queue are inputs.");
    return true;
}

int main(int argc, char **argv) {
    bool live = argc == 3 && strcmp(argv[1], "--live") == 0;
    if (argc > 2 && !live) {
        fprintf(stderr, "Usage: ab_level_bonus_tests [--live] [NATIVE_BONUS_TRACE]\n");
        return 2;
    }
    if (!edge_checks()) {
        return 1;
    }
    if (argc == 1) {
        return 0;
    }
    const char *path = argv[live ? 2 : 1];
    FILE *file = fopen(path, "rb");
    if (!file) {
        perror(path);
        return 1;
    }
    bool okay = live ? live_checks(file) : native_checks(file);
    if (fclose(file) != 0) {
        okay = false;
    }
    return okay ? 0 : 1;
}
