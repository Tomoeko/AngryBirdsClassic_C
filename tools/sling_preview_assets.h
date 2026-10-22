#ifndef AB_SLING_PREVIEW_ASSETS_H
#define AB_SLING_PREVIEW_ASSETS_H

#include "ab/classic_level.h"
#include "ab/coordinates.h"
#include "ab/image_points.h"
#include "ab/rotation.h"
#include "ab/slingshot_body.h"
#include "ab/slingshot_draw.h"
#include "ab/slingshot_pose.h"
#include "ab/sprite_atlas.h"

#include <math.h>
#include <string.h>

typedef struct {
    const AbSpriteRegion *region;
    float uv[8];
    uint32_t texture;
    bool alpha;
} PreviewSlingImage;

typedef struct {
    PreviewSlingImage images[4];
    AbSlingshotStartPose start;
    AbSlingshotPose pose;
    size_t bird;
} PreviewSling;

static bool sling_preview_binding(const AbSpriteAtlas *atlas, const char *name,
                                  PreviewSlingImage *output, AbError *error) {
    AbSpan key = {(const uint8_t *)name, strlen(name)};
    AbSpriteAtlasBinding binding;
    AbDrawQuad quad;
    if (!ab_sprite_atlas_find(atlas, key, &binding) ||
        binding.alpha == AB_SPRITE_ALPHA_UNKNOWN || binding.image_index >= UINT32_MAX ||
        !ab_sprite_quad(
            binding.sprite.region, binding.image->width, binding.image->height,
            (AbSpritePlacement){.scale_x = 1, .scale_y = 1}, &quad, error)) {
        return ab_fail(error, "Cannot prepare selected sling sprite binding");
    }
    output->region = binding.sprite.region;
    output->texture = (uint32_t)binding.image_index + 1;
    output->alpha = binding.alpha == AB_SPRITE_ALPHA_PRESENT;
    for (size_t vertex = 0; vertex < 4; ++vertex) {
        output->uv[vertex * 2] = quad.vertices[vertex].u;
        output->uv[vertex * 2 + 1] = quad.vertices[vertex].v;
    }
    return true;
}

/* Selected idle level-one endpoint. Skip transfer time explicitly in this
 * research preview, retaining the recovered endpoint pose/type operations.
 * Full gameplay scheduling, bird animation, input/queue and extras are separate. */
static bool sling_preview_open(AbClassicLevelView level, const AbSpriteAtlas *atlas,
                               PreviewSling *output, AbError *error) {
    *output = (PreviewSling){0};
    for (size_t image = 0; image < 4; ++image) {
        const char *name = image == 3
                               ? "SLINGSHOT_PIXEL"
                               : ab_slingshot_sprite_name((AbSlingshotSprite)image);
        if (!sling_preview_binding(atlas, name, output->images + image, error)) {
            return false;
        }
    }
    size_t bird = SIZE_MAX;
    for (size_t index = 0; index < level.bird_count; ++index) {
        size_t object = level.bird_order[index];
        if (object >= level.bodies->count) {
            return ab_fail(error, "Invalid initial sling bird index");
        }
        AbDataValue number = level.bodies->objects[object].start_number;
        if (number.kind == AB_DATA_NUMBER && number.bits == UINT32_C(0x3f800000)) {
            if (bird != SIZE_MAX) {
                return ab_fail(error, "Ambiguous first bird in selected preview");
            }
            bird = object;
        }
    }
    const char *scalar_name = "physicsToWorld";
    const AbDataValue *scalar = ab_data_get(
        level.data->globals,
        (AbDataValue){.kind = AB_DATA_STRING,
                      .string = {(const uint8_t *)scalar_name, strlen(scalar_name)}});
    if (bird == SIZE_MAX || !scalar || scalar->kind != AB_DATA_NUMBER) {
        return ab_fail(error, "Missing selected sling bird or conversion scalar");
    }
    const AbLevelObject *object = level.bodies->objects + bird;
    const AbSpriteRegion *back = output->images[AB_SLING_SPRITE_BACK].region;
    float conversion, angle;
    memcpy(&conversion, &scalar->bits, sizeof(conversion));
    if (object->physics.dimensions.radius < 0 || !isfinite(conversion) ||
        !ab_script_angle_from_components(-0.1f, 0.1f, &angle)) {
        return ab_fail(error, "Selected preview requires a circular first bird");
    }
    AbSlingshotStartInput start = {
        .object_position = level.frame->objects[bird].rendered.position,
        .radius = object->physics.dimensions.radius,
        .back_pivot_y = (float)(int32_t)((float)back->pivot_y + 0.5f),
        .back_height = (float)back->height,
        .physics_scale = 0.05f,
        .object_present = true,
        .radius_present = true};
    if (!ab_slingshot_start_pose(&start, &output->start, error)) {
        return false;
    }
    AbSlingshotPoseInput pose = {.start_physics = output->start.level_start,
                                 .band_physics = output->start.band,
                                 .band_angle = angle,
                                 .bird_radius = start.radius,
                                 .physics_to_world = conversion,
                                 .bird_present = true,
                                 .radius_present = true};
    AbSlingTransferInput transfer = {
        .start = start.object_position, .target = output->start.level_start, .time = 1};
    AbSlingTransferFrame frame;
    if (!ab_slingshot_render_pose(&pose, &output->pose, error) ||
        !ab_slingshot_transfer_body(level.world, &object->physics, &transfer, &frame,
                                    error) ||
        !frame.complete ||
        !ab_game_frame_object_reset(level.world, object->physics.body,
                                    level.frame->objects[bird].collider,
                                    level.frame->objects + bird, error)) {
        return false;
    }
    output->bird = bird;
    return true;
}

static bool sling_preview_sprite(const PreviewSlingImage *image, AbVec2 position,
                                 AbSlingshotDrawState state, AbDrawList *list,
                                 AbError *error) {
    AbDrawCommand command = {.texture = image->texture,
                             .blend = image->alpha ? AB_BLEND_PREMULTIPLIED
                                                   : AB_BLEND_OPAQUE,
                             .color = {1, 1, 1, 1},
                             .opacity = 1,
                             .coordinates = AB_DRAW_CLIP};
    bool visible = false;
    return ab_slingshot_sprite_quad(image->region, position, state, 480, 320, image->uv,
                                    &command.quad, &visible, error) &&
           (!visible || ab_draw_list_push(list, &command, error));
}

static bool sling_preview_band(const PreviewSlingImage *image, const AbRubberband *band,
                               AbCoordinateCamera camera, AbDrawList *list,
                               AbError *error) {
    AbVec2 points[4];
    bool drawn = false, visible = false;
    AbDrawCommand command = {0};
    if (!ab_rubberband_world_vertices(band, camera.top_left, camera.zoom.x, points,
                                      &drawn, error)) {
        return false;
    }
    return !drawn || (ab_image_points_command(
                          points, 480, 320, image->uv,
                          (AbImagePointMaterial){image->texture, image->alpha, 1},
                          &command, &visible, error) &&
                      (!visible || ab_draw_list_push(list, &command, error)));
}

static bool sling_preview_draw(const PreviewSling *sling, AbCoordinateCamera camera,
                               AbSlingshotDrawPart part, AbDrawList *list,
                               AbError *error) {
    if (camera.zoom.x != camera.zoom.y) {
        return ab_fail(error, "Selected sling preview requires scalar camera zoom");
    }
    const AbSpriteRegion *holder = sling->images[AB_SLING_SPRITE_HOLDER].region;
    AbSlingshotDrawInput input = {
        .position = sling->pose.position,
        .stretch_position = sling->pose.stretch_position,
        .band_width = sling->pose.width,
        .band_angle = sling->pose.angle,
        .camera_top_left = camera.top_left,
        .zoom = camera.zoom.x,
        .holder_pivot = {(float)(int32_t)((float)holder->pivot_x + 0.5f),
                         (float)(int32_t)((float)holder->pivot_y + 0.5f)}};
    AbSlingshotDrawPlan plan;
    if (!ab_slingshot_draw_plan(&input, part, &plan, error)) {
        return false;
    }
    AbSlingshotDrawState state = {0};
    for (size_t index = 0; index < plan.count; ++index) {
        const AbSlingshotDrawCommand *command = plan.commands + index;
        switch (command->kind) {
            case AB_SLING_COMMAND_STATE:
                state = command->state;
                break;
            case AB_SLING_COMMAND_SPRITE:
                if ((unsigned)command->sprite.image >= 3 ||
                    !sling_preview_sprite(sling->images + command->sprite.image,
                                          command->sprite.position, state, list,
                                          error)) {
                    return false;
                }
                break;
            case AB_SLING_COMMAND_BAND:
                if (!sling_preview_band(sling->images + 3, &command->band, camera, list,
                                        error)) {
                    return false;
                }
                break;
            case AB_SLING_COMMAND_EXTRAS:
                /* Default class: both scope and extra-trajectory flags are false. */
                break;
        }
    }
    return true;
}

#endif
