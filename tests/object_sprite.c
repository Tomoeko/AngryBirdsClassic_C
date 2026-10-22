#include "ab/object_sprite.h"
#include "ab/rotation.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Object sprite check failed at line %d: %s\n", __LINE__,   \
                    #condition);                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

static float number(uint32_t bits) {
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static uint32_t raw(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static int16_t signed_half(uint32_t value) {
    value &= UINT32_C(0xffff);
    return (int16_t)(value < 32768 ? (int32_t)value : (int32_t)value - 65536);
}

static bool edge_checks(void) {
    AbSpriteRegion region = {.width = 5, .height = 7, .pivot_x = -3, .pivot_y = 4};
    AbObjectSpritePlacement placement = {.x = 100,
                                         .y = 200,
                                         .horizontal_multiplier = 1,
                                         .zoom = 2,
                                         .scale_x = 1,
                                         .scale_y = -1,
                                         .origin_x = 10,
                                         .origin_y = 20};
    AbDrawQuad quad;
    CHECK(ab_object_sprite_quad(&region, 256, 256, placement, &quad, NULL));
    CHECK(quad.vertices[0].x == 184 && quad.vertices[0].y == 368);
    CHECK(quad.vertices[3].x == 194 && quad.vertices[3].y == 354);
    AbDrawQuad preserved = quad;
    placement.zoom = INFINITY;
    CHECK(!ab_object_sprite_quad(&region, 256, 256, placement, &quad, NULL));
    CHECK(memcmp(&quad, &preserved, sizeof(quad)) == 0);
    placement.zoom = 2;
    placement.angle_radians = 1e30f;
    CHECK(!ab_object_sprite_quad(&region, 256, 256, placement, &quad, NULL));
    CHECK(memcmp(&quad, &preserved, sizeof(quad)) == 0);
    placement.angle_radians = 0;
    CHECK(!ab_object_sprite_quad(&region, 0, 256, placement, &quad, NULL));
    CHECK(!ab_object_sprite_quad(NULL, 256, 256, placement, &quad, NULL));
    CHECK(!ab_object_sprite_quad(&region, 256, 256, placement, NULL, NULL));
    CHECK(memcmp(&quad, &preserved, sizeof(quad)) == 0);
    AbObjectSpritePose pose = {{10, 20}, 0, {2, -1}};
    AbCoordinateCamera camera = {{30, 40}, {0.5f, 2}};
    AbImageRectangle rectangle;
    AbSpriteRenderState state;
    CHECK(ab_object_sprite_state(&region, pose, camera, &rectangle, &state, NULL));
    CHECK(rectangle.x == 2 && rectangle.y == -4 && rectangle.width == 5 &&
          rectangle.height == 7 && state.translation_x == 85 &&
          state.translation_y == -360 && state.scale_x == 1 && state.scale_y == -2 &&
          state.rotation_origin_x == -2 && state.rotation_origin_y == 4);
    AbImageRectangle saved_rectangle = rectangle;
    AbSpriteRenderState saved_state = state;
    pose.scale.x = 0;
    CHECK(!ab_object_sprite_state(&region, pose, camera, &rectangle, &state, NULL));
    CHECK(memcmp(&rectangle, &saved_rectangle, sizeof(rectangle)) == 0 &&
          memcmp(&state, &saved_state, sizeof(state)) == 0);
    return true;
}

static bool read_words(FILE *file, uint32_t *words, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        CHECK(fscanf(file, "%8" SCNx32, words + index) == 1);
    }
    return true;
}

static bool compare_reference(FILE *file) {
    char header[80];
    CHECK(fgets(header, sizeof(header), file));
    CHECK(strcmp(header, "AB_ANDROID_OBJECT_SPRITE_V1 4096\n") == 0);
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[20], expected[20];
        CHECK(read_words(file, input, 20));
        CHECK(read_words(file, expected, 20));
        AbSpriteRegion region = {.width = signed_half(input[0]),
                                 .height = signed_half(input[0] >> 16),
                                 .pivot_x = (int16_t)number(input[1]),
                                 .pivot_y = (int16_t)number(input[2])};
        AbObjectSpritePlacement placement = {
            number(input[3]), number(input[4]),  number(input[5]),
            number(input[6]), number(input[7]),  number(input[8]),
            number(input[9]), number(input[10]), number(input[11])};
        AbDrawQuad quad;
        CHECK(ab_object_sprite_quad(&region, 256, 256, placement, &quad, NULL));
        for (size_t vertex = 0; vertex < 4; ++vertex) {
            uint32_t actual[] = {raw(quad.vertices[vertex].x),
                                 raw(quad.vertices[vertex].y), 0};
            for (size_t field = 0; field < 3; ++field) {
                uint32_t wanted = expected[vertex * 3 + field];
                if (actual[field] != wanted) {
                    fprintf(stderr,
                            "Case %zu vertex %zu field %zu: %08" PRIx32
                            " expected %08" PRIx32 "\n",
                            item, vertex, field, actual[field], wanted);
                    return false;
                }
            }
        }
        for (size_t component = 0; component < 8; ++component) {
            CHECK(expected[12 + component] == input[12 + component]);
        }
    }
    CHECK(fgetc(file) == '\n');
    CHECK(fgets(header, sizeof(header), file));
    CHECK(strcmp(header, "AB object sprite reference complete\n") == 0);
    CHECK(fgetc(file) == EOF && !ferror(file));
    puts("Matched 4096 original object sprite transforms and UV passthrough cases");
    return true;
}

static bool compare_state_reference(FILE *file) {
    char header[80];
    CHECK(fgets(header, sizeof(header), file));
    CHECK(strcmp(header, "AB_ANDROID_OBJECT_RENDER_V1 4096\n") == 0);
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[12], expected[17];
        CHECK(read_words(file, input, 12));
        CHECK(read_words(file, expected, 17));
        AbSpriteRegion region = {.width = signed_half(input[0]),
                                 .height = signed_half(input[0] >> 16),
                                 .pivot_x = (int16_t)number(input[1]),
                                 .pivot_y = (int16_t)number(input[2])};
        AbObjectSpritePose pose = {{number(input[3]), number(input[4])},
                                   number(input[5]),
                                   {number(input[6]), number(input[7])}};
        AbCoordinateCamera camera = {{number(input[8]), number(input[9])},
                                     {number(input[10]), number(input[11])}};
        AbImageRectangle rectangle;
        AbSpriteRenderState state;
        AbRotation rotation;
        CHECK(ab_object_sprite_state(&region, pose, camera, &rectangle, &state, NULL));
        CHECK(ab_rotation_from_angle(state.angle_radians, &rotation));
        const float values[] = {rectangle.x,
                                rectangle.y,
                                rectangle.width,
                                rectangle.height,
                                rotation.cosine,
                                rotation.sine,
                                -rotation.sine,
                                rotation.cosine,
                                state.translation_x,
                                state.translation_y,
                                state.scale_x,
                                state.scale_y,
                                state.rotation_origin_x,
                                state.rotation_origin_y,
                                1,
                                state.angle_radians};
        for (size_t field = 0; field < 17; ++field) {
            uint32_t actual = field < 16 ? raw(values[field]) : state.direct_vertices;
            if (actual != expected[field]) {
                fprintf(stderr,
                        "State case %zu field %zu: %08" PRIx32 " expected %08" PRIx32
                        "\n",
                        item, field, actual, expected[field]);
                return false;
            }
        }
    }
    CHECK(fgetc(file) == '\n');
    CHECK(fgets(header, sizeof(header), file));
    CHECK(strcmp(header, "AB object render reference complete\n") == 0);
    CHECK(fgetc(file) == EOF && !ferror(file));
    puts("Matched 4096 original ordinary object renderer states and rectangles");
    return true;
}

int main(int argc, char **argv) {
    if (!edge_checks()) {
        return 1;
    }
    if (argc == 1) {
        return 0;
    }
    bool state = argc == 3 && strcmp(argv[1], "--state") == 0;
    if (argc != 2 && !state) {
        return 2;
    }
    FILE *file = fopen(argv[state ? 2 : 1], "rb");
    if (!file) {
        return 1;
    }
    bool okay = state ? compare_state_reference(file) : compare_reference(file);
    if (fclose(file)) {
        okay = false;
    }
    return okay ? 0 : 1;
}
