#include "ab/camera_bootstrap.h"
#include "ab/target_data.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Camera bootstrap failed at %d: %s\n", __LINE__,           \
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

static AbDataValue string(const char *text) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)text, strlen(text)}};
}

static AbDataValue numeric(float value) {
    return (AbDataValue){.kind = AB_DATA_NUMBER, .bits = bits(value)};
}

static AbDataValue table(AbDataTable *value) {
    return (AbDataValue){.kind = AB_DATA_TABLE, .table = value};
}

static bool edge_checks(void) {
    AbDataEntry fields[2][3] = {{{string("px"), numeric(0)},
                                 {string("py"), numeric(-384)},
                                 {string("sx"), numeric(1)}},
                                {{string("px"), numeric(1024)},
                                 {string("py"), numeric(-768)},
                                 {string("sx"), numeric(0.5f)}}};
    AbDataTable profiles[] = {{fields[0], 3, 3, NULL}, {fields[1], 3, 3, NULL}};
    AbDataEntry devices[] = {{string("ipad"), table(profiles)},
                             {string("ipad"), table(profiles + 1)}};
    AbDataTable groups[] = {{devices, 1, 1, NULL}, {devices + 1, 1, 1, NULL}};
    AbDataEntry entries[] = {{string("birdCameraData"), table(groups)},
                             {string("castleCameraData"), table(groups + 1)}};
    AbDataTable level = {entries, 2, 2, NULL};
    AbCameraSnap result[2] = {0};
    CHECK(ab_camera_museum_snaps(&level, (AbVec2){1024, 768}, result, NULL) &&
          result[0].position.x == 0 && result[0].position.y == -384 &&
          result[0].zoom == 1 && result[1].position.x == 1024 &&
          result[1].position.y == -768 && result[1].zoom == 0.5f);
    CHECK(ab_camera_museum_snaps(&level, (AbVec2){512, 384}, result, NULL) &&
          result[0].position.x == 0 && result[0].position.y == -384 &&
          result[0].zoom == 0.5f && result[1].zoom == 0.25f);
    CHECK(!ab_camera_museum_snaps(NULL, (AbVec2){512, 384}, result, NULL) &&
          !ab_camera_museum_snaps(&level, (AbVec2){512, 384}, NULL, NULL));
    entries[1].value.kind = AB_DATA_NIL;
    CHECK(!ab_camera_museum_snaps(&level, (AbVec2){1024, 768}, result, NULL) &&
          result[0].zoom == 0.5f && result[1].zoom == 0.25f);
    entries[1].value = table(groups + 1);
    fields[1][2].value = numeric(0);
    CHECK(!ab_camera_museum_snaps(&level, (AbVec2){1024, 768}, result, NULL) &&
          result[0].zoom == 0.5f && result[1].zoom == 0.25f);
    AbCameraSnap authored = {{0x1p40f, 0}, 1};
    CHECK(!ab_camera_snap_fit(&authored, (AbVec2){480, 320}, result) &&
          result[0].zoom == 0.5f);
    authored.position.x = 0;
    CHECK(!ab_camera_snap_fit(&authored, (AbVec2){0, 320}, result) &&
          !ab_camera_snap_fit(&authored, (AbVec2){480, NAN}, result) &&
          !ab_camera_snap_fit(NULL, (AbVec2){480, 320}, result) &&
          !ab_camera_snap_fit(&authored, (AbVec2){480, 320}, NULL));
    CHECK(ab_camera_snap_fit(&authored, (AbVec2){1024, 768}, &authored) &&
          authored.position.x == 0 && authored.position.y == 0 && authored.zoom == 1);
    puts("Museum camera profile selection, fit arithmetic, bounds and atomic output "
         "pass.");
    return true;
}

static bool words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%" SCNx32, output + index) == 1 &&
              fgetc(file) == (index + 1 == count ? '\n' : ' '));
    }
    return true;
}

static bool native_checks(FILE *file, const AbDataTable *data) {
    char header[80];
    uint32_t dimensions[2];
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB_ANDROID_CAMERA_BOOTSTRAP_V1 2\n") == 0 &&
          words(file, dimensions, 2));
    AbVec2 pixel_size = {number(dimensions[0]), number(dimensions[1])};
    AbCameraSnap snaps[2];
    CHECK(ab_camera_museum_snaps(data, pixel_size, snaps, NULL));
    for (size_t index = 0; index < 2; ++index) {
        uint32_t expected[4];
        CHECK(words(file, expected, 4) && expected[0] == bits((float)(index + 1)) &&
              bits(snaps[index].position.x) == expected[1] &&
              bits(snaps[index].position.y) == expected[2] &&
              bits(snaps[index].zoom) == expected[3]);
    }
    uint32_t expected[8];
    CHECK(words(file, expected, 8));
    AbCameraLimits limits;
    CHECK(ab_camera_limits_initialize(snaps, 2, pixel_size, 200, 200, &limits));
    const float actual[] = {limits.left,
                            limits.right,
                            limits.top,
                            limits.bottom,
                            limits.close_zoom,
                            limits.far_zoom,
                            limits.absolute_bottom,
                            limits.horizontal_leeway};
    for (size_t field = 0; field < 8; ++field) {
        CHECK(bits(actual[field]) == expected[field]);
    }
    CHECK(fgets(header, sizeof(header), file) &&
          strcmp(header, "AB camera bootstrap complete\n") == 0 && fgetc(file) == EOF &&
          !ferror(file));
    puts("Both current native level-one camera snaps and all eight limit fields match "
         "direct APK metadata loading.");
    return true;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        return edge_checks() ? 0 : 1;
    }
    if (argc != 3) {
        return 2;
    }
    AbBuffer bytes = {0};
    AbZip archive = {0};
    AbTargetData level = {0};
    AbError error = {0};
    uint8_t key[32] = {0};
    FILE *file = fopen(argv[2], "rb");
    bool okay = file && ab_file_read(argv[1], 64 * 1024 * 1024, &bytes, &error) &&
                ab_zip_open((AbSpan){bytes.data, bytes.size}, &archive, &error) &&
                ab_target_resource_key(&archive, 16 * 1024 * 1024, key, &error) &&
                ab_target_data_open(&archive, "assets/data/levels/pack1/Level1.lua",
                                    (AbSpan){key, sizeof(key)}, 1024 * 1024, NULL,
                                    &level, &error) &&
                native_checks(file, level.data.globals);
    memset(key, 0, sizeof(key));
    if (!okay && error.message[0]) {
        fprintf(stderr, "%s\n", error.message);
    }
    if (file && fclose(file) != 0) {
        okay = false;
    }
    ab_target_data_close(&level);
    ab_zip_close(&archive);
    ab_buffer_free(&bytes);
    return okay ? 0 : 1;
}
