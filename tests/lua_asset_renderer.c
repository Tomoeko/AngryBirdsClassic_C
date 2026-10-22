#include "ab/lua_asset_renderer.h"
#include "lua_asset_construct_cases.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(value)                                                                   \
    do {                                                                               \
        if (!(value)) {                                                                \
            fprintf(stderr, "Lua asset renderer check at %d: %s\n", __LINE__, #value); \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    AbSpriteRegion region;
    AbImage image;
    uint32_t queries;
    bool missing_texture;
} Resources;

static bool sprite(void *context, AbSpan name, AbSpriteAtlasBinding *output) {
    Resources *resources = context;
    ++resources->queries;
    if (name.size != 13 || memcmp(name.data, "TRAIL_WHITE_1", 13))
        return false;
    *output = (AbSpriteAtlasBinding){.sprite.region = &resources->region,
                                     .image = &resources->image,
                                     .alpha = AB_SPRITE_ALPHA_PRESENT};
    return true;
}

static bool texture(void *context, const AbImage *image, uint32_t *output) {
    Resources *resources = context;
    if (image != &resources->image || resources->missing_texture)
        return false;
    *output = 31;
    return true;
}

static uint32_t bits(float number) {
    uint32_t result;
    memcpy(&result, &number, sizeof(result));
    return result;
}

static float number(uint32_t word) {
    float result;
    memcpy(&result, &word, sizeof(result));
    return result;
}

static bool run_case(const uint32_t input[3], uint32_t output[55]) {
    Resources resources = {.region = {.width = 16, .height = 16},
                           .image = {.width = 64, .height = 64}};
    AbSpriteProvider provider = {
        .context = &resources, .sprite = sprite, .texture = texture};
    struct {
        float sentinel, position;
        AbLuaAssetRenderer renderer;
    } container = {.sentinel = 42, .position = 7.25f};
    uint32_t counter = input[0];
    AbSceneMember item;
    AbSceneMembership scene = {&item, 0, 1};
    AbSpan name =
        input[2] ? (AbSpan){(const uint8_t *)"TRAIL_WHITE_1", 13} : (AbSpan){0};
    CHECK(ab_lua_asset_renderer_create(&provider, name, number(input[1]), &counter,
                                       &scene, &container.renderer, NULL));
    AbLuaAssetRenderer *renderer = &container.renderer;
    CHECK(scene.count == 1 && item.object == renderer &&
          item.layer == renderer->layer && item.id == renderer->id.value);
    size_t count = 0;
    uint32_t prefix[] = {1,
                         4,
                         resources.queries,
                         counter,
                         (uint32_t)renderer->id.value,
                         bits(renderer->id.script_number),
                         (uint32_t)renderer->layer,
                         (uint32_t)scene.count,
                         renderer->sprite.region != NULL,
                         0};
    memcpy(output, prefix, sizeof(prefix));
    count += sizeof(prefix) / sizeof(*prefix);
    /* Native table/function tags describe the typed C projection. setSprite is
     * the C method, not a runtime Lua value or a generic function interpreter. */
    uint32_t fields[] = {5, 1,
                         5, 1,
                         5, 1,
                         3, bits(renderer->pose.angle_radians),
                         1, renderer->pose.visible,
                         1, renderer->pose.use_alpha,
                         3, bits(renderer->pose.alpha),
                         1, renderer->draw_string,
                         4, (uint32_t)renderer->string.size,
                         3, bits(renderer->string_scale),
                         6, 1};
    memcpy(output + count, fields, sizeof(fields));
    count += sizeof(fields) / sizeof(*fields);
    uint32_t vectors[] = {3, bits(renderer->pose.world_position.x),
                          3, bits(renderer->pose.world_position.y),
                          3, bits(renderer->pose.scale.x),
                          3, bits(renderer->pose.scale.y),
                          1, renderer->image_size_use,
                          3, bits(renderer->image_size_w),
                          3, bits(renderer->image_size_h),
                          0, 0,
                          0, 0};
    memcpy(output + count, vectors, sizeof(vectors));
    count += sizeof(vectors) / sizeof(*vectors);
    uint32_t tail[] = {1, 3, bits(container.sentinel), 3, bits(container.position)};
    memcpy(output + count, tail, sizeof(tail));
    count += sizeof(tail) / sizeof(*tail);
    CHECK(count == 55);
    return true;
}

static bool corpus(FILE *file) {
    char header[96];
    CHECK(fgets(header, sizeof(header), file) &&
          !strcmp(header, "AB_ANDROID_LUA_ASSET_CONSTRUCT_V1 4096\n"));
    for (uint32_t sample = 0; sample < 4096; ++sample) {
        uint32_t input[3], output[55], expected;
        asset_construct_case(sample, input);
        CHECK(run_case(input, output));
        for (size_t index = 0; index < 58; ++index) {
            uint32_t actual = index < 3 ? input[index] : output[index - 3];
            CHECK(fscanf(file, "%" SCNx32, &expected) == 1);
            if (actual != expected) {
                fprintf(stderr,
                        "Lua asset constructor %u word %zu: %08" PRIx32 " != %08" PRIx32
                        "\n",
                        sample, index, actual, expected);
                return false;
            }
        }
    }
    CHECK(fgetc(file) == '\n' && fgets(header, sizeof(header), file) &&
          !strcmp(header, "AB Lua asset constructor reference complete\n") &&
          fgetc(file) == EOF && !ferror(file));
    puts(
        "All 4096 original GameLua renderer constructors match the typed C projection");
    return true;
}

static bool controls(void) {
    Resources resources = {.region = {.width = 16, .height = 16},
                           .image = {.width = 64, .height = 64}};
    AbSpriteProvider provider = {
        .context = &resources, .sprite = sprite, .texture = texture};
    AbSpan name = {(const uint8_t *)"TRAIL_WHITE_1", 13};
    uint32_t counter = 7;
    AbSceneMember item;
    AbSceneMembership scene = {&item, 0, 1};
    AbLuaAssetRenderer renderer = {.layer = 99};
    CHECK(!ab_lua_asset_renderer_create(NULL, name, 2, &counter, &scene, &renderer,
                                        NULL) &&
          counter == 7 && renderer.layer == 99);
    scene.count = 1;
    CHECK(!ab_lua_asset_renderer_create(&provider, name, 2, &counter, &scene, &renderer,
                                        NULL) &&
          counter == 7 && renderer.layer == 99);
    scene.count = 0;
    resources.missing_texture = true;
    CHECK(!ab_lua_asset_renderer_create(&provider, name, 2, &counter, &scene, &renderer,
                                        NULL) &&
          counter == 8 && !scene.count && renderer.layer == 99);
    resources.missing_texture = false;
    CHECK(ab_lua_asset_renderer_create(&provider, name, 2.75f, &counter, &scene,
                                       &renderer, NULL) &&
          counter == 9 && renderer.id.value == 9 && renderer.layer == 2 &&
          renderer.pose.visible);
    renderer.pose.world_position = (AbVec2){32, 32};
    AbCoordinateCamera camera = {.zoom = {1, 1}};
    AbDrawCommand command = {.texture = 99};
    bool visible = false;
    CHECK(ab_lua_asset_renderer_command(&renderer, camera, false, 480, 320, &command,
                                        &visible, NULL) &&
          visible && command.texture == 31);
    renderer.draw_string = true;
    CHECK(!ab_lua_asset_renderer_command(&renderer, camera, false, 480, 320, &command,
                                         &visible, NULL) &&
          visible);
    renderer.pose.visible = false;
    CHECK(ab_lua_asset_renderer_command(&renderer, camera, false, 480, 320, &command,
                                        &visible, NULL) &&
          !visible);
    renderer.pose.visible = true;
    resources.missing_texture = true;
    CHECK(!ab_lua_asset_renderer_set_sprite(&renderer, &provider, name, NULL) &&
          !renderer.sprite.region && renderer.sprite_name.data == name.data);
    CHECK(ab_lua_asset_renderer_command(&renderer, camera, false, 480, 320, &command,
                                        &visible, NULL) &&
          !visible);
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
