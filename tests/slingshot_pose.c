#include "ab/slingshot_pose.h"

#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Slingshot pose check failed at %d\n", __LINE__);          \
            return false;                                                              \
        }                                                                              \
    } while (0)

static float number(uint32_t word) {
    float value;
    memcpy(&value, &word, sizeof(value));
    return value;
}

static uint32_t bits(float number_value) {
    uint32_t word;
    memcpy(&word, &number_value, sizeof(word));
    return word;
}

static bool words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1 &&
              fgetc(file) == (index + 1 == count ? '\n' : ' '));
    }
    return true;
}

static bool edges(void) {
    AbSlingshotPoseInput input = {.band_angle = 2000,
                                  .bird_radius = NAN,
                                  .bird_width = INFINITY,
                                  .physics_to_world = NAN};
    AbSlingshotPose output;
    AbError error = {0};
    CHECK(ab_slingshot_render_pose(&input, &output, &error));
    CHECK(output.angle == 2000 && output.width == 25);
    uint8_t retained[sizeof(output)];
    memcpy(retained, &output, sizeof(output));
    CHECK(!ab_slingshot_render_pose(NULL, &output, &error));
    CHECK(!ab_slingshot_render_pose(&input, NULL, &error));
    input.bird_present = true;
    CHECK(!ab_slingshot_render_pose(&input, &output, &error));
    input.radius_present = true;
    input.bird_radius = 1;
    input.physics_to_world = 20;
    CHECK(!ab_slingshot_render_pose(&input, &output, &error));
    CHECK(memcmp(retained, &output, sizeof(output)) == 0);
    input.band_angle = 0;
    CHECK(ab_slingshot_render_pose(&input, &output, &error));
    CHECK(output.stretch_position.x == -21 && output.stretch_position.y == 0);
    memcpy(retained, &output, sizeof(output));
    input.start_physics.x = FLT_MAX;
    CHECK(!ab_slingshot_render_pose(&input, &output, &error) &&
          memcmp(retained, &output, sizeof(output)) == 0);
    input.start_physics.x = 0;
    input.bird_radius = FLT_MAX;
    CHECK(!ab_slingshot_render_pose(&input, &output, &error) &&
          memcmp(retained, &output, sizeof(output)) == 0);
    input.bird_present = false;
    input.distance = 0x1p-149f;
    CHECK(ab_slingshot_render_pose(&input, &output, &error) && output.width == 25);
    return true;
}

static bool reference(FILE *file) {
    char line[96];
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB_ANDROID_SLING_POSE_V1 4096\n") == 0);
    size_t birds = 0, boxes = 0, minimum = 0, maximum = 0;
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[10], transforms[2][4], expected[6];
        CHECK(words(file, input, 10) && words(file, transforms[0], 4) &&
              words(file, transforms[1], 4) && words(file, expected, 6) &&
              input[9] <= 7);
        AbSlingshotPoseInput state = {
            .start_physics = {number(input[0]), number(input[1])},
            .band_physics = {number(input[2]), number(input[3])},
            .band_angle = number(input[4]),
            .distance = number(input[5]),
            .bird_radius = number(input[6]),
            .bird_width = number(input[7]),
            .physics_to_world = number(input[8]),
            .bird_present = (input[9] & 1) != 0,
            .radius_present = (input[9] & 2) != 0,
            .box = (input[9] & 4) != 0};
        AbSlingshotPose output;
        AbError error = {0};
        CHECK(ab_slingshot_render_pose(&state, &output, &error));
        uint32_t actual[] = {bits(output.position.x),
                             bits(output.position.y),
                             bits(output.stretch_position.x),
                             bits(output.stretch_position.y),
                             bits(output.angle),
                             bits(output.width)};
        if (memcmp(actual, expected, sizeof(actual)) != 0) {
            fprintf(stderr, "Slingshot render pose differs at case %zu\n", item);
            for (size_t field = 0; field < 6; ++field) {
                fprintf(stderr, "%zu: %08" PRIx32 " expected %08" PRIx32 "\n", field,
                        actual[field], expected[field]);
            }
            return false;
        }
        const size_t indices[] = {2, 0};
        for (size_t event = 0; event < 2; ++event) {
            size_t offset = indices[event];
            CHECK(transforms[event][0] == input[offset] &&
                  transforms[event][1] == input[offset + 1] &&
                  transforms[event][2] == bits(number(input[offset]) * 20) &&
                  transforms[event][3] == bits(number(input[offset + 1]) * 20));
        }
        birds += state.bird_present;
        boxes += state.bird_present && (!state.radius_present || state.box);
        minimum += output.width == 10;
        maximum += output.width == 25;
    }
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB sling pose reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("4096 native sling poses and 8192 native transforms match: %zu birds, "
           "%zu half-width selections, %zu minimum/%zu maximum widths.\n",
           birds, boxes, minimum, maximum);
    return true;
}

static bool start_edges(void) {
    AbSlingshotStartInput input = {.object_position = {NAN, INFINITY},
                                   .radius = NAN,
                                   .back_pivot_y = INFINITY,
                                   .back_height = NAN,
                                   .physics_scale = NAN};
    AbSlingshotStartPose output;
    memset(&output, 0x5a, sizeof(output));
    uint8_t retained[sizeof(output)];
    memcpy(retained, &output, sizeof(output));
    AbError error = {0};
    CHECK(ab_slingshot_start_pose(&input, &output, &error) &&
          memcmp(retained, &output, sizeof(output)) == 0);
    CHECK(!ab_slingshot_start_pose(NULL, &output, &error));
    CHECK(!ab_slingshot_start_pose(&input, NULL, &error));
    input.object_present = true;
    CHECK(!ab_slingshot_start_pose(&input, &output, &error) &&
          memcmp(retained, &output, sizeof(output)) == 0);
    input = (AbSlingshotStartInput){
        .object_present = true, .radius = NAN, .physics_scale = 0.05f};
    CHECK(ab_slingshot_start_pose(&input, &output, &error));
    CHECK(output.level_start.y == 0.2f && output.base.y == 0 &&
          output.band.x == -0.1f && output.band.y == 0.1f);
    memcpy(retained, &output, sizeof(output));
    input.radius_present = true;
    CHECK(!ab_slingshot_start_pose(&input, &output, &error) &&
          memcmp(retained, &output, sizeof(output)) == 0);
    return true;
}

static bool start_reference(FILE *file) {
    char line[96];
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB_ANDROID_SLING_START_POSE_V1 4096\n") == 0);
    size_t objects = 0, radii = 0;
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[7], retained[6], calls = 0, expected[6];
        CHECK(words(file, input, 7) && words(file, retained, 6) &&
              words(file, &calls, 1) && words(file, expected, 6) && input[6] <= 3);
        AbSlingshotStartInput state = {
            .object_position = {number(input[0]), number(input[1])},
            .radius = number(input[2]),
            .back_pivot_y = number(input[3]),
            .back_height = number(input[4]),
            .physics_scale = number(input[5]),
            .object_present = (input[6] & 1) != 0,
            .radius_present = (input[6] & 2) != 0};
        AbSlingshotStartPose output = {
            .level_start = {number(retained[0]), number(retained[1])},
            .base = {number(retained[2]), number(retained[3])},
            .band = {number(retained[4]), number(retained[5])}};
        AbError error = {0};
        CHECK(ab_slingshot_start_pose(&state, &output, &error));
        uint32_t actual[] = {bits(output.level_start.x), bits(output.level_start.y),
                             bits(output.base.x),        bits(output.base.y),
                             bits(output.band.x),        bits(output.band.y)};
        if (memcmp(actual, expected, sizeof(actual)) != 0 ||
            calls != (state.object_present ? 2u : 0u)) {
            fprintf(stderr, "Slingshot start pose differs at case %zu\n", item);
            return false;
        }
        if (!state.object_present) {
            CHECK(memcmp(actual, retained, sizeof(actual)) == 0);
        }
        objects += state.object_present;
        radii += state.object_present && state.radius_present;
    }
    CHECK(fgets(line, sizeof(line), file) &&
          strcmp(line, "AB sling start pose reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    printf("4096 native sling start poses match: %zu objects, %zu supplied radii, "
           "%zu ordered sprite queries.\n",
           objects, radii, objects * 2);
    return true;
}

int main(int argc, char **argv) {
    bool start = argc == 3 && strcmp(argv[1], "--start") == 0;
    if (argc > 3 || (argc == 3 && !start)) {
        return 2;
    }
    bool okay = edges() && start_edges();
    if (okay && argc >= 2) {
        FILE *file = fopen(argv[argc - 1], "rb");
        okay = file && (start ? start_reference(file) : reference(file));
        if (file && fclose(file) != 0) {
            okay = false;
        }
    }
    return okay ? 0 : 1;
}
