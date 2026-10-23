#include "ab/classic_sling_draw.h"

#include "ab/coordinates.h"
#include "ab/image_points.h"
#include "ab/ui_image_render.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const AbSpriteRegion *region;
    const char *name;
    AbResolvedComposite composite;
    uint32_t texture;
    float uv[8];
    bool alpha;
} SlingImage;

struct AbClassicSlingDraw {
    AbClassicSlingDrawConfig config;
    AbClassicLevelView level;
    SlingImage images[4];
    SlingImage powered[4];
    AbSlingshotPose pose;
    bool prepared, calling, failed;
    uint32_t powered_ready;
};

static AbDataValue text(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)value, strlen(value)}};
}

static AbDataValue field(const AbDataTable *table, const char *name) {
    const AbDataValue *value = ab_data_get(table, text(name));
    return value ? *value : (AbDataValue){0};
}

static bool number(AbDataValue value, float *output, AbError *error) {
    if (value.kind != AB_DATA_NUMBER)
        return ab_fail(error, "Nonnumeric canonical sling draw property");
    memcpy(output, &value.bits, sizeof(*output));
    return isfinite(*output) ||
           ab_fail(error, "Nonfinite canonical sling draw property");
}

static bool bind_image(const AbSpriteProvider *provider, const char *name,
                       SlingImage *output, AbError *error) {
    AbSpriteAtlasBinding binding = {0};
    uint32_t texture = 0;
    if (!provider->sprite(provider->context, text(name).string, &binding)) {
        AbResolvedComposite composite = {0};
        if (provider->composite &&
            provider->composite(provider->context, text(name).string, &composite) &&
            composite.source) {
            *output = (SlingImage){.name = name, .composite = composite};
            return true;
        }
        return ab_fail(error, "Missing preloaded sling image binding: %s", name);
    }
    if (!binding.sprite.region || !binding.image ||
        binding.alpha == AB_SPRITE_ALPHA_UNKNOWN ||
        !provider->texture(provider->context, binding.image, &texture) || !texture)
        return ab_fail(error, "Missing preloaded sling image binding: %s", name);
    AbDrawQuad quad;
    if (!ab_sprite_quad(binding.sprite.region, binding.image->width,
                        binding.image->height,
                        (AbSpritePlacement){.scale_x = 1, .scale_y = 1}, &quad, error))
        return false;
    *output = (SlingImage){.name = name,
                           .region = binding.sprite.region,
                           .texture = texture,
                           .alpha = binding.alpha == AB_SPRITE_ALPHA_PRESENT};
    for (size_t vertex = 0; vertex < 4; ++vertex) {
        output->uv[2 * vertex] = quad.vertices[vertex].u;
        output->uv[2 * vertex + 1] = quad.vertices[vertex].v;
    }
    return true;
}

bool ab_classic_sling_draw_open(AbClassicSlingDrawConfig config,
                                AbClassicSlingDraw **output, AbError *error) {
    if (!output)
        return ab_fail(error, "Missing owned sling draw output");
    *output = NULL;
    AbClassicLevelView level =
        ab_classic_level_view(ab_classic_shots_level(config.shots));
    if (!config.sling || ab_classic_sling_shots(config.sling) != config.shots ||
        !level.properties || !config.provider || !config.provider->sprite ||
        !config.provider->texture || !config.physics_to_world || !config.render_alpha ||
        config.storage_bytes < sizeof(AbClassicSlingDraw))
        return ab_fail(error, "Invalid base-sling draw owners or storage");
    AbClassicSlingDraw *draw = calloc(1, sizeof(*draw));
    if (!draw)
        return ab_fail(error, "Cannot allocate base-sling draw owner");
    draw->config = config;
    draw->level = level;
    bool okay = true;
    for (size_t index = 0; okay && index < 4; ++index) {
        const char *name = index == 3
                               ? "SLINGSHOT_PIXEL"
                               : ab_slingshot_sprite_name((AbSlingshotSprite)index);
        okay = bind_image(config.provider, name, draw->images + index, error);
    }
    if (!okay) {
        ab_classic_sling_draw_close(draw);
        return false;
    }
    *output = draw;
    return true;
}

void ab_classic_sling_draw_close(AbClassicSlingDraw *draw) {
    free(draw);
}

size_t ab_classic_sling_draw_bytes(const AbClassicSlingDraw *draw) {
    return draw ? sizeof(*draw) : 0;
}

AbClassicLevel *ab_classic_sling_draw_level(const AbClassicSlingDraw *draw) {
    return draw ? ab_classic_shots_level(draw->config.shots) : NULL;
}

const float *ab_classic_sling_draw_alpha(const AbClassicSlingDraw *draw) {
    return draw ? draw->config.render_alpha : NULL;
}

static bool calculate(AbClassicSlingDraw *draw, AbError *error) {
    AbSlingBandState live = ab_classic_sling_view(draw->config.sling).band;
    AbSlingshotPoseInput input = {.start_physics = live.rest,
                                  .band_physics = live.rubber_band,
                                  .band_angle = live.angle,
                                  .distance = live.distance};
    /* Publish source's vectors/angle before the later canonical bird lookup. */
    if (!ab_coordinate_point(NULL, AB_SPACE_PHYSICS, AB_SPACE_WORLD, input.band_physics,
                             &draw->pose.stretch_position) ||
        !ab_coordinate_point(NULL, AB_SPACE_PHYSICS, AB_SPACE_WORLD,
                             input.start_physics, &draw->pose.position))
        return ab_fail(error, "Invalid live sling draw coordinate transform");
    draw->pose.angle = live.angle;
    const char *name = ab_classic_shots_state(draw->config.shots)->current_bird_name;
    if (name) {
        AbDataValue bird = field(&draw->level.properties->world, name);
        if (bird.kind != AB_DATA_TABLE || !bird.table)
            return ab_fail(error, "Missing current canonical sling draw bird");
        AbDataValue radius = field(bird.table, "radius");
        AbDataValue type = field(bird.table, "type");
        input.bird_present = true;
        input.radius_present = radius.kind != AB_DATA_NIL;
        input.box = type.kind == AB_DATA_STRING && type.string.size == 3 &&
                    type.string.data && !memcmp(type.string.data, "box", 3);
        if (!(input.radius_present && !input.box
                  ? number(radius, &input.bird_radius, error)
                  : number(field(bird.table, "width"), &input.bird_width, error)) ||
            !number(*draw->config.physics_to_world, &input.physics_to_world, error))
            return false;
    }
    AbSlingshotPose pose;
    if (!ab_slingshot_render_pose(&input, &pose, error))
        return false;
    draw->pose = pose;
    draw->prepared = true;
    return true;
}

bool ab_classic_sling_draw_calculate(AbClassicSlingDraw *draw, AbError *error) {
    if (!draw || draw->calling || draw->failed)
        return ab_fail(error, "Invalid or reentrant live sling pose calculation");
    draw->calling = true;
    bool okay = calculate(draw, error);
    draw->calling = false;
    draw->failed = !okay;
    return okay;
}

bool ab_classic_sling_draw_pose(const AbClassicSlingDraw *draw,
                                AbSlingshotPose *output) {
    if (!draw || !draw->prepared || !output)
        return false;
    *output = draw->pose;
    return true;
}

typedef struct {
    const AbClassicSlingDraw *draw;
    const SlingImage *image;
} SlingComposite;

static bool composite_lookup(void *context, AbSpan name, AbUiImageRenderBinding *output,
                             AbError *error) {
    SlingComposite *selected = context;
    if (name.size != strlen(selected->image->name) ||
        memcmp(name.data, selected->image->name, name.size))
        return ab_fail(error, "Foreign cached sling composite lookup");
    *output = (AbUiImageRenderBinding){.composite = selected->image->composite};
    return true;
}

static bool composite_part(void *context, const AbSpriteBinding *sprite,
                           AbUiImageRenderBinding *output, AbError *error) {
    SlingComposite *selected = context;
    const AbSpriteProvider *provider = selected->draw->config.provider;
    AbSpriteAtlasBinding binding = {0};
    uint32_t texture;
    if (!provider->binding ||
        !provider->binding(provider->context, *sprite, &binding) || !binding.image ||
        !binding.sprite.region || binding.alpha == AB_SPRITE_ALPHA_UNKNOWN ||
        !provider->texture(provider->context, binding.image, &texture) || !texture)
        return ab_fail(error, "Missing cached sling composite part binding");
    *output = (AbUiImageRenderBinding){.region = binding.sprite.region,
                                       .texture = texture,
                                       .texture_width = binding.image->width,
                                       .texture_height = binding.image->height,
                                       .alpha = binding.alpha};
    return true;
}

static bool ordinary_sprite(const AbClassicSlingDraw *draw, const SlingImage *image,
                            AbVec2 position, AbSlingshotDrawState state, uint32_t width,
                            uint32_t height, AbDrawList *list, AbError *error) {
    AbDrawCommand command = {.texture = image->texture,
                             .blend = image->alpha ? AB_BLEND_PREMULTIPLIED
                                                   : AB_BLEND_OPAQUE,
                             .color = {1, 1, 1, 1},
                             .opacity = image->alpha ? *draw->config.render_alpha : 1,
                             .coordinates = AB_DRAW_CLIP};
    bool visible = false;
    return ab_slingshot_sprite_quad(image->region, position, state, width, height,
                                    image->uv, &command.quad, &visible, error) &&
           (!visible || ab_draw_list_push(list, &command, error));
}

static bool sprite(const AbClassicSlingDraw *draw, const SlingImage *image,
                   AbVec2 position, AbSlingshotDrawState state, uint32_t width,
                   uint32_t height, AbDrawList *list, AbError *error) {
    if (image->composite.source) {
        SlingComposite selected = {draw, image};
        AbUiImageRenderBoundaries boundaries = {.context = &selected,
                                                .lookup = composite_lookup,
                                                .part = composite_part,
                                                .viewport_width = width,
                                                .viewport_height = height,
                                                .blend = AB_BLEND_PREMULTIPLIED,
                                                .color = {1, 1, 1, 1}};
        AbUiImageRenderState render = {
            .geometry = {.translation_x = state.translation.x,
                         .translation_y = state.translation.y,
                         .scale_x = state.scale.x,
                         .scale_y = state.scale.y,
                         .angle_radians = state.angle,
                         .rotation_origin_x = state.pivot.x,
                         .rotation_origin_y = state.pivot.y},
            .alpha = *draw->config.render_alpha};
        AbUiSpriteRender source = {.image = text(image->name).string,
                                   .position = position,
                                   .vertical = AB_ANCHOR_VPIVOT,
                                   .horizontal = AB_ANCHOR_HPIVOT};
        return ab_ui_sprite_render(&source, &render, &boundaries, list, error);
    }
    return ordinary_sprite(draw, image, position, state, width, height, list, error);
}

static bool band(const AbClassicSlingDraw *draw, const AbRubberband *geometry,
                 AbCoordinateCamera camera, uint32_t width, uint32_t height,
                 AbDrawList *list, AbError *error) {
    const SlingImage *image = draw->images + 3;
    AbVec2 vertices[4];
    bool drawn = false, visible = false;
    AbDrawCommand command = {0};
    return ab_rubberband_world_vertices(geometry, camera.top_left, camera.zoom.x,
                                        vertices, &drawn, error) &&
           (!drawn || (ab_image_points_command(
                           vertices, width, height, image->uv,
                           (AbImagePointMaterial){image->texture, image->alpha,
                                                  *draw->config.render_alpha},
                           &command, &visible, error) &&
                       (!visible || ab_draw_list_push(list, &command, error))));
}

static bool part(AbClassicSlingDraw *draw, AbCoordinateCamera camera, uint32_t width,
                 uint32_t height, AbSlingshotDrawPart selected, AbDrawList *list,
                 AbError *error) {
    AbClassicSlingView live = ab_classic_sling_view(draw->config.sling);
    if (live.king || live.has_scope) {
        const char *const names[] = {"SLING_SHOT_02_BACK", "SLING_SHOT_02_FRONT",
                                     "SLINGSHOT_POWERUP_KINGSLING",
                                     "SLINGSHOT_POWERUP_BASIC"};
        uint32_t required = live.king ? 3 : 0;
        if (live.has_scope)
            required |= live.king ? 4 : 8;
        for (size_t index = 0; index < 4; ++index) {
            uint32_t bit = UINT32_C(1) << index;
            if (!(required & bit) || (draw->powered_ready & bit))
                continue;
            if (!bind_image(draw->config.provider, names[index], draw->powered + index,
                            error))
                return false;
            draw->powered_ready |= bit;
        }
    }
    const AbSpriteRegion *holder = draw->images[AB_SLING_SPRITE_HOLDER].region;
    AbSlingshotDrawInput input = {
        .position = draw->pose.position,
        .stretch_position = draw->pose.stretch_position,
        .band_width = draw->pose.width,
        .band_angle = draw->pose.angle,
        .camera_top_left = camera.top_left,
        .zoom = camera.zoom.x,
        .holder_pivot = {(float)(int32_t)((float)holder->pivot_x + 0.5f),
                         (float)(int32_t)((float)holder->pivot_y + 0.5f)},
        .disabled = live.disabled_for_current_level,
        .has_scope = live.has_scope,
        .dragged = ab_classic_shots_state(draw->config.shots)->dragged != NULL};
    if (!input.disabled && camera.zoom.x != camera.zoom.y)
        return ab_fail(error, "Unsupported nonuniform base-sling rendering camera");
    AbSlingshotDrawPlan plan;
    if (!ab_slingshot_draw_plan(&input, selected, &plan, error))
        return false;
    AbSlingshotDrawState state = {0};
    for (size_t index = 0; index < plan.count; ++index) {
        const AbSlingshotDrawCommand *command = plan.commands + index;
        switch (command->kind) {
            case AB_SLING_COMMAND_STATE:
                state = command->state;
                break;
            case AB_SLING_COMMAND_SPRITE: {
                const SlingImage *image = NULL;
                switch (command->sprite.image) {
                    case AB_SLING_SPRITE_BACK:
                        image = live.king ? draw->powered : draw->images;
                        break;
                    case AB_SLING_SPRITE_HOLDER:
                        image = draw->images + 1;
                        break;
                    case AB_SLING_SPRITE_FRONT:
                        image = live.king ? draw->powered + 1 : draw->images + 2;
                        break;
                    case AB_SLING_SPRITE_FRONT_SCOPE:
                        image = draw->powered + (live.king ? 2 : 3);
                        break;
                }
                if (!image || !sprite(draw, image, command->sprite.position, state,
                                      width, height, list, error))
                    return false;
                break;
            }
            case AB_SLING_COMMAND_BAND:
                if (!band(draw, &command->band, camera, width, height, list, error))
                    return false;
                break;
            case AB_SLING_COMMAND_EXTRAS:
                /* Supported base-class initializer disables its extra trajectory
                 * and has no scope; drawExtras therefore reaches no work. */
                break;
        }
    }
    return true;
}

bool ab_classic_sling_draw_part(AbClassicSlingDraw *draw, AbCoordinateCamera camera,
                                uint32_t width, uint32_t height,
                                AbSlingshotDrawPart selected, AbDrawList *list,
                                AbError *error) {
    if (!draw || !draw->prepared || draw->calling || draw->failed || !list ||
        list->count > list->capacity || (list->capacity && !list->commands) || !width ||
        !height)
        return ab_fail(error, "Invalid or reentrant live sling draw part");
    draw->calling = true;
    bool okay = part(draw, camera, width, height, selected, list, error);
    draw->calling = false;
    draw->failed = !okay;
    return okay;
}
