#include "ab/lua_asset_sprite.h"
#include "ab/rotation.h"
#include "lua_asset_sprite_cases.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(value)                                                                   \
    do {                                                                               \
        if (!(value)) {                                                                \
            fprintf(stderr, "Lua asset sprite check at %d: %s\n", __LINE__, #value);   \
            return false;                                                              \
        }                                                                              \
    } while (0)

static bool run_case(const uint32_t input[16], uint32_t output[29]) {
    AbSpriteRegion region = {.width = (int16_t)(input[0] & 65535),
                             .height = (int16_t)(input[0] >> 16),
                             .pivot_x = (int16_t)asset_sprite_number(input[1]),
                             .pivot_y = (int16_t)asset_sprite_number(input[2])};
    AbLuaAssetSpritePose pose = {
        .world_position = {asset_sprite_number(input[3]),
                           asset_sprite_number(input[4])},
        .scale = {asset_sprite_number(input[5]), asset_sprite_number(input[6])},
        .angle_radians = asset_sprite_number(input[7]),
        .visible = input[12] != 0,
        .use_alpha = input[14] != 0,
        .alpha = asset_sprite_number(input[15])};
    AbCoordinateCamera camera = {
        .top_left = {asset_sprite_number(input[8]), asset_sprite_number(input[9])},
        .zoom = {asset_sprite_number(input[10]), asset_sprite_number(input[11])}};
    float nan_value = asset_sprite_number(UINT32_C(0x7fc00000));
    AbImageRectangle rectangle = {nan_value, nan_value, nan_value, nan_value};
    AbSpriteRenderState state = {0};
    float alpha = 0.25f;
    bool submit = false;
    CHECK(ab_lua_asset_sprite_state(input[13] ? &region : NULL, pose, camera,
                                    &rectangle, &state, &alpha, &submit, NULL));
    memset(output, 0, 29 * sizeof(*output));
    output[0] = submit;
    output[1] = asset_sprite_word(rectangle.x);
    output[2] = asset_sprite_word(rectangle.y);
    output[3] = asset_sprite_word(rectangle.width);
    output[4] = asset_sprite_word(rectangle.height);
    uint32_t *native_state = output + 5;
    native_state[16] = asset_sprite_word(alpha);
    if (submit) {
        AbRotation rotation;
        CHECK(ab_rotation_from_angle(state.angle_radians, &rotation));
        native_state[4] = native_state[7] = asset_sprite_word(rotation.cosine);
        native_state[5] = asset_sprite_word(rotation.sine);
        native_state[6] = asset_sprite_word(-rotation.sine);
        native_state[8] = asset_sprite_word(state.translation_x);
        native_state[9] = asset_sprite_word(state.translation_y);
        native_state[10] = asset_sprite_word(state.scale_x);
        native_state[11] = asset_sprite_word(state.scale_y);
        native_state[12] = native_state[14] =
            asset_sprite_word(state.rotation_origin_x);
        native_state[13] = native_state[15] =
            asset_sprite_word(state.rotation_origin_y);
        native_state[18] = asset_sprite_word(state.angle_radians);
    }
    return true;
}

static bool compare(FILE *file, const uint32_t *actual, size_t count, uint32_t sample) {
    for (size_t index = 0; index < count; ++index) {
        uint32_t expected;
        CHECK(fscanf(file, "%" SCNx32, &expected) == 1);
        if (actual[index] != expected) {
            fprintf(stderr, "Lua asset %u word %zu: %08" PRIx32 " != %08" PRIx32 "\n",
                    sample, index, actual[index], expected);
            return false;
        }
    }
    return true;
}

static bool corpus(FILE *file) {
    char header[96];
    CHECK(fgets(header, sizeof(header), file) &&
          !strcmp(header, "AB_ANDROID_LUA_ASSET_SPRITE_V1 4096\n"));
    size_t drawn = 0;
    for (uint32_t sample = 0; sample < LUA_ASSET_SPRITE_CASES; ++sample) {
        uint32_t input[16], output[29];
        asset_sprite_case(sample, input);
        CHECK(compare(file, input, 16, sample));
        CHECK(run_case(input, output) && compare(file, output, 29, sample));
        drawn += output[0];
    }
    CHECK(fgetc(file) == '\n' && fgets(header, sizeof(header), file) &&
          !strcmp(header, "AB Lua asset sprite reference complete\n") &&
          fgetc(file) == EOF && !ferror(file));
    printf("All 4096 native Lua asset sprite states/rectangles match: %zu draws\n",
           drawn);
    return true;
}

static bool controls(void) {
    AbSpriteRegion region = {.width = 10, .height = 12, .pivot_x = 5, .pivot_y = 6};
    AbLuaAssetSpritePose pose = {
        .world_position = {80.125f, -40.25f}, .scale = {2, 0.5f}, .visible = true};
    AbCoordinateCamera camera = {.top_left = {-20, -10}, .zoom = {0.5f, 0.75f}};
    AbImageRectangle rectangle = {0};
    AbSpriteRenderState state = {.direct_vertices = true};
    float alpha = 0.25f;
    bool submit = false;
    CHECK(ab_lua_asset_sprite_state(&region, pose, camera, &rectangle, &state, &alpha,
                                    &submit, NULL));
    CHECK(submit && rectangle.x == -5 && rectangle.y == -6 &&
          state.translation_x == 50.0625f && state.translation_y == -60.5f &&
          state.scale_x == 1 && state.scale_y == 0.375f && state.direct_vertices &&
          alpha == 1);
    AbSpriteRenderState saved = state;
    AbImageRectangle previous = rectangle;
    pose.scale.x = 0;
    CHECK(!ab_lua_asset_sprite_state(&region, pose, camera, &rectangle, &state, &alpha,
                                     &submit, NULL));
    CHECK(!memcmp(&state, &saved, sizeof(state)) &&
          !memcmp(&rectangle, &previous, sizeof(rectangle)) && submit && alpha == 1);
    pose.world_position.x = NAN;
    pose.visible = false;
    CHECK(ab_lua_asset_sprite_state(&region, pose, camera, &rectangle, &state, &alpha,
                                    &submit, NULL));
    CHECK(!submit && !memcmp(&state, &saved, sizeof(state)));
    pose.visible = true;
    CHECK(ab_lua_asset_sprite_state(NULL, pose, camera, &rectangle, &state, &alpha,
                                    &submit, NULL) &&
          !submit);
    return true;
}

int main(int argc, char **argv) {
    if (argc == 1)
        return controls() ? 0 : 1;
    if (argc != 2)
        return 2;
    FILE *file = fopen(argv[1], "rb");
    if (!file)
        return 1;
    bool okay = corpus(file);
    fclose(file);
    return okay ? 0 : 1;
}
