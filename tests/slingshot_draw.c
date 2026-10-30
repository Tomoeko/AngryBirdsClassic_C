#include "ab/slingshot_draw.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Slingshot draw check failed at %d\n", __LINE__);          \
            return false;                                                              \
        }                                                                              \
    } while (0)

static uint32_t bits(float value) {
    uint32_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static float number(uint32_t value) {
    float result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static bool words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1 &&
              fgetc(file) == (index + 1 == count ? '\n' : ' '));
    }
    return true;
}

static void command_words(const AbSlingshotDrawCommand *command, uint32_t output[9]) {
    memset(output, 0, 9 * sizeof(*output));
    output[0] = (uint32_t)command->kind + 1;
    switch (command->kind) {
        case AB_SLING_COMMAND_STATE: {
            const AbSlingshotDrawState *state = &command->state;
            const float values[] = {state->translation.x, state->translation.y,
                                    state->scale.x,       state->scale.y,
                                    state->angle,         state->pivot.x,
                                    state->pivot.y};
            for (size_t field = 0; field < 7; ++field) {
                output[field + 1] = bits(values[field]);
            }
            break;
        }
        case AB_SLING_COMMAND_SPRITE:
            output[1] = (uint32_t)command->sprite.image;
            output[2] = bits(command->sprite.position.x);
            output[3] = bits(command->sprite.position.y);
            break;
        case AB_SLING_COMMAND_BAND:
            output[1] = bits(command->band.start.x);
            output[2] = bits(command->band.start.y);
            output[3] = bits(command->band.end.x);
            output[4] = bits(command->band.end.y);
            output[5] = bits(command->band.width);
            break;
        case AB_SLING_COMMAND_EXTRAS:
            break;
    }
}

static bool edges(void) {
    AbSlingshotDrawInput input = {.zoom = 1};
    AbSlingshotDrawPlan plan;
    memset(&plan, 0x5a, sizeof(plan));
    uint8_t retained[sizeof(plan)];
    memcpy(retained, &plan, sizeof(plan));
    AbError error = {0};
    CHECK(!ab_slingshot_draw_plan(NULL, AB_SLING_DRAW_BACK, &plan, &error));
    CHECK(!ab_slingshot_draw_plan(&input, AB_SLING_DRAW_BACK, NULL, &error));
    CHECK(!ab_slingshot_draw_plan(&input, (AbSlingshotDrawPart)-1, &plan, &error));
    input.position.x = NAN;
    CHECK(!ab_slingshot_draw_plan(&input, AB_SLING_DRAW_FRONT, &plan, &error));
    CHECK(memcmp(retained, &plan, sizeof(plan)) == 0);
    input.disabled = true;
    CHECK(ab_slingshot_draw_plan(&input, AB_SLING_DRAW_BACK, &plan, &error) &&
          plan.count == 0);
    input = (AbSlingshotDrawInput){.zoom = 1,
                                   .stretch_position = {NAN, INFINITY},
                                   .band_width = NAN,
                                   .band_angle = INFINITY,
                                   .holder_pivot = {NAN, NAN}};
    CHECK(ab_slingshot_draw_plan(&input, AB_SLING_DRAW_FRONT, &plan, &error) &&
          plan.count == 2);
    memcpy(retained, &plan, sizeof(plan));
    CHECK(!ab_slingshot_draw_plan(&input, AB_SLING_DRAW_HOLDER, &plan, &error) &&
          memcmp(retained, &plan, sizeof(plan)) == 0);
    input.stretch_position = (AbVec2){0};
    input.band_width = 1;
    CHECK(ab_slingshot_draw_plan(&input, AB_SLING_DRAW_HOLDER, &plan, &error) &&
          plan.count == 3);
    memcpy(retained, &plan, sizeof(plan));
    input.dragged = true;
    CHECK(!ab_slingshot_draw_plan(&input, AB_SLING_DRAW_HOLDER, &plan, &error) &&
          memcmp(retained, &plan, sizeof(plan)) == 0);
    CHECK(!ab_slingshot_sprite_name((AbSlingshotSprite)-1));
    const char *names[] = {"SLING_SHOT_01_BACK", "SLING_HOLDER", "SLING_SHOT_01_FRONT",
                           "SLINGSHOT_POWERUP_BASIC"};
    for (size_t index = 0; index < 4; ++index) {
        CHECK(strcmp(ab_slingshot_sprite_name((AbSlingshotSprite)index),
                     names[index]) == 0);
    }
    return true;
}

static bool reference(FILE *file) {
    char line[96];
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB_ANDROID_SLING_DRAW_V1 4096\n") == 0);
    size_t event_count = 0;
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[12];
        CHECK(words(file, input, 12) && input[11] <= 7);
        AbSlingshotDrawInput state = {
            .position = {number(input[0]), number(input[1])},
            .stretch_position = {number(input[2]), number(input[3])},
            .band_width = number(input[4]),
            .band_angle = number(input[5]),
            .camera_top_left = {number(input[6]), number(input[7])},
            .zoom = number(input[8]),
            .holder_pivot = {number(input[9]), number(input[10])},
            .disabled = (input[11] & 1) != 0,
            .dragged = (input[11] & 2) != 0,
            .has_scope = (input[11] & 4) != 0};
        for (size_t part = 0; part < 3; ++part) {
            uint32_t count, camera[3];
            CHECK(words(file, &count, 1) && words(file, camera, 3));
            const uint32_t expected_camera[3][3] = {{1, 2, 3}, {1, 2, 3}, {3, 1, 2}};
            const uint32_t disabled_camera[3] = {0};
            CHECK(memcmp(camera,
                         state.disabled ? disabled_camera : expected_camera[part],
                         sizeof(camera)) == 0);
            AbSlingshotDrawPlan plan;
            AbError error = {0};
            CHECK(ab_slingshot_draw_plan(&state, (AbSlingshotDrawPart)part, &plan,
                                         &error) &&
                  plan.count == count && count <= AB_SLING_DRAW_COMMANDS);
            for (size_t event = 0; event < plan.count; ++event) {
                uint32_t expected[9], actual[9];
                CHECK(words(file, expected, 9));
                command_words(plan.commands + event, actual);
                if (memcmp(actual, expected, sizeof(actual)) != 0) {
                    fprintf(stderr,
                            "Sling draw differs at case %zu part %zu event %zu\n", item,
                            part, event);
                    return false;
                }
                ++event_count;
            }
        }
    }
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB sling draw reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("4096 native sling states, 12288 part calls and %zu draw events match.\n",
           event_count);
    return true;
}

static bool rectangle_reference(FILE *file) {
    char line[96];
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB_ANDROID_SLING_RECTANGLE_V1 4096\n") == 0);
    const float uv[] = {0, 0, 1, 0, 0, 1, 1, 1};
    size_t rendered = 0, rotated = 0;
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[14], expected_visible = 0;
        CHECK(words(file, input, 14) && words(file, &expected_visible, 1) &&
              expected_visible <= 1);
        AbSpriteRegion region = {.width = (int16_t)(input[0] & 0xffffu),
                                 .height = (int16_t)(input[0] >> 16),
                                 .pivot_x = (int16_t)(int32_t)input[1],
                                 .pivot_y = (int16_t)(int32_t)input[2]};
        AbSlingshotDrawState state = {
            .translation = {number(input[5]), number(input[6])},
            .scale = {number(input[7]), number(input[8])},
            .angle = number(input[9]),
            .pivot = {number(input[10]), number(input[11])}};
        AbDrawQuad quad;
        bool visible = false;
        AbError error = {0};
        CHECK(ab_slingshot_sprite_quad(
                  &region, (AbVec2){number(input[3]), number(input[4])}, state,
                  input[12], input[13], uv, &quad, &visible, &error) &&
              visible == (expected_visible != 0));
        if (!visible) {
            continue;
        }
        uint32_t positions[18], coordinates[12];
        CHECK(words(file, positions, 18) && words(file, coordinates, 12));
        const size_t indices[] = {0, 1, 2, 2, 1, 3};
        for (size_t vertex = 0; vertex < 6; ++vertex) {
            AbDrawVertex point = quad.vertices[indices[vertex]];
            uint32_t actual[] = {bits(point.x), bits(point.y), 0x3a83126f,
                                 bits(point.u), bits(point.v)};
            uint32_t expected[] = {positions[vertex * 3], positions[vertex * 3 + 1],
                                   positions[vertex * 3 + 2], coordinates[vertex * 2],
                                   coordinates[vertex * 2 + 1]};
            if (memcmp(actual, expected, sizeof(actual)) != 0) {
                fprintf(stderr, "Sling rectangle differs at case %zu vertex %zu\n",
                        item, vertex);
                return false;
            }
        }
        ++rendered;
        rotated += state.angle != 0;
    }
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB sling rectangle reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf(
        "4096 native Resources/Sprite/GL_Image calls match: %zu drawn, %zu rotated.\n",
        rendered, rotated);
    return true;
}

int main(int argc, char **argv) {
    bool rectangles = argc == 3 && strcmp(argv[1], "--rectangles") == 0;
    if (argc > 3 || (argc == 3 && !rectangles)) {
        return 2;
    }
    bool okay = edges();
    if (okay && argc >= 2) {
        FILE *file = fopen(argv[argc - 1], "rb");
        okay = file && (rectangles ? rectangle_reference(file) : reference(file));
        if (file && fclose(file) != 0) {
            okay = false;
        }
    }
    return okay ? 0 : 1;
}
