#ifndef AB_GAME_BIRDQUAKE_H
#define AB_GAME_BIRDQUAKE_H

#include "ab/script_math.h"

static void game_quake_reset_amplitude(PreviewGame *game, float amount) {
    game->shake.amplitude = amount;
    game->shake.present |= AB_CAMERA_SHAKE_AMPLITUDE;
}

static bool game_quake_reset(void *context, AbError *error) {
    (void)error;
    PreviewPowerups *powers = context;
    powers->quake_active = false;
    powers->quake_remaining = 0;
    powers->quake_particles = 0;
    powers->game->prevent_ending = powers->game->disabled;
    game_quake_reset_amplitude(powers->game, 0);
    return true;
}

static bool game_quake_activate(PreviewPowerups *powers, AbError *error) {
    PreviewGame *game = powers->game;
    game->prevent_ending = game->enabled;
    powers->quake_active = true;
    powers->quake_remaining = 2;
    powers->quake_delay = 1;
    powers->quake_particles = 0;
    powers->quake_sound = false;
    /* cancelLevelEnding88106 does not undo completed, scores or bonuses. */
    game->finish.complete.ending_timer = 0;
    game->finish.failed_timer = 0;
    game->finish.complete.completing = false;
    game->will_end = game->disabled;
    return ab_classic_camera_start_sequence(game->playfield.camera,
                                            "BirdquakeCameraSequence", error) &&
           game_powerup_notification(powers, "UI_POWERUPS_BIRDQUAKE", error);
}

static bool game_quake_pigs(PreviewGame *game, AbError *error) {
    AbClassicWorldRuntimeView world = ab_classic_world_runtime_view(game->world);
    AbClassicGoals *goals = ab_classic_dynamics_view(world.dynamics).goals;
    AbClassicCharacters *characters =
        ab_classic_game_runtime_view(game->game).characters;
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    size_t cursor = 0;
    AbStringMapEntry entry;
    for (;;) {
        if (!ab_string_map_next(ab_classic_goals_map(goals), &cursor, &entry, error))
            return false;
        if (!entry.present)
            return true;
        size_t object = 0;
        while (object < level.bodies->count) {
            AbSpan name = level.bodies->objects[object].name;
            if (name.size == entry.key.size &&
                !memcmp(name.data, entry.key.data, name.size))
                break;
            ++object;
        }
        if (object == level.bodies->count)
            return ab_fail(error, "Birdquake has a foreign level-goal name");
        if (!ab_classic_characters_special_expression(
                characters, object, menu_span("scaredSprite1"), 1, error))
            return false;
    }
}

static bool game_quake_menu_dust(PreviewPowerups *powers, AbError *error) {
    PreviewGame *game = powers->game;
    AbCameraViewport viewport;
    AbVec2 origin;
    float choice;
    if (!ab_camera_system_query_viewport(
            ab_classic_camera_system(game->playfield.camera), &viewport) ||
        !ab_coordinate_point(&viewport.rendering, AB_SPACE_WORLD, AB_SPACE_SCREEN,
                             (AbVec2){0}, &origin))
        return ab_fail(error, "Birdquake requires its current camera viewport");
    powers->menu_particle_scale =
        ab_classic_camera_view(game->playfield.camera).system.mover->current_zoom;
    /* The source selects from a one-element list and still consumes this draw. */
    if (ab_script_random(&game->menu->random, 2, 1, 1, &choice) != AB_SCRIPT_RANDOM_OK)
        return ab_fail(error, "Birdquake particle selection RNG failed");
    if (choice != 1)
        return ab_fail(error, "Birdquake selected beyond its source particle list");
    AbParticleEmission emission = {
        .definition = menu_span("quake"),
        .amount = 1,
        .position = {game->screen.x * 0.5f,
                     origin.y + 0.5f * (game->screen.y - origin.y)},
        .size = {game->screen.x, game->screen.y - origin.y},
        .ignore_limits = true,
        .alternate = true};
    AbParticles *particles = ab_classic_particles_engine(game->particles);
    if (!ab_particles_emit(particles, &emission, error))
        return false;
    emission.definition = menu_span("quake_smoke");
    emission.position.y = origin.y - 1;
    emission.size.y = 2;
    return ab_particles_emit(particles, &emission, error);
}

static bool game_quake_object(PreviewPowerups *powers, AbClassicLevelView level,
                              size_t object, float seconds, size_t bursts,
                              AbError *error) {
    PreviewGame *game = powers->game;
    AbDataTable *physical = &level.properties->objects[object].physical;
    if (game->flying.kind == AB_DATA_TABLE && game->flying.table == physical)
        return true;
    float density, mass, x, y;
    if (!game_powerup_number(menu_route_field(physical, "density"), &density, error))
        return false;
    if (density > 0) {
        float cosine;
        if (!ab_script_cosine(powers->quake_remaining * 0x1.921fb6p+1f, &cosine) ||
            !game_powerup_number(menu_route_field(physical, "mass"), &mass, error) ||
            !game_powerup_number(menu_route_field(physical, "x"), &x, error) ||
            !game_powerup_number(menu_route_field(physical, "y"), &y, error))
            return false;
        float impulse_x = seconds * cosine;
        impulse_x = (impulse_x * mass) * 12;
        float impulse_y = (seconds * -15) * mass;
        size_t body = level.bodies->objects[object].physics.body;
        if (!ab_world_has_body(level.world, body) ||
            !ab_body_impulse_apply(&level.world->storage.bodies[body].motion,
                                   (AbVec2){impulse_x, impulse_y}, &(AbVec2){x, y}))
            return ab_fail(error, "Birdquake cannot apply its native body impulse");
    }
    AbDataValue definition;
    if (!ab_object_definition_lookup(&level.properties->lookup,
                                     level.bodies->objects[object].name, &definition,
                                     error))
        return false;
    if (definition.kind != AB_DATA_TABLE)
        return ab_fail(error, "Birdquake requires the actual object definition");
    AbDataValue group = menu_route_field(definition.table, "group");
    const char *particle =
        menu_route_equal(group, game_text("woodBlocks"))    ? "woodenBuff"
        : menu_route_equal(group, game_text("rockBlocks"))  ? "rockBuff"
        : menu_route_equal(group, game_text("lightBlocks")) ? "lightBuff"
                                                            : NULL;
    if (!particle)
        return true;
    for (size_t burst = 0; burst < bursts; ++burst) {
        float random;
        if (ab_script_random(&game->menu->random, 0, 0, 0, &random) !=
            AB_SCRIPT_RANDOM_OK)
            return ab_fail(error, "Birdquake material particle RNG failed");
        if (!(random < 0.03f))
            continue;
        float width = 0, height = 0;
        AbDataValue raw_width = menu_route_field(physical, "width");
        AbDataValue raw_height = menu_route_field(physical, "height");
        if (menu_route_truthy(raw_width) && menu_route_truthy(raw_height)) {
            if (!game_powerup_number(raw_width, &width, error) ||
                !game_powerup_number(raw_height, &height, error))
                return false;
        } else {
            AbDataValue raw_radius = menu_route_field(physical, "radius");
            if (menu_route_truthy(raw_radius)) {
                if (!game_powerup_number(raw_radius, &width, error))
                    return false;
                width *= 2;
                height = width;
            }
        }
        float angle;
        if (!game_powerup_number(menu_route_field(physical, "x"), &x, error) ||
            !game_powerup_number(menu_route_field(physical, "y"), &y, error) ||
            !game_powerup_number(menu_route_field(physical, "angle"), &angle, error))
            return false;
        AbParticleEmission emission = {.definition = menu_span(particle),
                                       .amount = 1,
                                       .position = {x * 20, y * 20},
                                       .size = {width * 20, height * 20},
                                       .angle = angle};
        if (!ab_particles_emit(ab_classic_particles_engine(game->particles), &emission,
                               error))
            return false;
    }
    return true;
}

static bool game_quake_update(void *context, float seconds, AbError *error) {
    PreviewPowerups *powers = context;
    PreviewGame *game = powers->game;
    if (powers->quake_delay > 0) {
        powers->quake_delay -= seconds * game->speed;
        if (!ab_classic_camera_start_sequence(game->playfield.camera,
                                              "BirdquakeCameraSequence", error))
            return false;
        game->globals.eagle_darkness =
            menu_route_number((1 - powers->quake_delay) * 0.5f);
        return true;
    }
    if (!game_quake_pigs(game, error))
        return false;
    seconds = fminf(seconds * game->speed, powers->quake_remaining);
    powers->quake_remaining = fmaxf(powers->quake_remaining - seconds, 0);
    if (powers->quake_remaining == 0) {
        powers->quake_active = false;
        if (!ab_classic_powerup_manager_deactivate(&powers->manager, 3, false, error))
            return false;
        game_quake_reset_amplitude(game, 0);
        return true;
    }
    size_t bursts = 0;
    powers->quake_particles += seconds;
    while (powers->quake_particles > 0.05f) {
        ++bursts;
        powers->quake_particles -= 0.05f;
        if (!game_quake_menu_dust(powers, error))
            return false;
    }
    game_quake_reset_amplitude(game, powers->quake_remaining * 25 * game->speed);
    game->globals.eagle_darkness = menu_route_number(100);
    if (!powers->quake_sound) {
        if (!game_powerup_audio(game, "earthquake", error))
            return false;
        powers->quake_sound = true;
    }
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    AbClassicWorldCursor cursor = {0};
    for (;;) {
        size_t object;
        bool present;
        if (!ab_classic_level_world_next(game->playfield.level, &cursor, &object,
                                         &present, error))
            return false;
        if (!present)
            return true;
        if (!game_quake_object(powers, level, object, seconds, bursts, error))
            return false;
    }
}

#endif
