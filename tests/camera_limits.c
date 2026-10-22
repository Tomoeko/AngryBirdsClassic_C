#include "ab/camera_limits.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Camera limits failed at %d: %s\n", __LINE__, #condition); \
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

static void values_words(AbCameraValues values, uint32_t output[4]) {
    output[0] = values.present;
    output[1] = values.present & AB_CAMERA_VALUE_X ? bits(values.position.x) : 0;
    output[2] = values.present & AB_CAMERA_VALUE_Y ? bits(values.position.y) : 0;
    output[3] = values.present & AB_CAMERA_VALUE_ZOOM ? bits(values.zoom) : 0;
}

static bool edge_checks(void) {
    AbCameraSnap snaps[] = {{{0, -50}, 1}, {{100, -50}, 1}};
    AbCameraLimits limits = {.snaps = snaps,
                             .snap_count = 2,
                             .left = 0,
                             .right = 100,
                             .top = -100,
                             .bottom = 0,
                             .close_zoom = 2,
                             .far_zoom = 0.5f,
                             .horizontal_leeway = 200,
                             .enabled = true};
    AbCameraLimitContext context = {{20, 20}, {25, -10}, 1, {0, 0}, 0};
    AbVec2 projected = {99, 99};
    CHECK(ab_camera_limits_project(&limits, (AbVec2){25, -10}, &projected) &&
          projected.x == 25 && projected.y == -50);
    CHECK(ab_camera_limits_project(&limits, (AbVec2){-10, 100}, &projected) &&
          projected.x == 0 && projected.y == -50);
    CHECK(ab_camera_limits_project(&limits, (AbVec2){100, 100}, &projected) &&
          projected.x == 100 && projected.y == -50);
    CHECK(ab_camera_limits_project(&limits, (AbVec2){110, 100}, &projected) &&
          projected.x == 100 && projected.y == -50);
    CHECK(!ab_camera_limits_project(NULL, (AbVec2){0, 0}, &projected) &&
          !ab_camera_limits_project(&limits, (AbVec2){NAN, 0}, &projected) &&
          projected.x == 100 && projected.y == -50);
    AbCameraValues values = {.zoom = 0.1f, .present = AB_CAMERA_VALUE_ZOOM};
    AbCameraValues result = {0};
    CHECK(ab_camera_limits_zoom(&limits, &values, &result) && result.zoom == 0.5f);
    values.zoom = 3;
    CHECK(ab_camera_limits_zoom(&limits, &values, &values) && values.zoom == 2);
    limits.far_zoom = 4;
    CHECK(ab_camera_limits_zoom(&limits, &values, &result) && result.zoom == 4);
    limits.far_zoom = 0.5f;
    values.zoom = NAN;
    CHECK(ab_camera_limits_zoom(&limits, &values, &result) &&
          result.zoom == limits.close_zoom &&
          ab_camera_limits_input(&limits, &context, &values, &result) &&
          result.zoom == limits.close_zoom);
    values.zoom = -INFINITY;
    CHECK(ab_camera_limits_zoom(&limits, &values, &result) &&
          result.zoom == limits.far_zoom);
    limits.enabled = false;
    values.zoom = NAN;
    CHECK(ab_camera_limits_input(&limits, NULL, &values, &result) &&
          isnan(result.zoom));
    limits.enabled = true;
    values = (AbCameraValues){{-5, 10}, 1, 7};
    CHECK(ab_camera_limits_position(&limits, &context, &values, true, &result) &&
          result.position.x == 10 && result.position.y == -10 && result.present == 7);
    context.pixel_size = (AbVec2){200, 200};
    values.position = (AbVec2){50, -50};
    CHECK(ab_camera_limits_position(&limits, &context, &values, true, &result) &&
          result.position.x == 50 && result.position.y == -50);
    context.pixel_size = (AbVec2){20, 20};
    context.current_position = (AbVec2){50, -50};
    context.original_globals = context.current_position;
    context.original_present = 3;
    values = (AbCameraValues){0};
    CHECK(ab_camera_limits_position(&limits, &context, &values, true, &result) &&
          result.present == 0);
    context.original_present = 0;
    CHECK(ab_camera_limits_position(&limits, &context, &values, true, &result) &&
          result.present == 3 && result.position.x == 50 && result.position.y == -50);
    context.current_position = (AbVec2){25, -10};
    CHECK(ab_camera_limits_input(&limits, &context, &values, &result) &&
          result.present == AB_CAMERA_VALUE_Y && result.position.y == -50);
    values = (AbCameraValues){{-1000, -10}, 1, 7};
    CHECK(ab_camera_limits_input(&limits, &context, &values, &result) &&
          result.position.x < 0 && result.position.x > -200 &&
          result.position.y == -50);
    limits.enabled = false;
    CHECK(ab_camera_limits_input(&limits, NULL, &values, &result) &&
          result.position.x == -1000 && result.position.y == -10 &&
          result.present == 7);
    limits.enabled = true;
    uint32_t saved[4], changed[4];
    values_words(result, saved);
    values.zoom = 0;
    CHECK(!ab_camera_limits_position(&limits, &context, &values, false, &result));
    values_words(result, changed);
    CHECK(memcmp(saved, changed, sizeof(saved)) == 0);
    CHECK(ab_camera_limits_input(&limits, &context, &values, &result) &&
          result.zoom == limits.far_zoom);
    values.present = 8;
    CHECK(!ab_camera_limits_zoom(&limits, &values, &result));
    CHECK(!ab_camera_limits_initialize(NULL, 2, (AbVec2){20, 20}, 200, 200, &limits) &&
          !ab_camera_limits_initialize(snaps, 0, (AbVec2){20, 20}, 200, 200, &limits) &&
          !ab_camera_limits_initialize(snaps, SIZE_MAX, (AbVec2){20, 20}, 200, 200,
                                       &limits));
    CHECK(ab_camera_limits_initialize(snaps, 1, (AbVec2){20, 20}, 200, 200, &limits) &&
          limits.left == -11 && limits.right == 11 && limits.close_zoom == 1 &&
          limits.enabled && limits.top > limits.bottom);
    snaps[0].zoom = 0;
    CHECK(!ab_camera_limits_initialize(snaps, 1, (AbVec2){20, 20}, 200, 200, &limits) &&
          limits.left == -11);
    puts("Camera snap projection, strict/elastic limits, presence and bounds pass.");
    return true;
}

static bool words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1 &&
              fgetc(file) == (index + 1 == count ? '\n' : ' '));
    }
    return true;
}

static bool compare_native(FILE *file) {
    char header[80];
    CHECK(fgets(header, sizeof(header), file) &&
          (strcmp(header, "AB_ANDROID_CAMERA_LIMITS_V1 4096\n") == 0 ||
           strcmp(header, "AB_ANDROID_CAMERA_LIMITS_V2 4096\n") == 0));
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[17], expected[22], actual[22];
        CHECK(words(file, input, 17) && input[0] > 0 && input[0] <= 8 &&
              input[1] <= 7 && input[2] <= 3 && input[3] <= 1 && input[4] <= 1);
        AbCameraSnap snaps[8];
        for (size_t index = 0; index < input[0]; ++index) {
            uint32_t fields[3];
            CHECK(words(file, fields, 3));
            snaps[index] = (AbCameraSnap){{number(fields[0]), number(fields[1])},
                                          number(fields[2])};
        }
        CHECK(words(file, expected, 22));
        AbCameraLimitContext context = {
            .pixel_size = {number(input[5]), number(input[6])},
            .current_position = {number(input[7]), number(input[8])},
            .current_zoom = number(input[9]),
            .original_globals = {number(input[13]), number(input[14])},
            .original_present = input[2]};
        AbCameraValues values = {
            {number(input[10]), number(input[11])}, number(input[12]), input[1]};
        AbCameraLimits limits;
        CHECK(ab_camera_limits_initialize(snaps, input[0], context.pixel_size,
                                          number(input[15]), number(input[16]),
                                          &limits));
        limits.enabled = input[4] != 0;
        const float fields[] = {limits.left,
                                limits.right,
                                limits.top,
                                limits.bottom,
                                limits.close_zoom,
                                limits.far_zoom,
                                limits.absolute_bottom,
                                limits.horizontal_leeway};
        for (size_t field = 0; field < 8; ++field) {
            actual[field] = bits(fields[field]);
        }
        AbVec2 wanted = {
            values.present & AB_CAMERA_VALUE_X ? values.position.x
                                               : context.current_position.x,
            values.present & AB_CAMERA_VALUE_Y ? values.position.y
                                               : context.current_position.y};
        AbVec2 projected;
        CHECK(ab_camera_limits_project(&limits, wanted, &projected));
        actual[8] = bits(projected.x);
        actual[9] = bits(projected.y);
        AbCameraValues result;
        CHECK(ab_camera_limits_zoom(&limits, &values, &result));
        values_words(result, actual + 10);
        if ((values.present & AB_CAMERA_VALUE_ZOOM) &&
            (!isfinite(values.zoom) || values.zoom == 0)) {
            memset(actual + 14, 0, 4 * sizeof(*actual));
        } else {
            CHECK(ab_camera_limits_position(&limits, &context, &values, input[3] != 0,
                                            &result));
            values_words(result, actual + 14);
        }
        CHECK(ab_camera_limits_input(&limits, &context, &values, &result));
        values_words(result, actual + 18);
        for (size_t field = 0; field < 22; ++field) {
            if (actual[field] != expected[field]) {
                fprintf(stderr,
                        "Native camera limits case %zu field %zu: %08" PRIx32
                        " != %08" PRIx32 "\n",
                        item, field, actual[field], expected[field]);
                return false;
            }
        }
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB camera limits reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    puts("All 4096 native camera-limit initializations, snap projections, zoom clamps, "
         "hard positions and elastic input results match.");
    return true;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        return edge_checks() ? 0 : 1;
    }
    if (argc != 2) {
        return 2;
    }
    FILE *file = fopen(argv[1], "rb");
    bool okay = file && compare_native(file);
    if (file && fclose(file) != 0) {
        okay = false;
    }
    return okay ? 0 : 1;
}
