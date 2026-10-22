#include "ab/classic_game_render.h"

#include <stdlib.h>

struct AbClassicGameRender {
    AbThemeLayers *layers;
    AbClassicSlingDraw *sling;
    AbThemeLayerRenderState render;
    AbThemeDraw theme;
    AbClassicGameDraw draw;
    uint32_t background;
};

bool ab_classic_game_render_open(AbClassicGameRenderConfig config,
                                 AbClassicGameRender **output, AbError *error) {
    if (!output)
        return ab_fail(error, "Missing gameplay render output");
    *output = NULL;
    AbClassicPlayfieldView playfield = ab_classic_playfield_view(config.playfield);
    size_t remaining = config.storage_bytes;
    if (!playfield.level || !config.provider || !config.white_texture ||
        sizeof(AbClassicGameRender) > remaining)
        return ab_fail(error, "Invalid gameplay render owners or budget");
    remaining -= sizeof(AbClassicGameRender);
    if (!config.layers_bytes || config.layers_bytes > remaining)
        return ab_fail(error, "Invalid gameplay theme budget");
    remaining -= config.layers_bytes;
    if (!config.sling_bytes || config.sling_bytes > remaining)
        return ab_fail(error, "Invalid gameplay sling render budget");
    AbClassicGameRender *owner = calloc(1, sizeof(*owner));
    if (!owner)
        return ab_fail(error, "Cannot allocate gameplay render owner");
    owner->render =
        (AbThemeLayerRenderState){.geometry = {.scale_x = 1, .scale_y = 1}, .alpha = 1};
    bool okay =
        ab_theme_layers_open_provider(config.theme, config.provider,
                                      config.layers_bytes, &owner->layers, error) &&
        ab_theme_draw_initialize(
            (AbThemeDrawConfig){.layers = owner->layers,
                                .provider = config.provider,
                                .render = &owner->render,
                                .background_argb = &owner->background,
                                .white_texture = config.white_texture,
                                .placement_limit = config.placement_limit},
            &owner->theme, error) &&
        ab_classic_sling_draw_open(
            (AbClassicSlingDrawConfig){.sling = playfield.sling,
                                       .shots = playfield.shots,
                                       .provider = config.provider,
                                       .physics_to_world = config.physics_to_world,
                                       .render_alpha = &owner->render.alpha,
                                       .storage_bytes = config.sling_bytes},
            &owner->sling, error) &&
        ab_classic_game_draw_initialize(
            (AbClassicGameDrawConfig){.level = playfield.level,
                                      .visuals = playfield.visuals,
                                      .behaviors = playfield.behaviors,
                                      .scene = playfield.scene,
                                      .camera = playfield.camera,
                                      .sling = owner->sling,
                                      .powerups = config.powerups,
                                      .particles = config.particles,
                                      .provider = config.provider,
                                      .theme = &owner->theme,
                                      .theme_definition = config.theme,
                                      .local_settings = config.settings,
                                      .extra_draw_functions =
                                          config.extra_draw_functions,
                                      .state = config.state,
                                      .basic_font = config.basic_font,
                                      .current_font = config.current_font,
                                      .context = config.context,
                                      .editor = config.editor,
                                      .popup = config.popup,
                                      .restricted_borders = config.restricted_borders},
            &owner->draw, error);
    if (!okay) {
        ab_classic_game_render_close(owner);
        return false;
    }
    *output = owner;
    return true;
}

void ab_classic_game_render_close(AbClassicGameRender *owner) {
    if (!owner)
        return;
    ab_classic_sling_draw_close(owner->sling);
    ab_theme_layers_close(owner->layers);
    free(owner);
}

AbClassicGameDraw *ab_classic_game_render_draw(AbClassicGameRender *render) {
    return render ? &render->draw : NULL;
}

const AbThemeLayerRenderState *
ab_classic_game_render_state(const AbClassicGameRender *render) {
    return render ? &render->render : NULL;
}
