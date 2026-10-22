#include "ab/particle_draw.h"

#include <math.h>
#include <string.h>

bool ab_particle_sprite_state(const AbParticle *particle, const AbSpriteRegion *region,
                              AbCoordinateCamera camera, AbImageRectangle *rectangle,
                              AbSpriteRenderState *state, AbError *error) {
    if (!particle || !region || !rectangle || !state || !isfinite(particle->scale) ||
        particle->scale == 0 || !isfinite(particle->position.x) ||
        !isfinite(particle->position.y) || !isfinite(particle->angle) ||
        !isfinite(camera.top_left.x) || !isfinite(camera.top_left.y) ||
        !isfinite(camera.zoom.x) || !isfinite(camera.zoom.y))
        return ab_fail(error, "Invalid ordinary world particle sprite state");
    float pivot_x = (float)(int32_t)((float)region->pivot_x + 0.5f);
    float pivot_y = (float)(int32_t)((float)region->pivot_y + 0.5f);
    AbSpriteRenderState result = {.angle_radians = particle->angle,
                                  .translation_x = -camera.top_left.x / particle->scale,
                                  .translation_y = -camera.top_left.y / particle->scale,
                                  .scale_x = particle->scale * camera.zoom.x,
                                  .scale_y = particle->scale * camera.zoom.y,
                                  .rotation_origin_x = pivot_x,
                                  .rotation_origin_y = pivot_y};
    AbImageRectangle image = {particle->position.x / particle->scale - pivot_x,
                              particle->position.y / particle->scale - pivot_y,
                              (float)region->width, (float)region->height};
    if (!isfinite(result.translation_x) || !isfinite(result.translation_y) ||
        !isfinite(result.scale_x) || !isfinite(result.scale_y) || !isfinite(image.x) ||
        !isfinite(image.y))
        return ab_fail(error, "World particle render state exceeds finite range");
    *rectangle = image;
    *state = result;
    return true;
}

bool ab_level_particle_sprite_state(const AbParticle *particle,
                                    const AbSpriteRegion *region,
                                    AbCoordinateCamera camera, int32_t layer,
                                    AbImageRectangle *rectangle,
                                    AbSpriteRenderState *state, AbError *error) {
    AbSpriteRenderState result;
    AbImageRectangle image;
    if (!ab_particle_sprite_state(particle, region, camera, &image, &result, error))
        return false;
    float factor = layer > 0 ? (float)layer : 1;
    result.scale_x *= factor;
    result.scale_y *= factor;
    if (!rectangle || !state || !isfinite(result.scale_x) || !isfinite(result.scale_y))
        return ab_fail(error, "Invalid level particle output or layer scale");
    *rectangle = image;
    *state = result;
    return true;
}

bool ab_menu_particle_sprite_state(const AbParticle *particle,
                                   const AbSpriteRegion *region, float menu_scale,
                                   AbImageRectangle *rectangle,
                                   AbSpriteRenderState *state, AbError *error) {
    if (!particle || !region || !rectangle || !state || !isfinite(menu_scale) ||
        !isfinite(particle->scale) || !isfinite(particle->position.x) ||
        !isfinite(particle->position.y) || !isfinite(particle->angle))
        return ab_fail(error, "Invalid ordinary menu particle sprite state");
    float scale = menu_scale * particle->scale;
    if (!isfinite(scale) || scale == 0)
        return ab_fail(error, "Invalid menu particle combined scale");
    float pivot_x = (float)(int32_t)((float)region->pivot_x + 0.5f);
    float pivot_y = (float)(int32_t)((float)region->pivot_y + 0.5f);
    AbSpriteRenderState result = {.angle_radians = particle->angle,
                                  .translation_x = 0,
                                  .translation_y = 0,
                                  .scale_x = scale,
                                  .scale_y = scale,
                                  .rotation_origin_x = pivot_x,
                                  .rotation_origin_y = pivot_y};
    AbImageRectangle image = {particle->position.x / scale - pivot_x,
                              particle->position.y / scale - pivot_y,
                              (float)region->width, (float)region->height};
    if (!isfinite(image.x) || !isfinite(image.y))
        return ab_fail(error, "Menu particle rectangle exceeds finite range");
    *rectangle = image;
    *state = result;
    return true;
}

typedef enum { PARTICLE_WORLD, PARTICLE_LEVEL, PARTICLE_MENU } ParticleDrawKind;

static bool append(const AbParticle *particle, AbParticleDrawStyle style,
                   ParticleDrawKind kind, int32_t layer, float menu_scale,
                   AbDrawList *list, AbError *error) {
    if (!particle->sprite_handle)
        return true;
    AbSpriteAtlasBinding binding;
    uint32_t texture;
    if (!style.provider || !style.provider->sprite || !style.provider->texture ||
        !style.provider->sprite(style.provider->context, particle->sprite, &binding) ||
        !binding.sprite.region || !binding.image ||
        !style.provider->texture(style.provider->context, binding.image, &texture))
        return ab_fail(error, "Missing retained world particle sprite binding");
    AbImageRectangle rectangle;
    AbSpriteRenderState state;
    bool transformed =
        kind == PARTICLE_MENU
            ? ab_menu_particle_sprite_state(particle, binding.sprite.region, menu_scale,
                                            &rectangle, &state, error)
        : kind == PARTICLE_LEVEL
            ? ab_level_particle_sprite_state(particle, binding.sprite.region,
                                             style.camera, layer, &rectangle, &state,
                                             error)
            : ab_particle_sprite_state(particle, binding.sprite.region, style.camera,
                                       &rectangle, &state, error);
    if (!transformed)
        return false;
    AbDrawQuad atlas;
    if (!ab_sprite_quad(binding.sprite.region, binding.image->width,
                        binding.image->height,
                        (AbSpritePlacement){0, 0, 1, 1, AB_ANCHOR_TOP, AB_ANCHOR_LEFT},
                        &atlas, error))
        return false;
    float uv[8];
    for (size_t vertex = 0; vertex < 4; ++vertex) {
        uv[2 * vertex] = atlas.vertices[vertex].u;
        uv[2 * vertex + 1] = atlas.vertices[vertex].v;
    }
    AbDrawCommand command = {.texture = texture,
                             .blend = style.blend,
                             .opacity = style.opacity,
                             .coordinates = AB_DRAW_CLIP};
    memcpy(command.color, style.color, sizeof(command.color));
    bool visible;
    if (!ab_image_rectangle_quad(rectangle, state, style.width, style.height, uv,
                                 &command.quad, &visible, error))
        return false;
    return !visible || ab_draw_list_push(list, &command, error);
}

static bool draw(const AbParticles *particles, AbParticleDrawStyle style,
                 ParticleDrawKind kind, int32_t layer, float menu_scale,
                 AbDrawList *list, AbError *error) {
    if (!particles || !list || list->count > list->capacity ||
        (list->capacity && !list->commands) || !style.width || !style.height)
        return ab_fail(error, "Invalid world particle draw bindings");
    size_t original = list->count;
    bool level = kind == PARTICLE_LEVEL, menu = kind == PARTICLE_MENU;
    size_t count = level ? ab_particles_level_count(particles)
                         : ab_particles_count(particles, menu);
    for (size_t index = 0; index < count; ++index) {
        const AbParticle *particle = level ? ab_particles_level_at(particles, index)
                                           : ab_particles_at(particles, menu, index);
        if (particle && level && layer && particle->layer != layer)
            continue;
        if (!particle ||
            !append(particle, style, kind, layer, menu_scale, list, error)) {
            list->count = original;
            return false;
        }
    }
    return true;
}

bool ab_particles_draw_world(const AbParticles *particles, AbParticleDrawStyle style,
                             AbDrawList *list, AbError *error) {
    return draw(particles, style, PARTICLE_WORLD, 0, 0, list, error);
}

bool ab_particles_draw_level(const AbParticles *particles, AbParticleDrawStyle style,
                             int32_t layer, AbDrawList *list, AbError *error) {
    return draw(particles, style, PARTICLE_LEVEL, layer, 0, list, error);
}

bool ab_particles_draw_menu(const AbParticles *particles, AbParticleDrawStyle style,
                            float menu_scale, AbDrawList *list, AbError *error) {
    return draw(particles, style, PARTICLE_MENU, 0, menu_scale, list, error);
}
