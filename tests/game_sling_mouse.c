#include "ab/coordinates.h"
#include "ab/game_sling_mouse.h"

#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Mouse sling failed at line %d: %s\n", __LINE__,           \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static float number(uint32_t bits) {
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static uint32_t word(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static bool edge_checks(void) {
    AbSlingMouseState state = {0};
    AbSlingMouseInput input = {0};
    AbSlingMousePlan output;
    AbError error = {0};
    CHECK(ab_game_sling_mouse_plan(&state, &input, &output, &error));
    CHECK(output.count == 0);
    state.drag_started = state.in_game_pressed = true;
    input.held = true;
    memset(&output, 0xa5, sizeof(output));
    AbSlingMousePlan before = output;
    CHECK(!ab_game_sling_mouse_plan(&state, &input, &output, &error));
    CHECK(memcmp(&before, &output, sizeof(output)) == 0);
    input.pressed = true;
    input.touch_count = 1;
    input.bird_present = true;
    input.bird_position.x = 4;
    input.shoot_range = 4;
    input.zoom = 1;
    input.screen_width = 480;
    CHECK(ab_game_sling_mouse_plan(&state, &input, &output, &error));
    CHECK(output.count == 4 && !output.bird_dragged);
    CHECK(output.next.sample_mask == 1 && output.next.sample_index == 1);
    input.bird_position.x = 3;
    CHECK(ab_game_sling_mouse_plan(&state, &input, &output, &error));
    CHECK(output.bird_dragged && output.count == 6);
    input.pressed = false;
    input.held = false;
    input.bird_dragged = true;
    input.level_completed = true;
    CHECK(ab_game_sling_mouse_plan(&state, &input, &output, &error));
    CHECK(output.count == 1 && !output.next.drag_started &&
          output.calls[0].kind == AB_SLING_MOUSE_IS_DRAGGED);
    input.level_completed = false;
    input.pressed = input.held = true;
    input.cursor.x = FLT_MAX;
    input.cursor_physics.x = -FLT_MAX;
    before = output;
    CHECK(!ab_game_sling_mouse_plan(&state, &input, &output, &error));
    CHECK(memcmp(&before, &output, sizeof(output)) == 0);
    CHECK(!ab_game_sling_mouse_plan(NULL, &input, &output, &error));
    CHECK(!ab_game_sling_mouse_plan(&state, NULL, &output, &error));
    CHECK(!ab_game_sling_mouse_plan(&state, &input, NULL, &error));
    return true;
}

static bool read_words(FILE *file, uint32_t *output, size_t count) {
    for (size_t field = 0; field < count; ++field) {
        CHECK(fscanf(file, "%8" SCNx32, output + field) == 1);
    }
    CHECK(fgetc(file) == '\n');
    return true;
}

static AbVec2 vector(const uint32_t *words) {
    return (AbVec2){number(words[0]), number(words[1])};
}

static bool native_checks(FILE *file) {
    char marker[80];
    CHECK(fgets(marker, sizeof(marker), file));
    CHECK(strcmp(marker, "AB_ANDROID_GAME_SLING_MOUSE_V1 4160\n") == 0);
    for (size_t item = 0; item < 4160; ++item) {
        uint32_t data[36], expected[29], actual[29] = {0};
        CHECK(read_words(file, data, 36));
        CHECK(read_words(file, expected, 29));
        uint32_t flags = data[17];
        AbCoordinateCamera camera = {.top_left = vector(data + 34),
                                     .zoom = {number(data[14]), number(data[14])}};
        AbVec2 physics;
        CHECK(ab_coordinate_point(&camera, AB_SPACE_SCREEN, AB_SPACE_PHYSICS,
                                  vector(data + 1), &physics));
        AbSlingMouseState state = {.old_cursor = vector(data + 3),
                                   .start_physics = vector(data + 5),
                                   .start_screen = vector(data + 7),
                                   .sample_index = (uint32_t)number(data[18]),
                                   .sample_mask = 31,
                                   .drag_started = (flags & 1024) != 0,
                                   .in_game_pressed = (flags & 2048) != 0};
        for (size_t index = 0; index < AB_SLING_MOUSE_SAMPLES; ++index) {
            state.samples[index] = (AbSlingMouseSample){vector(data + 19 + index * 3),
                                                        number(data[21 + index * 3])};
        }
        AbSlingMouseInput input = {.cursor = vector(data + 1),
                                   .cursor_physics = physics,
                                   .bird_position = vector(data + 11),
                                   .rest = vector(data + 9),
                                   .seconds = number(data[0]),
                                   .touch_count = number(data[16]),
                                   .screen_width = number(data[15]),
                                   .zoom = number(data[14]),
                                   .shoot_range = number(data[13]),
                                   .pressed = (flags & 1) != 0,
                                   .held = (flags & 2) != 0,
                                   .released = (flags & 4) != 0,
                                   .skip_input = (flags & 8) != 0,
                                   .level_completed = (flags & 16) != 0,
                                   .bird_present = (flags & 32) != 0,
                                   .sling_disabled = (flags & 64) != 0,
                                   .bird_ready = (flags & 128) != 0,
                                   .sling_visible = (flags & 256) != 0,
                                   .bird_dragged = (flags & 512) != 0};
        AbError error = {0};
        AbSlingMousePlan plan;
        CHECK(ab_game_sling_mouse_plan(&state, &input, &plan, &error));
        const AbVec2 vectors[] = {physics, plan.next.old_cursor,
                                  plan.next.start_physics, plan.next.start_screen};
        for (size_t index = 0; index < 4; ++index) {
            actual[index * 2] = word(vectors[index].x);
            actual[index * 2 + 1] = word(vectors[index].y);
        }
        actual[8] = plan.next.drag_started;
        actual[9] = plan.next.in_game_pressed;
        actual[10] = plan.bird_dragged;
        actual[11] = word((float)plan.next.sample_index);
        actual[12] = plan.next.sample_mask;
        for (size_t index = 0; index < AB_SLING_MOUSE_SAMPLES; ++index) {
            actual[13 + index * 3] = word(plan.next.samples[index].displacement.x);
            actual[14 + index * 3] = word(plan.next.samples[index].displacement.y);
            actual[15 + index * 3] = word(plan.next.samples[index].seconds);
        }
        actual[28] = (uint32_t)plan.count;
        for (size_t field = 0; field < 29; ++field) {
            if (actual[field] != expected[field]) {
                fprintf(stderr,
                        "Native mouse case %zu field %zu: %08" PRIx32
                        " expected %08" PRIx32 "\n",
                        item, field, actual[field], expected[field]);
                return false;
            }
        }
        for (size_t index = 0; index < plan.count; ++index) {
            uint32_t event[6];
            CHECK(read_words(file, event, 6));
            const AbSlingMouseCall *call = plan.calls + index;
            const uint32_t result[] = {call->kind,           word(call->vector.x),
                                       word(call->vector.y), call->bird_dragged,
                                       call->drag_started,   call->in_game_pressed};
            if (memcmp(result, event, sizeof(event)) != 0) {
                fprintf(stderr, "Native mouse case %zu call %zu differs\n", item,
                        index);
                return false;
            }
        }
    }
    CHECK(fgets(marker, sizeof(marker), file));
    CHECK(strcmp(marker, "AB game sling mouse reference complete\n") == 0);
    CHECK(fgetc(file) == EOF && !ferror(file));
    puts("All 4160 original mouse sling input states and ordered boundaries match, "
         "including native point conversion and all five drag samples.");
    return true;
}

int main(int argc, char **argv) {
    if (argc > 2) {
        return 2;
    }
    if (argc == 1) {
        return edge_checks() ? 0 : 1;
    }
    FILE *file = fopen(argv[1], "rb");
    if (!file) {
        return 1;
    }
    bool okay = native_checks(file);
    if (fclose(file) != 0) {
        okay = false;
    }
    return okay ? 0 : 1;
}
