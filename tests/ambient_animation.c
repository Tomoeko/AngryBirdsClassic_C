#include "ab/ambient_animation.h"
#include "ab/ambient_animation_data.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Ambient animation failed at %d: %s\n", __LINE__,          \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static float from_bits(uint32_t bits) {
    float result;
    memcpy(&result, &bits, sizeof(result));
    return result;
}

static uint32_t to_bits(float number) {
    uint32_t result;
    memcpy(&result, &number, sizeof(result));
    return result;
}

static const char *const names[] = {"frame1", "frame2", "frame3", "frame4", "frame5",
                                    "frame6", "frame7", "frame8", "base"};

static AbSpan sprite(uint32_t code) {
    if (!code || code > 9) {
        return (AbSpan){0};
    }
    return (AbSpan){(const uint8_t *)names[code - 1], strlen(names[code - 1])};
}

static uint32_t sprite_code(AbSpan value, bool present) {
    if (!present) {
        return 0;
    }
    for (size_t index = 0; index < 9; ++index) {
        if (value.size == strlen(names[index]) &&
            memcmp(value.data, names[index], value.size) == 0) {
            return (uint32_t)index + 1;
        }
    }
    return UINT32_MAX;
}

typedef struct {
    AbSpan sprite;
    bool present, fail;
    uint32_t calls, last;
} Host;

static bool set_sprite(void *context, AbSpan value, bool present, AbError *error) {
    Host *host = context;
    ++host->calls;
    host->last = sprite_code(value, present);
    host->sprite = value;
    host->present = present;
    return !host->fail || ab_fail(error, "Test ambient sprite failure");
}

static bool edge_checks(void) {
    AbSpan frames[] = {sprite(1)};
    AbAmbientAnimationVariant variant = {.sprites = frames,
                                         .count = 1,
                                         .speed = 1,
                                         .delay = 0.5f,
                                         .delay_present = true};
    AbAmbientAnimationState state = {0};
    AbRandom48 random, expected;
    CHECK(ab_random48_seed(&random, 17));
    expected = random;
    uint32_t ignored;
    CHECK(ab_random48_next(&expected, &ignored));
    CHECK(ab_ambient_animation_start(&state, &variant, 1, sprite(9), true, &random,
                                     NULL) &&
          random.value == expected.value && state.current_present &&
          state.delay_duration == 0.5f && variant.timer_present && variant.timer == 0);
    Host host = {.sprite = sprite(9), .present = true};
    AbAmbientAnimationHost binding = {&host, set_sprite};
    AbAmbientAnimationResult result;
    variant.speed = NAN;
    CHECK(ab_ambient_animation_advance(&state, &variant, 1, 1, host.sprite,
                                       host.present, binding, &result, NULL) &&
          result == AB_AMBIENT_ANIMATION_WAITING && state.delay_timer == 1 &&
          variant.timer == 0 && !host.calls);
    variant.speed = 1;
    CHECK(ab_ambient_animation_advance(&state, &variant, 1, 0.25f, host.sprite,
                                       host.present, binding, &result, NULL) &&
          result == AB_AMBIENT_ANIMATION_PLAYING && host.calls == 1 && host.last == 1);
    CHECK(ab_ambient_animation_advance(&state, &variant, 1, 0, host.sprite,
                                       host.present, binding, &result, NULL) &&
          result == AB_AMBIENT_ANIMATION_PLAYING && host.calls == 1);
    CHECK(ab_ambient_animation_advance(&state, &variant, 1, 1, host.sprite,
                                       host.present, binding, &result, NULL) &&
          result == AB_AMBIENT_ANIMATION_FINISHED && host.calls == 2 &&
          host.last == 9 && !variant.timer_present && state.current_present);
    variant.delay_present = false;
    variant.min_delay = 0;
    variant.max_delay = 0;
    CHECK(ab_ambient_animation_start(&state, &variant, 1, host.sprite, host.present,
                                     &random, NULL));
    host.fail = true;
    CHECK(!ab_ambient_animation_advance(&state, &variant, 1, 0, host.sprite,
                                        host.present, binding, &result, NULL) &&
          !variant.timer_present && host.calls == 3);
    CHECK(
        !ab_ambient_animation_start(NULL, &variant, 1, sprite(9), true, &random, NULL));
    CHECK(!ab_ambient_animation_start(&state, &variant, 0, sprite(9), true, &random,
                                      NULL));
    return true;
}

static bool read_words(FILE *file, uint32_t *words, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, words + index) == 1);
    }
    CHECK(fgetc(file) == '\n');
    return true;
}

static AbDataValue data_string(const char *text) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)text, strlen(text)}};
}

static AbDataValue data_number(float value) {
    return (AbDataValue){.kind = AB_DATA_NUMBER, .bits = to_bits(value)};
}

static AbDataValue data_table(AbDataTable *table) {
    return (AbDataValue){.kind = AB_DATA_TABLE, .table = table};
}

static bool data_checks(void) {
    AbDataEntry frames[] = {{data_number(2), data_string("frame2")},
                            {data_number(1), data_string("frame1")}};
    AbDataTable sprites = {frames, 2, 2, NULL};
    AbDataEntry first_fields[] = {{data_string("speed"), data_number(1)},
                                  {data_string("delay"), data_number(0)},
                                  {data_string("sprites"), data_table(&sprites)}};
    AbDataEntry second_fields[] = {{data_string("speed"), data_number(2)},
                                   {data_string("minDelay"), data_number(4)},
                                   {data_string("maxDelay"), data_number(7)},
                                   {data_string("timer"), data_number(9)},
                                   {data_string("sprites"), data_table(&sprites)}};
    AbDataTable first = {first_fields, 3, 3, NULL};
    AbDataTable second = {second_fields, 5, 5, NULL};
    AbDataEntry entries[] = {{data_string("first"), data_table(&first)},
                             {data_string("second"), data_table(&second)}};
    AbDataTable component = {entries, 2, 2, NULL};
    size_t order[] = {1, 0};
    AbSpan output_names[4];
    AbAmbientAnimationVariant variants[2];
    AbAmbientAnimationStorage storage = {variants, 2, output_names, 4};
    AbAmbientAnimationConfiguration configuration;
    CHECK(ab_ambient_animation_data(&component, order, storage, &configuration, NULL) &&
          configuration.count == 2 && configuration.sprite_count == 4 &&
          variants[0].speed == 2 && variants[0].timer_present &&
          variants[0].timer == 9 && !variants[0].delay_present &&
          variants[0].min_delay == 4 && variants[1].speed == 1 &&
          !variants[1].timer_present && variants[1].delay_present &&
          variants[1].delay == 0 && variants[1].sprites == output_names + 2 &&
          sprite_code(variants[0].sprites[0], true) == 1);
    AbAmbientAnimationConfiguration saved;
    memcpy(&saved, &configuration, sizeof(saved));
    order[1] = 1;
    CHECK(
        !ab_ambient_animation_data(&component, order, storage, &configuration, NULL) &&
        memcmp(&saved, &configuration, sizeof(saved)) == 0);
    order[1] = 0;
    storage.sprite_capacity = 3;
    CHECK(!ab_ambient_animation_data(&component, order, storage, &configuration, NULL));
    storage.sprite_capacity = 4;
    first_fields[0].value = data_string("1");
    CHECK(!ab_ambient_animation_data(&component, order, storage, &configuration, NULL));
    component.count = 0;
    CHECK(ab_ambient_animation_data(&component, NULL, (AbAmbientAnimationStorage){0},
                                    &configuration, NULL) &&
          !configuration.count && !configuration.sprite_count);
    return true;
}

static bool native_checks(FILE *file) {
    char header[80];
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB_ANDROID_AMBIENT_ANIMATION_V1 4096\n") == 0);
    AbSpan frames[8];
    for (uint32_t index = 0; index < 8; ++index) {
        frames[index] = sprite(index + 1);
    }
    size_t setters = 0, starts = 0, waiting = 0, playing = 0, finished = 0;
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[25];
        CHECK(read_words(file, input, 25) && input[0] >= 1 && input[0] <= 4 &&
              input[3] <= 8 && input[4] <= 9);
        AbAmbientAnimationVariant variants[4] = {0};
        for (size_t index = 0; index < input[0]; ++index) {
            size_t base = 9 + index * 4;
            CHECK(input[base] <= 2);
            variants[index] =
                (AbAmbientAnimationVariant){.sprites = frames,
                                            .count = input[3],
                                            .speed = from_bits(input[2]),
                                            .delay_present = input[base] == 2,
                                            .delay = from_bits(input[base + 1]),
                                            .min_delay = from_bits(input[base + 2]),
                                            .max_delay = from_bits(input[base + 3]),
                                            .timer = (float)index + 7,
                                            .timer_present = true};
        }
        AbAmbientAnimationState state = {.delay_timer = 99,
                                         .delay_duration = 99,
                                         .original_sprite = sprite(9),
                                         .original_sprite_present = true};
        Host host = {.sprite = sprite(input[4]), .present = input[4] != 0};
        AbRandom48 random;
        CHECK(ab_random48_seed(&random, input[1]));
        for (size_t step = 0; step < 4; ++step) {
            uint32_t expected[12];
            CHECK(read_words(file, expected, 12));
            AbError error = {0};
            host.calls = host.last = 0;
            if (!state.current_present) {
                if (!ab_ambient_animation_start(&state, variants, input[0], host.sprite,
                                                host.present, &random, &error)) {
                    fprintf(stderr, "Ambient start %zu/%zu: %s\n", item, step,
                            error.message);
                    return false;
                }
                ++starts;
            }
            AbAmbientAnimationResult result;
            if (!ab_ambient_animation_advance(
                    &state, variants, input[0], from_bits(input[5 + step]), host.sprite,
                    host.present, (AbAmbientAnimationHost){&host, set_sprite}, &result,
                    &error)) {
                fprintf(stderr, "Ambient advance %zu/%zu: %s\n", item, step,
                        error.message);
                return false;
            }
            if (result == AB_AMBIENT_ANIMATION_FINISHED) {
                state.current_present = false;
                ++finished;
            } else if (result == AB_AMBIENT_ANIMATION_WAITING) {
                ++waiting;
            } else {
                ++playing;
            }
            uint32_t actual[12] = {
                state.current_present ? (uint32_t)state.current + 1 : 0,
                to_bits(state.delay_timer), to_bits(state.delay_duration),
                sprite_code(state.original_sprite, state.original_sprite_present)};
            for (size_t index = 0; index < input[0]; ++index) {
                if (variants[index].timer_present) {
                    actual[4] |= 1u << index;
                    actual[5 + index] = to_bits(variants[index].timer);
                }
            }
            actual[9] = host.calls;
            actual[10] = host.last;
            actual[11] = sprite_code(host.sprite, host.present);
            setters += host.calls;
            for (size_t field = 0; field < 12; ++field) {
                if (actual[field] != expected[field]) {
                    fprintf(stderr,
                            "Ambient case %zu/%zu field %zu: %08" PRIx32
                            " instead of %08" PRIx32 "\n",
                            item, step, field, actual[field], expected[field]);
                    return false;
                }
            }
        }
        uint32_t expected, actual;
        CHECK(read_words(file, &expected, 1) && ab_random48_next(&random, &actual) &&
              actual == expected);
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB ambient animation reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("All 4096 native sequences / 16384 updates match: %zu starts, %zu "
           "waiting, %zu playing, %zu finished, %zu sprite calls\n",
           starts, waiting, playing, finished, setters);
    return true;
}

int main(int argc, char **argv) {
    if (argc > 2 || !edge_checks() || !data_checks()) {
        return 1;
    }
    if (argc == 2) {
        FILE *file = fopen(argv[1], "rb");
        bool okay = file && native_checks(file);
        if (file && fclose(file) != 0) {
            okay = false;
        }
        return okay ? 0 : 1;
    }
    puts("Ambient delay crossing, sprite changes/restoration and failure checks pass");
    return 0;
}
