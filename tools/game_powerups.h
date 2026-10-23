#ifndef AB_GAME_POWERUPS_H
#define AB_GAME_POWERUPS_H

#include "ab/ui_powerup_effect.h"

struct PreviewPowerups {
    PreviewGame *game;
    AbClassicPowerupManager manager;
    AbClassicPowerupParticles *trails;
    AbUiWidget root;
    AbUiHudPowerups bar;
    AbUiPointerValue enabled[AB_CLASSIC_POWERUPS];
    AbUiFrameNode *frames[1];
    AbUiDrawChild draw[1];
    AbUiPointerChild pointer[1];
    char counters[AB_CLASSIC_POWERUPS][32];
    AbUiPowerupEffect effects[AB_CLASSIC_POWERUPS];
    bool effect_live[AB_CLASSIC_POWERUPS];
    size_t effect_count;
    AbClassicPowerupUpdate king, potion, quake, scope;
    AbClassicSuperSeed seed;
    float seed_timer;
    unsigned seed_stage;
    float king_timer, menu_particle_scale;
    float quake_remaining, quake_delay, quake_particles;
    bool quake_active, quake_sound;
    float scope_delay;
    bool scope_activated, scope_bound;
    struct {
        const AbSpriteRegion *region;
        uint32_t texture;
        float uv[8];
        bool alpha;
    } scope_images[2];
    bool entered, menu_particles_drawn;
};

#include "game_powerup_effects.h"

static bool game_powerups_notifications(void *context, bool *output, AbError *error) {
    (void)error;
    PreviewGame *game = context;
    *output = game->notifications.frame->child_count != 0 || game->menu->reward != NULL;
    return true;
}

static bool game_powerups_layout(PreviewGame *game, AbError *error) {
    if (!game->powerups || game->ended || !game->pause)
        return true;
    PreviewPowerups *powers = game->powerups;
    return ab_ui_hud_powerups_layout(
               &powers->bar, &game->pause->button.widget, game->pause->margin,
               game->eagle && ab_ui_pointer_value_truthy(
                                  &game->eagle->button.widget.frame->fields.visible),
               error) &&
           ab_ui_widget_lifecycle(&powers->root, AB_UI_FRAME_LAYOUT, error);
}

static bool game_powerups_open(PreviewGame *game, AbError *error) {
    PreviewMenu *menu = game->menu;
    if (!menu->powerup_catalog &&
        !ab_powerup_catalog_open(menu->archive, (AbSpan){menu->key, sizeof(menu->key)},
                                 (AbDataValue){0}, 1024 * 1024, &menu->powerup_catalog,
                                 error))
        return false;
    PreviewPowerups *powers = calloc(1, sizeof(*powers));
    if (!powers)
        return ab_fail(error, "Cannot allocate actual game powerup controls");
    game->powerups = powers;
    powers->game = game;
    powers->menu_particle_scale = 1;
    AbClassicGameRuntimeView runtime = ab_classic_game_runtime_view(game->game);
    AbClassicDynamicsView dynamics =
        ab_classic_dynamics_view(ab_classic_world_runtime_view(game->world).dynamics);
    powers->seed = (AbClassicSuperSeed){.playfield = game->playfield,
                                        .birds = runtime.birds,
                                        .physical = dynamics.physical,
                                        .simulation = dynamics.simulation,
                                        .context = game,
                                        .clear_updates = game_seed_clear_updates,
                                        .bird_added = game_seed_bird_added};
    size_t birds = ab_classic_level_view(game->playfield.level).bird_count;
    if (!ab_classic_powerup_particles_open(
            game->playfield.level, runtime.birds, game->particles, &menu->random,
            &game->physics, birds ? birds : 1, 2 * 1024 * 1024, &powers->trails, error))
        return false;
    if (!ab_classic_powerup_manager_initialize(
            (AbClassicPowerupManagerConfig){.catalog = menu->powerup_catalog,
                                            .settings = menu->settings,
                                            .events = menu->events,
                                            .uses = game->uses,
                                            .sling = game->playfield.sling,
                                            .flying = &game->flying,
                                            .specialty_available =
                                                &game->globals.specialty_available,
                                            .level_name = &game->level_name,
                                            .infinite = menu->infinite_powerups,
                                            .active = &game->active,
                                            .passive = &game->passive},
            &powers->manager, error))
        return false;
    powers->king =
        (AbClassicPowerupUpdate){.context = powers, .update = game_king_update};
    powers->manager.config.context = powers;
    powers->manager.config.activate = game_powerup_activate;
    powers->manager.config.instances[1] = &powers->king;
    powers->potion =
        (AbClassicPowerupUpdate){.context = powers, .update = game_seed_update};
    powers->manager.config.instances[0] = &powers->potion;
    powers->manager.config.reset[0] = game_powerup_reset;
    powers->quake =
        (AbClassicPowerupUpdate){.context = powers, .update = game_quake_update};
    powers->manager.config.instances[3] = &powers->quake;
    powers->manager.config.reset[3] = game_quake_reset;
    powers->scope = (AbClassicPowerupUpdate){
        .context = powers, .update = game_scope_update, .draw = game_scope_draw};
    powers->scope_delay = 1.5f;
    powers->manager.config.instances[2] = &powers->scope;
    powers->manager.config.reset[2] = game_scope_reset;
    for (size_t index = 0; index < AB_CLASSIC_POWERUPS; ++index)
        powers->enabled[index] = result_boolean(true);
    bool visible = !menu_route_truthy(
        menu_route_field(menu->level_request.episode, "powerup_button_disabled"));
    AbUiHudPowerupsHost host = {
        .slider = {.scaling = &menu->scaling,
                   .screen = &game->screen,
                   .context = game,
                   .notifications_visible = game_powerups_notifications},
        .fonts = &menu->fonts,
        .catalog = menu->powerup_catalog,
        .registered_enabled = powers->enabled,
        .device_model = menu_span("android"),
        .global_enabled = result_boolean(true)};
    if (!ab_ui_widget_init(
            &powers->root, &menu->context, AB_UI_WIDGET_FRAME,
            (AbUiWidgetChildren){powers->frames, powers->draw, powers->pointer, 1},
            NULL, NULL, NULL, error) ||
        !ab_ui_hud_powerups_init(
            &powers->bar, &powers->root, host,
            (AbUiHudPowerupsSource){.visible = result_boolean(visible),
                                    .birds_shot = game->shot.birds_shot,
                                    .bird_count = game->shot.bird_count},
            error) ||
        !game_powerups_layout(game, error) ||
        !ab_ui_widget_lifecycle(&powers->root, AB_UI_FRAME_ENTRY, error))
        return false;
    powers->entered = true;
    return !menu->keep_powerups_open || ab_ui_slider_open(&powers->bar.slider, error);
}

static void game_powerups_free(PreviewGame *game) {
    PreviewPowerups *powers = game->powerups;
    if (!powers)
        return;
    AbError ignored = {0};
    if (!game->notification_context.failed)
        for (size_t index = 0; index < AB_CLASSIC_POWERUPS; ++index)
            if (powers->effect_live[index])
                (void)ab_ui_frame_remove_self(&powers->effects[index].widget.tree,
                                              &ignored);
    if (powers->entered && !game->menu->context.failed)
        (void)ab_ui_widget_lifecycle(&powers->root, AB_UI_FRAME_EXIT, &ignored);
    ab_classic_powerup_particles_close(powers->trails);
    free(powers);
    game->powerups = NULL;
}

static bool game_powerups_pointer(PreviewGame *game, AbSpan action, AbVec2 point,
                                  bool *consumed, AbError *error) {
    *consumed = false;
    if (!game->powerups || game->paused || game->ended)
        return true;
    PreviewPowerups *powers = game->powerups;
    AbUiPointerReply reply;
    if (!ab_ui_widget_pointer(&powers->root, action, point, &reply, error))
        return false;
    *consumed = reply.values[0].kind != AB_UI_POINTER_NONE;
    if (menu_equal(reply.values[0], "TOGGLE_POWERUPS")) {
        game->menu->keep_powerups_open = !game->menu->keep_powerups_open;
    } else if (menu_equal(reply.values[0], "ACTIVATE_POWERUP")) {
        if (reply.values[1].kind != AB_UI_POINTER_OBJECT)
            return ab_fail(error, "Powerup button lost its actual metadata");
        const AbDataTable *meta = (const AbDataTable *)reply.values[1].object;
        AbDataValue name = menu_route_field(meta, "name");
        for (size_t index = 0; index < powers->bar.count; ++index) {
            const AbPowerupMetadata *row =
                ab_powerup_catalog_at(game->menu->powerup_catalog, index);
            if (!menu_route_equal(
                    name, (AbDataValue){.kind = AB_DATA_STRING, .string = row->name}))
                continue;
            float amount;
            if (!ab_settings_inventory_amount(game->menu->settings, row->name, &amount,
                                              error))
                return false;
            if (amount > 0 || powers->manager.infinite[index]) {
                bool activated;
                if (!ab_classic_powerup_manager_activate(
                        &powers->manager, index, (AbDataValue){0}, &activated, error))
                    return false;
                if (activated &&
                    !ab_ui_hud_powerup_enable(powers->bar.powerups + index,
                                              result_boolean(false), error))
                    return false;
                game->save_pending = true;
            }
            break;
        }
    }
    if (*consumed)
        game->input.skip_trigger_specialty = game->enabled;
    return true;
}

static bool game_powerups_draw(PreviewGame *game, float seconds, AbError *error) {
    if (!game->powerups || game->ended || game->paused)
        return true;
    PreviewPowerups *powers = game->powerups;
    powers->menu_particles_drawn = false;
    for (size_t index = 0; index < powers->bar.count; ++index) {
        AbUiHudPowerup *button = powers->bar.powerups + index;
        if (!button->present)
            continue;
        bool can_activate;
        float amount;
        if (!ab_settings_inventory_amount(game->menu->settings, button->metadata->name,
                                          &amount, error) ||
            !ab_classic_powerup_manager_can_activate(&powers->manager, index,
                                                     &can_activate, error) ||
            !ab_ui_hud_powerup_enable(button, result_boolean(can_activate), error))
            return false;
        if (powers->manager.infinite[index])
            button->counter.state.fields.text = menu_text("\xe2\x88\x9e");
        else {
            int count =
                snprintf(powers->counters[index], sizeof(powers->counters[index]), "%g",
                         (double)fminf(amount, 9999));
            if (count < 1 || (size_t)count >= sizeof(powers->counters[index]))
                return ab_fail(error, "Powerup counter exceeds its text storage");
            button->counter.state.fields.text = menu_text(powers->counters[index]);
        }
    }
    /* GameHud.updatePowerupCounters18850 reopens the slider from the retained
     * toggle preference after refreshing all counters. */
    if (game->menu->keep_powerups_open &&
        !ab_ui_slider_open(&powers->bar.slider, error))
        return false;
    AbUiDrawInput input = {.transform.scale = {1, 1}, .present = 12};
    if (!ab_ui_widget_update(&powers->root, seconds, game->menu->time, error) ||
        !ab_ui_widget_draw(&powers->root, &input, error) ||
        !ab_ui_widget_update(&game->notifications, seconds, game->menu->time, error))
        return false;
    for (size_t index = 0; index < AB_CLASSIC_POWERUPS; ++index)
        if (powers->effect_live[index] && powers->effects[index].timer >= 1.25f) {
            if (!ab_ui_frame_remove_self(&powers->effects[index].widget.tree, error))
                return false;
            powers->effect_live[index] = false;
            --powers->effect_count;
        }
    return ab_ui_widget_draw(&game->notifications, &input, error) &&
           game_powerup_menu_particles(powers, error);
}

#endif
