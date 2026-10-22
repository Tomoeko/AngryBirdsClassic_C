#include "ab/lua_asset_sprite.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(value)                                                                   \
    do {                                                                               \
        if (!(value)) {                                                                \
            fprintf(stderr, "Lua asset binding check at %d: %s\n", __LINE__, #value);  \
            return false;                                                              \
        }                                                                              \
    } while (0)

typedef struct {
    AbSpriteRegion region;
    AbImage image;
    AbSpriteAlpha alpha;
    uint32_t texture;
    size_t sprite_queries, composite_queries, texture_queries;
} Resources;

static bool sprite(void *context, AbSpan name, AbSpriteAtlasBinding *output) {
    Resources *resources = context;
    ++resources->sprite_queries;
    if (name.size != 1 || name.data[0] != 's')
        return false;
    *output = (AbSpriteAtlasBinding){.sprite.region = &resources->region,
                                     .image = &resources->image,
                                     .alpha = resources->alpha};
    return true;
}

static bool composite(void *context, AbSpan name, AbResolvedComposite *output) {
    (void)output;
    Resources *resources = context;
    ++resources->composite_queries;
    return name.size == 1 && name.data[0] == 'c';
}

static bool texture(void *context, const AbImage *image, uint32_t *output) {
    Resources *resources = context;
    ++resources->texture_queries;
    if (image != &resources->image)
        return false;
    *output = resources->texture;
    return true;
}

static bool checks(void) {
    Resources resources = {
        .region =
            {.x = 16, .y = 32, .width = 16, .height = 16, .pivot_x = 8, .pivot_y = 8},
        .image = {.width = 64, .height = 64},
        .alpha = AB_SPRITE_ALPHA_PRESENT,
        .texture = 17};
    AbSpriteProvider provider = {.context = &resources,
                                 .sprite = sprite,
                                 .composite = composite,
                                 .texture = texture};
    AbSpan name = {(const uint8_t *)"s", 1};
    AbLuaAssetSpriteBinding binding = {.texture = 99};
    CHECK(!ab_lua_asset_sprite_bind(NULL, name, &binding, NULL) &&
          binding.texture == 99);
    CHECK(!ab_lua_asset_sprite_bind(&provider, (AbSpan){NULL, 1}, &binding, NULL));
    CHECK(ab_lua_asset_sprite_bind(&provider, name, &binding, NULL));
    CHECK(binding.region == &resources.region && binding.texture == 17 &&
          binding.has_alpha && binding.uv[0] == 0.25f && binding.uv[1] == 0.5f &&
          binding.uv[2] == 0.5f && binding.uv[5] == 0.75f);
    CHECK(resources.sprite_queries == 1 && !resources.composite_queries &&
          resources.texture_queries == 1);
    AbLuaAssetSpritePose pose = {
        .world_position = {64, 32}, .scale = {1, 1}, .visible = true};
    AbCoordinateCamera camera = {.zoom = {1, 1}};
    AbDrawCommand command = {.texture = 99};
    bool visible = false;
    CHECK(ab_lua_asset_sprite_command(&binding, pose, camera, false, 128, 64, &command,
                                      &visible, NULL) &&
          visible);
    CHECK(command.texture == 17 && command.coordinates == AB_DRAW_CLIP &&
          command.blend == AB_BLEND_PREMULTIPLIED && command.opacity == 1 &&
          command.quad.vertices[0].x == -0.125f &&
          command.quad.vertices[0].y == 0.25f && command.quad.vertices[3].x == 0.125f &&
          command.quad.vertices[3].y == -0.25f && command.quad.vertices[0].u == 0.25f &&
          command.quad.vertices[3].v == 0.75f);
    CHECK(resources.sprite_queries == 1 && resources.texture_queries == 1);
    binding.has_alpha = false;
    pose.alpha = NAN;
    CHECK(ab_lua_asset_sprite_command(&binding, pose, camera, false, 128, 64, &command,
                                      &visible, NULL) &&
          visible && command.blend == AB_BLEND_OPAQUE && command.opacity == 1);
    pose.use_alpha = true;
    CHECK(!ab_lua_asset_sprite_command(&binding, pose, camera, false, 128, 64, &command,
                                       &visible, NULL) &&
          visible);
    pose.alpha = 0.5f;
    CHECK(ab_lua_asset_sprite_command(&binding, pose, camera, false, 128, 64, &command,
                                      &visible, NULL) &&
          visible && command.blend == AB_BLEND_PREMULTIPLIED &&
          command.opacity == 0.5f);
    CHECK(ab_lua_asset_sprite_command(&binding, pose, camera, true, 128, 64, &command,
                                      &visible, NULL) &&
          visible && command.quad.vertices[0].x == -8 &&
          command.quad.vertices[3].y == 8);
    AbDrawCommand saved = command;
    pose.world_position.x = 10000;
    CHECK(ab_lua_asset_sprite_command(&binding, pose, camera, false, 128, 64, &command,
                                      &visible, NULL) &&
          !visible && !memcmp(&command, &saved, sizeof(command)));
    visible = true;
    pose.visible = false;
    pose.world_position.x = NAN;
    CHECK(ab_lua_asset_sprite_command(&binding, pose, camera, false, 0, 0, &command,
                                      &visible, NULL) &&
          !visible && !memcmp(&command, &saved, sizeof(command)));
    resources.alpha = AB_SPRITE_ALPHA_UNKNOWN;
    CHECK(!ab_lua_asset_sprite_bind(&provider, name, &binding, NULL) &&
          binding.texture == 17);
    resources.alpha = AB_SPRITE_ALPHA_PRESENT;
    resources.texture = 0;
    CHECK(!ab_lua_asset_sprite_bind(&provider, name, &binding, NULL) &&
          binding.texture == 17);
    name.data = (const uint8_t *)"c";
    CHECK(!ab_lua_asset_sprite_bind(&provider, name, &binding, NULL) &&
          binding.texture == 17);
    name.data = (const uint8_t *)"m";
    CHECK(ab_lua_asset_sprite_bind(&provider, name, &binding, NULL) && !binding.region);
    pose.visible = true;
    CHECK(ab_lua_asset_sprite_command(&binding, pose, camera, false, 0, 0, &command,
                                      &visible, NULL) &&
          !visible);
    size_t queries = resources.sprite_queries;
    CHECK(ab_lua_asset_sprite_bind(&provider, (AbSpan){0}, &binding, NULL) &&
          !binding.region && resources.sprite_queries == queries);
    return true;
}

int main(void) {
    return checks() ? 0 : 1;
}
