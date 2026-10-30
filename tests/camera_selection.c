#include "ab/camera_selection.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Camera selection failed at %d: %s\n", __LINE__,           \
                    #condition);                                                       \
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

static bool edge_checks(void) {
    AbCameraSnap snaps[] = {{{-10, 0}, 1}, {{10, 0}, 2}, {{10, 0}, 3}};
    size_t index = 99;
    CHECK(ab_camera_snap_closest(snaps, 3, (AbVec2){0}, &index) && index == 0);
    CHECK(ab_camera_snap_direction(snaps, 3, (AbVec2){0}, 1, &index) && index == 1);
    CHECK(ab_camera_snap_direction(snaps, 3, (AbVec2){0}, -0.0f, &index) && index == 0);
    CHECK(ab_camera_snap_direction(snaps, 3, (AbVec2){0}, NAN, &index) && index == 1);
    CHECK(ab_camera_snap_direction(snaps, 3, (AbVec2){-20, 0}, -1, &index) &&
          index == 0);
    CHECK(!ab_camera_snap_closest(NULL, 3, (AbVec2){0}, &index) && index == 0 &&
          !ab_camera_snap_closest(snaps, 0, (AbVec2){0}, &index) &&
          !ab_camera_snap_direction(snaps, 3, (AbVec2){NAN, 0}, 1, &index) &&
          !ab_camera_snap_closest(snaps, SIZE_MAX, (AbVec2){0}, &index));
    float value = 99;
    CHECK(ab_camera_relative_zoom(1, 2, 0, &value) && value == 0.5f);
    CHECK(ab_camera_snap_zoom(1, 2, 0, false, 0, &value) && value == 0);
    CHECK(ab_camera_snap_zoom(1.5f, 2, 0, false, NAN, &value) && value == 2);
    CHECK(ab_camera_snap_zoom(0.5f, 2, 0, false, 0, &value) && value == 0);
    CHECK(ab_camera_snap_zoom(1, 2, 0, false, INFINITY, &value) && value == 2);
    CHECK(ab_camera_snap_zoom(1, 2, 0, true, INFINITY, &value) && value == 1);
    CHECK(ab_camera_relative_zoom(0, 0, 0, &value) &&
          bits(value) == UINT32_C(0x7fc00000));
    CHECK(ab_camera_relative_zoom(1, 0, 0, &value) &&
          bits(value) == UINT32_C(0xff800000));
    value = 99;
    CHECK(!ab_camera_relative_zoom(NAN, 1, 0, &value) && value == 99 &&
          !ab_camera_snap_zoom(0, NAN, 0, false, 0, &value) && value == 99 &&
          !ab_camera_relative_zoom(0, 1, 0, NULL));
    puts("Camera nearest/directional snap ties, fallback, zoom thresholds, equal "
         "limits and invalid-input preservation pass.");
    return true;
}

static bool words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1 &&
              fgetc(file) == (index + 1 == count ? '\n' : ' '));
    }
    return true;
}

static void snap_words(const AbCameraSnap *snaps, size_t index, uint32_t output[4]) {
    output[0] = bits(snaps[index].position.x);
    output[1] = bits(snaps[index].position.y);
    output[2] = bits(snaps[index].zoom);
    output[3] = bits((float)(index + 1));
}

static bool compare_native(FILE *file) {
    char header[80];
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB_ANDROID_CAMERA_SELECTION_V1 4096\n") == 0);
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[11], expected[11], actual[11];
        CHECK(words(file, input, 11) && input[0] > 0 && input[0] <= 8 && input[6] <= 1);
        AbCameraSnap snaps[8];
        for (size_t index = 0; index < input[0]; ++index) {
            uint32_t values[3];
            CHECK(words(file, values, 3));
            snaps[index] = (AbCameraSnap){{number(values[0]), number(values[1])},
                                          number(values[2])};
        }
        CHECK(words(file, expected, 11));
        float relative, zoom = 0.0f;
        CHECK(ab_camera_relative_zoom(number(input[3]), number(input[4]),
                                      number(input[5]), &relative) &&
              ab_camera_snap_zoom(number(input[3]), number(input[4]), number(input[5]),
                                  input[6] != 0, number(input[7]), &zoom));
        actual[0] = bits(relative);
        actual[1] = bits(zoom);
        size_t selected;
        CHECK(ab_camera_snap_closest(
            snaps, input[0], (AbVec2){number(input[9]), number(input[10])}, &selected));
        snap_words(snaps, selected, actual + 2);
        actual[6] = bits((float)(selected + 1));
        CHECK(ab_camera_snap_direction(snaps, input[0],
                                       (AbVec2){number(input[1]), number(input[2])},
                                       number(input[7]), &selected));
        snap_words(snaps, selected, actual + 7);
        for (size_t field = 0; field < 11; ++field) {
            if (actual[field] != expected[field]) {
                fprintf(stderr,
                        "Camera selection case %zu field %zu: %08" PRIx32
                        " != %08" PRIx32 "\n",
                        item, field, actual[field], expected[field]);
                return false;
            }
        }
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB camera selection reference complete\n") == 0 &&
          fgetc(file) == EOF && !ferror(file));
    puts("All 4096 original nearest/directional snap, relative zoom and zoom-snap "
         "queries match, including ties, fallback and nonfinite comparison effects.");
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
