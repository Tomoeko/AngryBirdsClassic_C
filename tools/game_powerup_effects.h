#ifndef AB_GAME_POWERUP_EFFECTS_H
#define AB_GAME_POWERUP_EFFECTS_H

static bool game_powerup_menu_particles(void *context, AbError *error) {
    PreviewPowerups *powers = context;
    if (powers->menu_particles_drawn)
        return true;
    PreviewGame *game = powers->game;
    AbParticleDrawStyle style = {.provider = &game->menu->sprites.provider,
                                 .width = (uint32_t)game->screen.x,
                                 .height = (uint32_t)game->screen.y,
                                 .blend = AB_BLEND_PREMULTIPLIED,
                                 .color = {1, 1, 1, 1},
                                 .opacity = 1};
    if (!ab_particles_draw_menu(ab_classic_particles_engine(game->particles), style,
                                powers->menu_particle_scale,
                                game->menu->context.draw_list, error))
        return false;
    powers->menu_particles_drawn = true;
    return true;
}

static bool game_powerup_notification(PreviewPowerups *powers, const char *sprite,
                                      AbError *error) {
    PreviewGame *game = powers->game;
    size_t slot = 0;
    while (slot < AB_CLASSIC_POWERUPS && powers->effect_live[slot])
        ++slot;
    if (slot == AB_CLASSIC_POWERUPS)
        return ab_fail(error,
                       "Powerup notifications exceed registered effect capacity");
    AbUiPowerupEffect *effect = powers->effects + slot;
    AbVec2 size;
    if (!game->menu->context.images.bounds(game->menu->context.images.context,
                                           menu_span(sprite), &size, error) ||
        !ab_ui_powerup_effect_init(
            effect, &game->notification_context,
            (AbUiPowerupEffectHost){.scaling = &game->menu->scaling,
                                    .screen = &game->screen,
                                    .audio = game->menu->audio,
                                    .particles =
                                        ab_classic_particles_engine(game->particles),
                                    .shake = &game->shake,
                                    .menu_particle_scale = &powers->menu_particle_scale,
                                    .context = powers,
                                    .draw_menu_particles = game_powerup_menu_particles},
            menu_span(sprite), menu_span("powerupBuff"), size, error) ||
        !ab_ui_widget_add(&game->notifications, &effect->widget, error))
        return false;
    powers->effect_live[slot] = true;
    ++powers->effect_count;
    return true;
}

static bool game_powerup_audio(PreviewGame *game, const char *name, AbError *error) {
    AbDataValue resolved;
    uint32_t handle;
    return ab_audio_bank_name_value(game->menu->audio, game_text(name), &resolved,
                                    error) &&
           ab_audio_bank_play(game->menu->audio,
                              (AbAudioPlay){.name = resolved,
                                            .volume = {.kind = AB_DATA_NUMBER,
                                                       .bits = UINT32_C(0x3f800000)},
                                            .loop = {.kind = AB_DATA_BOOLEAN}},
                              &handle, error);
}

static bool game_sling_particles(PreviewGame *game, const char *definition,
                                 float amount, bool top, AbError *error) {
    AbSlingshotStartPose pose = ab_classic_sling_start_pose(game->playfield.sling);
    AbVec2 position = top ? pose.level_start : pose.base;
    AbParticleEmission emission = {.definition = menu_span(definition),
                                   .amount = amount,
                                   .position = {position.x * 20, position.y * 20},
                                   .ignore_limits = true};
    return ab_particles_emit(ab_classic_particles_engine(game->particles), &emission,
                             error);
}

static bool game_king_update(void *context, float seconds, AbError *error) {
    PreviewPowerups *powers = context;
    if (powers->king_timer > 0) {
        powers->king_timer -= seconds;
        return true;
    }
    PreviewGame *game = powers->game;
    bool scope;
    return ab_classic_powerup_manager_in_use(&powers->manager, 2, &scope, error) &&
           ab_classic_sling_set_king(game->playfield.sling, scope, error) &&
           game_powerup_audio(game, "super_slingshot", error) &&
           game_sling_particles(game, "superSlingActivation", 1, false, error) &&
           game_sling_particles(game, "superSlingBurst", 40, false, error) &&
           ab_classic_powerup_manager_deactivate(&powers->manager, 1, false, error);
}

static bool game_powerup_trails(void *context, size_t object, bool potion, bool king,
                                AbError *error) {
    PreviewGame *game = context;
    if (!game->powerups)
        return ab_fail(error, "Powered launch has no actual trail owner");
    return (!potion ||
            ab_classic_powerup_particles_add(game->powerups->trails, object,
                                             AB_POWERUP_PARTICLES_POTION, error)) &&
           (!king ||
            ab_classic_powerup_particles_add(game->powerups->trails, object,
                                             AB_POWERUP_PARTICLES_KING, error));
}

static bool game_powerup_number(AbDataValue value, float *output, AbError *error);

#include "game_birdquake.h"
#include "game_slingscope.h"

static bool game_powerup_activate(void *context, size_t index, bool *result,
                                  AbError *error) {
    PreviewPowerups *powers = context;
    if (index == 0) {
        powers->seed_stage = 1;
        powers->seed_timer = 1.5f;
        *result = true;
        return game_powerup_notification(powers, "UI_POWERUPS_SUPERSEEDS", error) &&
               ab_classic_camera_start_sequence(powers->game->playfield.camera,
                                                "AngerActivationCameraSequence", error);
    }
    if (index == 3) {
        *result = true;
        return game_quake_activate(powers, error);
    }
    if (index == 2) {
        powers->scope_delay = 1.5f;
        powers->scope_activated = false;
        *result = false;
        return game_powerup_notification(powers, "UI_POWERUPS_SLINGSCOPE", error) &&
               ab_classic_camera_start_sequence(powers->game->playfield.camera,
                                                "AngerActivationCameraSequence", error);
    }
    if (index != 1)
        return ab_fail(error, "Unbound powerup activation effect");
    powers->king_timer = 1.5f;
    *result = false;
    return game_powerup_notification(powers, "UI_POWERUPS_KINGSLING", error) &&
           ab_classic_camera_start_sequence(powers->game->playfield.camera,
                                            "AngerActivationCameraSequence", error);
}

static bool game_seed_clear_updates(void *context, size_t object, AbError *error) {
    PreviewGame *game = context;
    if (game->tail.hooks)
        for (size_t index = 0; index < game->tail.hooks->count; ++index) {
            AbGameTailHook *hook = game->tail.hooks->items + index;
            if (hook->object && hook->object->identity == object &&
                !ab_object_update_clear(hook->updates, error))
                return false;
        }
    return true;
}

static bool game_seed_bird_added(void *context, size_t object, AbDataValue hook,
                                 AbError *error) {
    (void)object;
    PreviewGame *game = context;
    if (!menu_route_equal(hook, game_text("redBirdAdded")) ||
        game->tail.red_special_available)
        return ab_fail(error,
                       "Super Seeds requires its recovered active bird-added hook");
    AbSpan path[] = {menu_span("tutorials"), menu_span("BIRD_RED_FEATHERS"),
                     menu_span("showHelp")};
    AbDataValue tutorials = menu_route_field(
        ab_settings_statistics_root(game->menu->settings), "tutorials");
    if (tutorials.kind == AB_DATA_NIL) {
        if (!ab_settings_statistics_create_table(game->menu->settings, path, 1, 0,
                                                 error))
            return false;
        tutorials = menu_route_field(ab_settings_statistics_root(game->menu->settings),
                                     "tutorials");
    }
    if (tutorials.kind != AB_DATA_TABLE)
        return ab_fail(error, "Red bird-added hook requires its tutorial table");
    AbDataValue item = menu_route_field(tutorials.table, "BIRD_RED_FEATHERS");
    if (!menu_route_truthy(item))
        return true;
    return (item.kind == AB_DATA_TABLE ||
            ab_fail(error, "Red bird tutorial is not a table")) &&
           ab_settings_statistics_write(game->menu->settings, path, 3,
                                        (AbDataValue){.kind = AB_DATA_BOOLEAN}, error);
}

static bool game_seed_audio(PreviewGame *game, AbError *error) {
    AbCameraSystemView view = ab_classic_camera_view(game->playfield.camera).system;
    if (!view.limits || view.limits->snap_count != 2)
        return ab_fail(error, "Super Seeds audio requires its bird/castle snap points");
    AbVec2 bird = view.limits->snaps[0].position;
    AbVec2 castle = view.limits->snaps[1].position;
    float x = castle.x - bird.x, y = castle.y - bird.y;
    float total = (float)sqrt((double)(x * x + y * y));
    x = view.screen_position.x - bird.x;
    y = view.screen_position.y - bird.y;
    float distance = (float)sqrt((double)(x * x + y * y));
    float gain = fminf(1, fmaxf(0.25f, 1 - distance / total));
    AbDataValue resolved;
    uint32_t handle;
    return ab_audio_bank_name_value(game->menu->audio, game_text("big_bird"), &resolved,
                                    error) &&
           ab_audio_bank_play(game->menu->audio,
                              (AbAudioPlay){.name = resolved,
                                            .volume = menu_route_number(gain),
                                            .loop = {.kind = AB_DATA_BOOLEAN}},
                              &handle, error);
}

static bool game_powerup_number(AbDataValue value, float *output, AbError *error) {
    memcpy(output, &value.bits, sizeof(*output));
    return (value.kind == AB_DATA_NUMBER && isfinite(*output)) ||
           ab_fail(error, "Powerup effect requires a finite physical position");
}

static bool game_seed_update(void *context, float seconds, AbError *error) {
    PreviewPowerups *powers = context;
    PreviewGame *game = powers->game;
    if (!powers->seed_stage)
        return true;
    game->input.skip_input = game->enabled;
    if (powers->seed_timer > 0) {
        powers->seed_timer -= seconds;
        return true;
    }
    if (powers->seed_stage != 1) {
        powers->seed_stage = 0;
        return ab_classic_powerup_manager_deactivate(&powers->manager, 0, false, error);
    }
    powers->seed_stage = 2;
    powers->seed_timer = 0.5f;
    size_t object;
    AbClassicSlingView sling = ab_classic_sling_view(game->playfield.sling);
    if (!ab_classic_sling_next_bird(game->playfield.sling, sling.current_bird_index,
                                    &object, error))
        return false;
    if (object == SIZE_MAX)
        return ab_fail(error, "Super Seeds activation lost its queued bird");
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    AbDataTable *physical = &level.properties->objects[object].physical;
    if (!menu_route_truthy(menu_route_field(physical, "shot"))) {
        if (!ab_classic_superseed_apply(&powers->seed, object, error))
            return false;
        float x, y;
        if (!game_powerup_number(menu_route_field(physical, "x"), &x, error) ||
            !game_powerup_number(menu_route_field(physical, "y"), &y, error))
            return false;
        AbParticleEmission emission = {.definition = menu_span("superSeedActivation"),
                                       .amount = 1,
                                       .position = {x * 20, y * 20},
                                       .ignore_limits = true};
        if (!ab_particles_emit(ab_classic_particles_engine(game->particles), &emission,
                               error))
            return false;
        emission.definition = menu_span("powerPotionExplosion");
        emission.amount = 40;
        if (!ab_particles_emit(ab_classic_particles_engine(game->particles), &emission,
                               error) ||
            !ab_classic_powerup_particles_add(powers->trails, object,
                                              AB_POWERUP_PARTICLES_BUBBLES, error))
            return false;
    }
    return game_seed_audio(game, error);
}

static bool game_powerup_reset(void *context, AbError *error) {
    (void)error;
    PreviewPowerups *powers = context;
    powers->seed_stage = 0;
    return true;
}

static bool game_powerup_bird_update(void *context, AbClassicBirdUpdate kind,
                                     size_t object, float seconds, bool *alive,
                                     AbError *error) {
    PreviewGame *game = context;
    if (kind != AB_CLASSIC_BIRD_SUPERSEED || !game->powerups)
        return ab_fail(error, "Missing actual timed bird specialty continuation");
    return ab_classic_superseed_update(&game->powerups->seed, object, seconds, alive,
                                       error);
}

#endif
