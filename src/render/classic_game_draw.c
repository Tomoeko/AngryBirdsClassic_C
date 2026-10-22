#include "ab/classic_game_draw.h"

#include "ab/fill_rectangle.h"
#include "ab/game_rectangle.h"

#include <limits.h>
#include <math.h>
#include <string.h>

static AbDataValue text(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)value, strlen(value)}};
}

static AbDataValue field(const AbDataTable *table, const char *name) {
    const AbDataValue *value = ab_data_get(table, text(name));
    return value ? *value : (AbDataValue){0};
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL && (value.kind != AB_DATA_BOOLEAN || value.bits);
}

static bool named(AbDataValue value, const char *name) {
    size_t length = strlen(name);
    return value.kind == AB_DATA_STRING && value.string.data &&
           value.string.size == length && !memcmp(value.string.data, name, length);
}

static bool equal(AbDataValue first, AbDataValue second) {
    if (first.kind != second.kind)
        return false;
    switch (first.kind) {
        case AB_DATA_NIL:
            return true;
        case AB_DATA_BOOLEAN:
            return !!first.bits == !!second.bits;
        case AB_DATA_NUMBER: {
            float a, b;
            memcpy(&a, &first.bits, sizeof(a));
            memcpy(&b, &second.bits, sizeof(b));
            return a == b;
        }
        case AB_DATA_STRING:
            return first.string.size == second.string.size &&
                   (!first.string.size ||
                    (first.string.data && second.string.data &&
                     !memcmp(first.string.data, second.string.data,
                             first.string.size)));
        case AB_DATA_TABLE:
            return first.table == second.table;
        case AB_DATA_OBJECT:
            return first.object == second.object;
    }
    return false;
}

static bool number(AbDataValue value, float *output, AbError *error) {
    if (value.kind != AB_DATA_NUMBER)
        return ab_fail(error, "Nonnumeric reached gameplay draw value");
    memcpy(output, &value.bits, sizeof(*output));
    return isfinite(*output) || ab_fail(error, "Nonfinite gameplay draw value");
}

static bool valid_table(const AbDataTable *table) {
    return table && table->count <= table->capacity &&
           (!table->count || table->entries);
}

static void reset_geometry(AbClassicGameDrawConfig *config) {
    config->theme->config.render->geometry =
        (AbSpriteRenderState){.scale_x = 1, .scale_y = 1};
}

bool ab_classic_game_draw_initialize(AbClassicGameDrawConfig config,
                                     AbClassicGameDraw *output, AbError *error) {
    AbClassicVisualView visual = ab_classic_visuals_view(config.visuals);
    if (!output || !config.level || visual.level != config.level ||
        ab_classic_render_scene_level(config.scene) != config.level ||
        !config.behaviors || !config.camera || !config.sling || !config.powerups ||
        !config.powerups->initialized || !config.particles || !config.provider ||
        !config.theme || !config.theme->initialized || !config.state ||
        ab_classic_sling_draw_level(config.sling) != config.level ||
        ab_classic_sling_draw_alpha(config.sling) !=
            &config.theme->config.render->alpha ||
        !config.current_font || !config.basic_font.data || !config.basic_font.size ||
        !valid_table(config.theme_definition) || !valid_table(config.local_settings) ||
        !valid_table(config.extra_draw_functions))
        return ab_fail(error, "Invalid concrete gameplay draw owners");
    *output = (AbClassicGameDraw){.config = config, .initialized = true};
    return true;
}

typedef struct {
    AbClassicGameDrawConfig *config;
    AbThemeDrawInput theme;
    AbDrawList *list;
} DrawCall;

static bool refresh_camera(DrawCall *call, AbError *error) {
    AbCameraViewport viewport;
    if (!ab_camera_system_query_viewport(ab_classic_camera_system(call->config->camera),
                                         &viewport))
        return ab_fail(error, "Invalid gameplay draw camera geometry");
    call->theme.camera = viewport.rendering;
    return true;
}

static bool fullscreen(DrawCall *call, const float rgba[4], AbError *error) {
    /* drawFullscreenRect162365 defaults respect-state to false, uses ceil of
     * the logical dimensions, then invokes the native integer fill route. */
    AbGameRectangle rectangle = {.right = (float)call->theme.width,
                                 .bottom = (float)call->theme.height};
    memcpy(rectangle.color, rgba, sizeof(rectangle.color));
    AbGameRectanglePlan plan;
    if (!ab_game_rectangle_plan(&rectangle, &plan, error))
        return false;
    reset_geometry(call->config);
    call->config->theme->config.render->alpha = 1;
    AbDrawCommand command = {0};
    bool visible = false;
    return ab_fill_rectangle_reset_command(
               (AbFillRectangle){plan.argb, plan.left, plan.top, plan.width,
                                 plan.height, AB_FILL_BLEND_AUTO},
               call->theme.width, call->theme.height,
               call->config->theme->config.white_texture, &command, &visible, error) &&
           (!visible || ab_draw_list_push(call->list, &command, error));
}

static bool color(AbDataValue table, float rgba[4], size_t count, AbError *error) {
    static const char *const keys[] = {"r", "g", "b", "a"};
    if (table.kind != AB_DATA_TABLE || !valid_table(table.table))
        return ab_fail(error, "Missing actual gameplay color table");
    for (size_t channel = 0; channel < count; ++channel)
        if (!number(field(table.table, keys[channel]), rgba + channel, error))
            return false;
    return true;
}

static bool background(DrawCall *call, AbError *error) {
    AbClassicGameDrawConfig *config = call->config;
    float rgba[4] = {0, 0, 0, 1};
    if (!color(field(config->theme_definition, "color"), rgba, 3, error) ||
        !ab_background_color_rgb(rgba, config->theme->config.background_argb, error))
        return false;
    for (size_t channel = 0; channel < 3; ++channel)
        rgba[channel] /= 255.0f;
    if (!fullscreen(call, rgba, error))
        return false;
    *config->current_font = config->basic_font;
    if (!truthy(field(config->local_settings, "gfxLowQuality")) &&
        !ab_theme_draw_background(config->theme, call->theme, call->list, error))
        return false;
    if (!truthy(config->state->background_overlay))
        return true;
    return color(config->state->background_overlay, rgba, 4, error) &&
           fullscreen(call, rgba, error);
}

static bool empty_extras(AbClassicGameDrawConfig *config, AbError *error) {
    config->state->has_extra_z_ordered_draws = (AbDataValue){.kind = AB_DATA_BOOLEAN};
    if (!valid_table(config->extra_draw_functions))
        return ab_fail(error, "Invalid actual extra-draw registry");
    for (size_t index = 0; index < config->extra_draw_functions->count; ++index)
        if (config->extra_draw_functions->entries[index].value.kind != AB_DATA_NIL)
            return ab_fail(error, "Unimplemented reached registered extra/z draw");
    return true;
}

static bool sling_part(DrawCall *call, AbSlingshotDrawPart part, AbError *error) {
    return refresh_camera(call, error) &&
           ab_classic_sling_draw_part(call->config->sling, call->theme.camera,
                                      call->theme.width, call->theme.height, part,
                                      call->list, error);
}

static AbParticleDrawStyle particle_style(const DrawCall *call) {
    return (AbParticleDrawStyle){.provider = call->config->provider,
                                 .camera = call->theme.camera,
                                 .width = call->theme.width,
                                 .height = call->theme.height,
                                 .blend = AB_BLEND_PREMULTIPLIED,
                                 .color = {1, 1, 1, 1},
                                 .opacity = call->config->theme->config.render->alpha};
}

static bool level_particles(DrawCall *call, int32_t layer, AbError *error) {
    if (truthy(field(call->config->local_settings, "gfxLowQuality")) ||
        !truthy(call->config->state->level_particles_enabled))
        return true;
    return refresh_camera(call, error) &&
           ab_particles_draw_level(call->config->particles, particle_style(call), layer,
                                   call->list, error);
}

static bool world(DrawCall *call, AbError *error) {
    AbClassicGameDrawConfig *config = call->config;
    if (!refresh_camera(call, error))
        return false;
    float zoom = ab_classic_camera_view(config->camera).system.mover->current_zoom;
    if (!empty_extras(config, error) ||
        !ab_classic_sling_draw_calculate(config->sling, error) ||
        !sling_part(call, AB_SLING_DRAW_BACK, error))
        return false;
    /* Capture getZoomScale's result before drawing any registered content. */
    if (truthy(*config->powerups->config.enabled) &&
        !ab_classic_game_powerups_draw(config->powerups, zoom, call->list, error))
        return false;
    return refresh_camera(call, error) &&
           ab_classic_render_scene_sync(config->scene, error) &&
           ab_classic_scene_sync_visuals(ab_classic_render_scene_objects(config->scene),
                                         config->visuals, error) &&
           ab_classic_render_scene_draw(config->scene, call->theme.camera,
                                        call->theme.width, call->theme.height,
                                        call->list, error) &&
           ab_particles_draw_world(config->particles, particle_style(call), call->list,
                                   error) &&
           sling_part(call, AB_SLING_DRAW_HOLDER, error) &&
           level_particles(call, 1, error) &&
           (named(config->state->current_theme, "themeUGC") ||
            ab_theme_draw_foreground(config->theme, call->theme, call->list, error)) &&
           level_particles(call, 2, error) &&
           sling_part(call, AB_SLING_DRAW_FRONT, error);
}

static bool eagle(DrawCall *call, AbError *error) {
    AbClassicGameDrawConfig *config = call->config;
    AbClassicLevelView level = ab_classic_level_view(config->level);
    AbDataValue bird = field(&level.properties->world, "MightyEagle_a");
    if (bird.kind != AB_DATA_NIL) {
        float target, position;
        if (bird.kind != AB_DATA_TABLE || !valid_table(bird.table) ||
            !number(field(bird.table, "targetX"), &target, error) ||
            !number(field(bird.table, "x"), &position, error))
            return ab_fail(error, "Unsupported reached eagle darkness object");
        if (position < target) {
            float darkness = fabsf(target - position);
            if (!isfinite(darkness))
                return ab_fail(error, "Nonfinite reached eagle darkness distance");
            darkness = (100.0f - fminf(fmaxf(darkness, 0), 100)) / 200.0f;
            config->state->eagle_darkness = (AbDataValue){.kind = AB_DATA_NUMBER};
            memcpy(&config->state->eagle_darkness.bits, &darkness, sizeof(darkness));
        }
    }
    /* No assignment on absent eagle or x>=target: an earlier darkness remains. */
    if (config->state->eagle_darkness.kind == AB_DATA_NIL)
        return true;
    float rgba[4] = {0};
    return number(config->state->eagle_darkness, rgba + 3, error) &&
           fullscreen(call, rgba, error);
}

static bool debug_draw(AbClassicGameDrawConfig *config, AbError *error) {
    AbClassicLevelView level = ab_classic_level_view(config->level);
    for (size_t index = 0; index < level.bodies->count; ++index) {
        size_t object = level.object_order[index];
        AbDataValue live = level.properties->world_entries[object].value;
        if (live.kind == AB_DATA_NIL)
            continue;
        AbLevelPropertyObject *canonical = level.properties->objects + object;
        if (live.kind != AB_DATA_TABLE || live.table != &canonical->physical)
            return ab_fail(error, "Unsupported rebound gameplay draw world object");
        AbDataValue script = field(live.table, "gameObjectScript");
        if (!truthy(script))
            continue;
        if (script.kind != AB_DATA_TABLE || script.table != &canonical->script)
            return ab_fail(error, "Unsupported rebound gameplay debugDraw script");
        if (!ab_classic_behaviors_debug_draw(config->behaviors, object, error))
            return false;
    }
    return true;
}

static bool tail(DrawCall *call, AbError *error) {
    AbClassicGameDrawConfig *config = call->config;
    AbClassicGameDrawState *state = config->state;
    if (equal(state->current_game_mode, state->editor_mode)) {
        if (!config->editor)
            return ab_fail(error, "Missing reached actual editor draw");
        if (!config->editor(config->context, call->list, error))
            return false;
    } else {
        reset_geometry(config);
    }
    if (truthy(state->has_extra_z_ordered_draws))
        return ab_fail(error, "Unimplemented reached sorted extra draw");
    /* Recheck the second traversal: an earlier bound draw can register work. */
    if (!valid_table(config->extra_draw_functions))
        return ab_fail(error, "Invalid actual late extra-draw registry");
    for (size_t index = 0; index < config->extra_draw_functions->count; ++index)
        if (config->extra_draw_functions->entries[index].value.kind != AB_DATA_NIL)
            return ab_fail(error, "Unimplemented reached late extra draw");
    reset_geometry(config);
    state->old_zoom_level = state->zoom_level;
    if (!eagle(call, error))
        return false;
    if (state->popup_page.kind != AB_DATA_NIL) {
        if (!config->popup)
            return ab_fail(error, "Missing reached actual popup page draw");
        if (!config->popup(config->context, state->popup_page, call->list, error))
            return false;
    }
    if (!debug_draw(config, error))
        return false;
    if (truthy(state->use_restricted_aspect_ratio) &&
        state->updated_width.kind != AB_DATA_NIL &&
        state->updated_height.kind != AB_DATA_NIL) {
        if (!config->restricted_borders)
            return ab_fail(error, "Missing reached actual restricted border draw");
        return config->restricted_borders(config->context, call->list, error);
    }
    return true;
}

bool ab_classic_game_draw(AbClassicGameDraw *draw, uint32_t width, uint32_t height,
                          float time, AbDrawList *list, AbError *error) {
    if (!draw || !draw->initialized || draw->drawing || draw->failed || !list ||
        list->count > list->capacity || (list->capacity && !list->commands) || !width ||
        !height || width > INT32_MAX || height > INT32_MAX)
        return ab_fail(error, "Invalid or reentrant concrete gameplay draw");
    draw->drawing = true;
    AbCameraViewport viewport;
    bool okay = ab_camera_system_query_viewport(
        ab_classic_camera_system(draw->config.camera), &viewport);
    if (okay) {
        DrawCall call = {
            &draw->config, {viewport.rendering, width, height, time, 0}, list};
        okay = background(&call, error) && world(&call, error) && tail(&call, error);
    } else {
        ab_fail(error, "Missing reached gameplay draw camera viewport");
    }
    draw->drawing = false;
    draw->failed = !okay;
    return okay;
}
