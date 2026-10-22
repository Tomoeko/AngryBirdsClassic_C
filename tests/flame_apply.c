#include "ab/flame_data.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Flame apply failed at %d: %s\n", __LINE__, #condition);   \
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

static AbSpan span(const char *text) {
    return (AbSpan){(const uint8_t *)text, strlen(text)};
}

static AbDataValue string(const char *text) {
    return (AbDataValue){.kind = AB_DATA_STRING, .string = span(text)};
}

static AbDataValue number(float value) {
    return (AbDataValue){.kind = AB_DATA_NUMBER, .bits = to_bits(value)};
}

static AbDataValue table(AbDataTable *value) {
    return (AbDataValue){.kind = AB_DATA_TABLE, .table = value};
}

static uint32_t sprite_code(const AbFlameState *state) {
    const char *const names[] = {"frame1", "frame2", "frame3", "frame4",
                                 "frame5", "frame6", "frame7", "frame8"};
    if (!state->sprite_present) {
        return 0;
    }
    for (size_t index = 0; index < 8; ++index) {
        if (state->sprite.size == strlen(names[index]) &&
            memcmp(state->sprite.data, names[index], state->sprite.size) == 0) {
            return (uint32_t)index + 1;
        }
    }
    return UINT32_MAX;
}

typedef struct {
    const AbFlameDefinition *definition;
    AbFlameState *state;
    uint32_t initial[5];
    float draw_order;
    size_t updates, draws;
    bool edit, fail_update, fail_draw;
    AbFlameStep *body;
    float *z_order;
} Host;

static bool register_update(void *context, AbFlameState *state,
                            const AbFlameDefinition *definition, AbError *error) {
    Host *host = context;
    CHECK(!host->updates && !host->draws && definition == host->definition &&
          state == host->state);
    ++host->updates;
    host->initial[0] = to_bits(state->timer);
    host->initial[1] = to_bits(state->timer_speed);
    host->initial[2] = to_bits(state->scale);
    host->initial[3] = sprite_code(state);
    host->initial[4] = state->angle_present ? 4 : 0;
    CHECK(to_bits(state->new_angle) == 0 && to_bits(state->old_angle) == 0 &&
          to_bits(state->offset.x) == 0 && to_bits(state->offset.y) == 0);
    if (host->edit) {
        state->timer = 0.5f;
        host->body->body_angle = 3;
        *host->z_order = 8;
    }
    return !host->fail_update || ab_fail(error, "Test update registration failure");
}

static bool register_draw(void *context, AbFlameState *state, float z_order,
                          AbError *error) {
    Host *host = context;
    CHECK(host->updates == 1 && !host->draws && state == host->state &&
          state->angle_present);
    ++host->draws;
    host->draw_order = z_order;
    return !host->fail_draw || ab_fail(error, "Test draw registration failure");
}

static AbFlameHost callbacks(Host *host) {
    return (AbFlameHost){host, register_update, register_draw};
}

static bool edge_checks(void) {
    AbSpan sprites[] = {span("frame1"), span("frame2")};
    AbFlameDefinition definition = {.sprites = sprites, .count = 2};
    AbFlameState state = {0};
    AbFlameStep body = {.seconds = NAN};
    float z_order = 2;
    AbFlameCreation creation = {&definition, &body, &z_order, 1, 1};
    Host host = {.definition = &definition,
                 .state = &state,
                 .body = &body,
                 .z_order = &z_order,
                 .edit = true};
    AbRandom48 random;
    CHECK(ab_random48_seed(&random, 17));
    CHECK(ab_flame_apply(&state, &creation, &random, callbacks(&host), NULL) &&
          host.updates == 1 && host.draws == 1 && state.timer == 0.5f &&
          sprite_code(&state) == 2 && state.new_angle == 3 && host.draw_order == 9);
    CHECK(!ab_flame_apply(NULL, &creation, &random, callbacks(&host), NULL));
    CHECK(!ab_flame_apply(&state, NULL, &random, callbacks(&host), NULL));
    CHECK(!ab_flame_apply(&state, &creation, &random, (AbFlameHost){0}, NULL));
    uint64_t before = random.value;
    host = (Host){.definition = &definition, .state = &state, .fail_update = true};
    CHECK(!ab_flame_apply(&state, &creation, &random, callbacks(&host), NULL) &&
          host.updates == 1 && !host.draws && random.value == before &&
          !state.angle_present && state.timer == 0);
    host = (Host){.definition = &definition, .state = &state, .fail_draw = true};
    CHECK(!ab_flame_apply(&state, &creation, &random, callbacks(&host), NULL) &&
          host.updates == 1 && host.draws == 1 && random.value != before &&
          state.angle_present);
    return true;
}

static bool data_checks(void) {
    AbDataEntry frames[] = {{number(2), string("frame2")},
                            {number(1), string("frame1")}};
    AbDataTable sprites = {frames, 2, 2, NULL};
    AbDataEntry point_fields[] = {{string("x"), number(1)}, {string("y"), number(-2)}};
    AbDataTable point = {point_fields, 2, 2, NULL};
    AbDataEntry offset_fields[] = {{string("default"), table(&point)},
                                   {string("base"), table(&point)}};
    AbDataTable offsets = {offset_fields, 2, 2, NULL};
    AbDataEntry fields[] = {{string("sprites"), table(&sprites)},
                            {string("offset"), table(&offsets)},
                            {string("speed"), number(0)},
                            {string("scale"), {.kind = AB_DATA_BOOLEAN}}};
    AbDataTable component = {fields, 4, 4, NULL};
    AbSpan output_sprites[2];
    AbFlameOffset output_offsets[1];
    AbFlameStorage storage = {output_sprites, 2, output_offsets, 1};
    AbFlameConfiguration configuration;
    CHECK(ab_flame_data(&component, storage, &configuration, NULL) &&
          configuration.timer_speed == 0 && configuration.scale == 1 &&
          configuration.definition.count == 2 &&
          memcmp(configuration.definition.sprites[0].data, "frame1", 6) == 0 &&
          configuration.definition.default_present &&
          configuration.definition.default_offset.y == -2 &&
          configuration.definition.offset_count == 1);
    AbFlameConfiguration saved;
    memcpy(&saved, &configuration, sizeof(saved));
    frames[0].key = number(1);
    CHECK(!ab_flame_data(&component, storage, &configuration, NULL) &&
          memcmp(&saved, &configuration, sizeof(saved)) == 0);
    frames[0].key = number(2);
    fields[2].value = string("1");
    CHECK(!ab_flame_data(&component, storage, &configuration, NULL));
    fields[2].value = (AbDataValue){0};
    CHECK(ab_flame_data(&component, storage, &configuration, NULL) &&
          configuration.timer_speed == 1);
    storage.sprite_capacity = 1;
    CHECK(!ab_flame_data(&component, storage, &configuration, NULL));
    CHECK(!ab_flame_data(NULL, storage, &configuration, NULL));
    return true;
}

static bool read_words(FILE *file, uint32_t *words, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, words + index) == 1);
    }
    CHECK(fgetc(file) == '\n');
    return true;
}

static bool native_checks(FILE *file) {
    char header[80];
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB_ANDROID_FLAME_APPLY_V1 4096\n") == 0);
    const char *const names[] = {"frame1", "frame2", "frame3", "frame4",
                                 "frame5", "frame6", "frame7", "frame8"};
    AbSpan sprites[8];
    for (size_t index = 0; index < 8; ++index) {
        sprites[index] = span(names[index]);
    }
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[23], expected[20];
        CHECK(read_words(file, input, 23) && read_words(file, expected, 20) &&
              input[8] <= 8 && input[10] <= 2 && input[11] <= 3 && input[20] <= 2 &&
              input[21] <= 2);
        AbFlameOffset special = {span("base"),
                                 {from_bits(input[12]), from_bits(input[13])}};
        AbFlameDefinition definition = {
            .sprites = sprites,
            .count = input[8],
            .offsets = &special,
            .offset_count = input[11] & 1,
            .default_offset = {from_bits(input[14]), from_bits(input[15])},
            .default_present = (input[11] & 2) != 0};
        AbFlameState state = {.timer = from_bits(input[1]),
                              .angle = from_bits(input[3]),
                              .angle_present = true};
        AbFlameStep body = {.seconds = from_bits(input[0]),
                            .body_angle = from_bits(input[5]),
                            .velocity = {from_bits(input[6]), from_bits(input[7])},
                            .object_sprite = span(input[10] == 1 ? "base" : "other"),
                            .object_sprite_present = input[10] != 0};
        float z_order = from_bits(input[22]);
        AbFlameCreation creation = {&definition, &body, &z_order,
                                    input[20] == 2 ? from_bits(input[2]) : 1,
                                    input[21] == 2 ? from_bits(input[16]) : 1};
        Host host = {.definition = &definition, .state = &state};
        AbRandom48 random;
        CHECK(ab_random48_seed(&random, input[9]));
        AbError error = {0};
        if (!ab_flame_apply(&state, &creation, &random, callbacks(&host), &error)) {
            fprintf(stderr, "Native flame apply %zu failed: %s\n", item, error.message);
            return false;
        }
        CHECK(host.updates == 1 && host.draws == 1 && state.angle_present);
        uint32_t next;
        CHECK(ab_random48_next(&random, &next));
        uint32_t actual[] = {host.initial[0],
                             host.initial[1],
                             host.initial[2],
                             host.initial[3],
                             host.initial[4],
                             to_bits(state.timer),
                             sprite_code(&state),
                             to_bits(state.new_angle),
                             to_bits(state.old_angle),
                             to_bits(state.angle),
                             to_bits(state.offset.x),
                             to_bits(state.offset.y),
                             to_bits(state.scale),
                             to_bits(state.timer_speed),
                             next,
                             to_bits(host.draw_order),
                             host.draws == 1,
                             host.draws == 1,
                             host.definition == &definition,
                             host.updates == 1};
        for (size_t field = 0; field < 20; ++field) {
            if (actual[field] != expected[field]) {
                fprintf(stderr,
                        "Native flame apply %zu field %zu: %08" PRIx32
                        " instead of %08" PRIx32 "\n",
                        item, field, actual[field], expected[field]);
                return false;
            }
        }
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB flame apply reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    puts("All 4096 native flame construction, registration, initial update and RNG "
         "results match");
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
    puts("Flame registration order, zero-time update, borrowed data and failures pass");
    return 0;
}
