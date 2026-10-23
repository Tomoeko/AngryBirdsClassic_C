#include "ab/classic_game_runtime.h"

#include <stdlib.h>

struct AbClassicGameRuntime {
    AbClassicGameRuntimeConfig config;
    AbClassicPlayfieldView playfield;
    AbClassicWorldRuntimeView world;
    AbClassicDynamicsView dynamics;
    AbClassicGameGlobals *globals;
    AbClassicGameRuntimeView view;
    AbClassicTouches *touches;
    AbClassicCharacterServices conditions;
    AbClassicGameEffects effects;
    AbClassicGamePrelude prelude;
    AbClassicGameStart start;
    AbClassicGamePowerups powerups;
    AbClassicGameLevelParticles level_particles;
    AbClassicGameInput input;
    AbClassicGameUpdate update;
    AbClassicBoomerang boomerang;
    AbClassicBluesSplit blues;
};

static bool budget(AbClassicGameRuntimeConfig *config, AbError *error) {
    const size_t parts[] = {
        sizeof(AbClassicGameRuntime),    config->birds.storage_bytes,
        config->characters_bytes,        config->touches_bytes,
        config->finish.storage_bytes,    config->ending.storage_bytes,
        config->progress.storage_bytes,  config->specialty.storage_bytes,
        config->controller.storage_bytes};
    size_t remaining = config->storage_bytes;
    for (size_t index = 0; index < sizeof(parts) / sizeof(*parts); ++index) {
        if (!parts[index] || parts[index] > remaining)
            return ab_fail(error, "Invalid or exhausted GameUpdate runtime budget");
        remaining -= parts[index];
    }
    if (config->eggs.storage_bytes > remaining)
        return ab_fail(error, "White bird egg owner exceeds GameUpdate runtime budget");
    return true;
}

static bool open_birds(AbClassicGameRuntime *runtime, AbError *error) {
    AbClassicGameRuntimeConfig *config = &runtime->config;
    AbClassicPlayfieldView playfield = runtime->playfield;
    AbClassicBirdConfig birds = config->birds;
    birds.level = playfield.level;
    birds.shots = playfield.shots;
    birds.behaviors = playfield.behaviors;
    birds.camera = playfield.camera;
    birds.physical = runtime->dynamics.physical;
    birds.particles = config->particles;
    birds.audio = config->audio;
    birds.explosions = config->explosions;
    birds.object_capacity = runtime->world.object_capacity;
    birds.hats = runtime->world.hats;
    birds.flying_object = config->flying;
    birds.specialty_available = &runtime->globals->specialty_available;
    if (!ab_classic_birds_open(birds, &runtime->view.birds, error))
        return false;
    return ab_classic_playfield_bind_other_birds(
        config->playfield, ab_classic_birds_view(runtime->view.birds).other_birds_field,
        error);
}

static bool open_characters(AbClassicGameRuntime *runtime, AbError *error) {
    AbClassicGameRuntimeConfig *config = &runtime->config;
    AbClassicPlayfieldView playfield = runtime->playfield;
    AbClassicCharacterServiceConfig conditions = config->conditions;
    conditions.level = playfield.level;
    conditions.camera = playfield.camera;
    conditions.sling = playfield.sling;
    conditions.birds = runtime->view.birds;
    conditions.audio = config->audio;
    conditions.goals = runtime->dynamics.goals;
    conditions.level_timeout = &runtime->globals->level_timeout;
    conditions.moving_objects = &runtime->globals->has_moving_objects;
    conditions.do_not_wait_for_moving_objects = &config->ending.state->do_not_wait;
    conditions.do_not_timeout = config->prelude.do_not_timeout;
    return ab_classic_character_services_initialize(conditions, &runtime->conditions,
                                                    error) &&
           ab_classic_characters_open_capacity(
               playfield.level, playfield.visuals, config->random,
               ab_classic_character_services_host(&runtime->conditions),
               runtime->world.object_capacity, config->characters_bytes,
               &runtime->view.characters, error);
}

static bool open_progress(AbClassicGameRuntime *runtime, AbError *error) {
    AbClassicGameRuntimeConfig *config = &runtime->config;
    AbClassicPlayfieldView playfield = runtime->playfield;
    AbClassicLevelFinishConfig finish = config->finish;
    finish.shots = playfield.shots;
    finish.sling = playfield.sling;
    finish.camera = playfield.camera;
    finish.audio = config->audio;
    finish.events = config->events;
    if (!ab_classic_level_finish_open(finish, &runtime->view.finish, error))
        return false;
    AbClassicLevelEndingConfig ending = config->ending;
    ending.shots = playfield.shots;
    ending.sling = playfield.sling;
    ending.scores = runtime->world.scores;
    ending.floating = runtime->world.floating;
    ending.audio = config->audio;
    ending.events = config->events;
    ending.conditions = &runtime->conditions;
    ending.finish = finish.state;
    ending.eagle_launched = config->conditions.eagle_launched;
    ending.bird_sound = config->conditions.bird_sound;
    if (!ab_classic_level_ending_open(ending, &runtime->view.ending, error))
        return false;
    AbClassicGameProgressConfig progress = config->progress;
    progress.finish = runtime->view.finish;
    progress.ending = runtime->view.ending;
    progress.finish_state = finish.state;
    progress.conditions = &runtime->conditions;
    progress.events = config->events;
    progress.cheats_enabled = &config->input.state->cheats_enabled;
    return ab_classic_game_progress_open(progress, &runtime->view.progress, error);
}

static bool blues_created(void *context, const AbClassicBluesSplitResult *result,
                          AbError *error) {
    AbClassicGameRuntime *runtime = context;
    for (size_t child = 0; child < 3; ++child)
        if (!ab_classic_world_runtime_register_circle(runtime->config.world,
                                                      result->objects[child], error) ||
            !ab_classic_controller_register_object(runtime->view.controller,
                                                   result->objects[child], error))
            return false;
    if (!ab_classic_characters_register_objects(runtime->view.characters, error))
        return false;
    runtime->world = ab_classic_world_runtime_view(runtime->config.world);
    if (runtime->config.explosions)
        runtime->config.explosions->physical_count = runtime->world.physical_count;
    return true;
}

static bool egg_created(void *context, size_t object, AbError *error) {
    AbClassicGameRuntime *runtime = context;
    if (!ab_classic_world_runtime_register_circle(runtime->config.world, object,
                                                  error) ||
        !ab_classic_controller_register_object(runtime->view.controller, object,
                                               error) ||
        !ab_classic_characters_register_objects(runtime->view.characters, error))
        return false;
    runtime->world = ab_classic_world_runtime_view(runtime->config.world);
    runtime->config.explosions->physical_count = runtime->world.physical_count;
    return true;
}

static bool open_input(AbClassicGameRuntime *runtime, AbError *error) {
    AbClassicGameRuntimeConfig *config = &runtime->config;
    AbClassicPlayfieldView playfield = runtime->playfield;
    AbClassicBirdSpecialtyConfig specialty = config->specialty;
    specialty.shots = playfield.shots;
    specialty.behaviors = playfield.behaviors;
    specialty.audio = config->audio;
    specialty.events = config->events;
    specialty.flying = config->flying;
    specialty.settings = config->settings;
    specialty.boost.level = playfield.level;
    specialty.boost.visuals = playfield.visuals;
    specialty.boost.trajectories = playfield.trajectories;
    specialty.boost.particles = config->particles;
    specialty.boost.audio = config->audio;
    specialty.boost.settings = config->settings;
    runtime->boomerang = config->boomerang;
    runtime->boomerang.level = playfield.level;
    runtime->boomerang.birds = runtime->view.birds;
    runtime->boomerang.sling = playfield.sling;
    runtime->boomerang.visuals = playfield.visuals;
    runtime->boomerang.trajectories = playfield.trajectories;
    runtime->boomerang.audio = config->audio;
    runtime->boomerang.settings = config->settings;
    runtime->boomerang.forces =
        ab_classic_simulation_view(runtime->dynamics.simulation).queue;
    runtime->boomerang.flying = config->flying;
    runtime->boomerang.specialty_available = &runtime->globals->specialty_available;
    runtime->view.boomerang = &runtime->boomerang;
    specialty.boomerang = &runtime->boomerang;
    specialty.explosions = config->explosions;
    specialty.birds = runtime->view.birds;
    if (config->eggs.storage_bytes) {
        AbClassicEggConfig eggs = config->eggs;
        eggs.playfield = playfield;
        eggs.birds = runtime->view.birds;
        eggs.physical = runtime->dynamics.physical;
        eggs.particles = config->particles;
        eggs.explosions = config->explosions;
        eggs.audio = config->audio;
        eggs.settings = config->settings;
        eggs.grenades = runtime->world.grenades;
        eggs.context = runtime;
        eggs.created = egg_created;
        if (!ab_classic_eggs_open(eggs, &runtime->view.eggs, error))
            return false;
        specialty.eggs = runtime->view.eggs;
    }
    if (config->blues_names.size) {
        if (specialty.blues || !config->blues_names.data ||
            !ab_classic_blues_split_initialize(
                (AbClassicBluesSplitConfig){.playfield = playfield,
                                            .birds = runtime->view.birds,
                                            .physical = runtime->dynamics.physical,
                                            .settings = config->settings,
                                            .audio = config->audio,
                                            .hats = runtime->world.hats,
                                            .teleporters = runtime->world.teleporters,
                                            .flying = config->flying,
                                            .names = config->blues_names},
                &runtime->blues, error))
            return error && error->message[0]
                       ? false
                       : ab_fail(error, "Conflicting actual Blues owners");
        runtime->view.blues = &runtime->blues;
        specialty.blues = &runtime->blues;
        specialty.blues_context = runtime;
        specialty.blues_created = blues_created;
    }
    if (!ab_classic_bird_specialty_open(specialty, &runtime->view.specialty, error) ||
        !ab_classic_touches_open(
            (AbClassicTouchesConfig){.level = playfield.level,
                                     .behaviors = playfield.behaviors,
                                     .camera =
                                         ab_classic_camera_system(playfield.camera),
                                     .capacity = config->touches,
                                     .storage_bytes = config->touches_bytes},
            &runtime->touches, error))
        return false;
    AbClassicGameInputConfig input = config->input;
    input.shots = playfield.shots;
    input.sling = playfield.sling;
    input.specialty = runtime->view.specialty;
    input.touches = runtime->touches;
    input.camera = ab_classic_camera_system(playfield.camera);
    input.tail = config->controller.state;
    input.flying = config->flying;
    input.level_completed = &config->finish.state->complete.completed;
    input.hats = runtime->world.hats;
    return ab_classic_game_input_initialize(input, &runtime->input, error);
}

static bool initialize_phases(AbClassicGameRuntime *runtime, AbError *error) {
    AbClassicGameRuntimeConfig *config = &runtime->config;
    AbClassicPlayfieldView playfield = runtime->playfield;
    AbClassicGameEffectConfig effects = config->effects;
    effects.random = config->random;
    effects.trajectories = playfield.trajectories;
    if (!ab_classic_game_effects_initialize(effects, &runtime->effects, error))
        return false;
    AbClassicGamePreludeConfig prelude = config->prelude;
    prelude.shots = playfield.shots;
    prelude.camera = playfield.camera;
    prelude.effects = &runtime->effects;
    prelude.settings = config->settings;
    prelude.flying = config->flying;
    prelude.cheats_enabled = &config->input.state->cheats_enabled;
    prelude.touch_count = &config->input.state->touch_count;
    prelude.cursor = &config->input.state->cursor;
    prelude.cursor_physics = &config->input.state->cursor_physics;
    if (!ab_classic_game_prelude_initialize(prelude, &runtime->prelude, error))
        return false;
    AbClassicGameStartConfig start = config->start;
    start.sling = playfield.sling;
    start.audio = config->audio;
    start.cheats_enabled = &config->input.state->cheats_enabled;
    start.keys = config->input.pressed;
    if (!ab_classic_game_start_initialize(start, &runtime->start, error))
        return false;
    AbClassicGamePowerupConfig powerups = config->powerups;
    powerups.sling = playfield.sling;
    powerups.cheats_enabled = &config->input.state->cheats_enabled;
    powerups.released_keys = config->input.released;
    powerups.skip_input = &config->input.state->skip_input;
    if (!ab_classic_game_powerups_initialize(powerups, &runtime->powerups, error))
        return false;
    AbClassicGameLevelParticleConfig particles = config->level_particles;
    particles.particles = ab_classic_particles_engine(config->particles);
    particles.physics_enabled = config->start.physics_enabled;
    return ab_classic_game_level_particles_initialize(particles,
                                                      &runtime->level_particles, error);
}

static bool open_controller(AbClassicGameRuntime *runtime, AbError *error) {
    AbClassicGameRuntimeConfig *config = &runtime->config;
    AbClassicPlayfieldView playfield = runtime->playfield;
    AbClassicBlockScriptView blocks =
        ab_classic_block_scripts_view(runtime->world.scripts);
    AbClassicControllerConfig controller = config->controller;
    controller.level = playfield.level;
    controller.eggs = runtime->view.eggs;
    if (controller.state)
        controller.state->grenade_count = &runtime->world.grenades->count;
    controller.shots = playfield.shots;
    controller.sling = playfield.sling;
    controller.birds = runtime->view.birds;
    controller.behaviors = playfield.behaviors;
    controller.camera = playfield.camera;
    controller.characters = runtime->view.characters;
    controller.scene = playfield.scene;
    controller.removals = runtime->dynamics.removals;
    controller.floating = runtime->world.floating;
    controller.scores = runtime->world.scores;
    controller.blocks = blocks.manager;
    controller.block_update = blocks.update;
    controller.physics_enabled = config->start.physics_enabled;
    controller.level_completed = &config->finish.state->complete.completed;
    controller.input = &runtime->input;
    controller.boomerang = &runtime->boomerang;
    controller.object_capacity = runtime->world.object_capacity;
    return ab_classic_controller_open(controller, &runtime->view.controller, error) &&
           ab_classic_game_update_initialize(
               (AbClassicGameUpdateConfig){.prelude = &runtime->prelude,
                                           .start = &runtime->start,
                                           .progress = runtime->view.progress,
                                           .powerups = &runtime->powerups,
                                           .particles = &runtime->level_particles,
                                           .input = &runtime->input,
                                           .controller = runtime->view.controller},
               &runtime->update, error);
}

bool ab_classic_game_runtime_open(AbClassicGameRuntimeConfig config,
                                  AbClassicGameRuntime **output, AbError *error) {
    if (!output)
        return ab_fail(error, "Missing GameUpdate runtime output");
    *output = NULL;
    AbClassicPlayfieldView playfield = ab_classic_playfield_view(config.playfield);
    AbClassicWorldRuntimeView world = ab_classic_world_runtime_view(config.world);
    AbClassicDynamicsView dynamics = ab_classic_dynamics_view(world.dynamics);
    if (!playfield.level || !dynamics.simulation ||
        ab_classic_simulation_view(dynamics.simulation).level != playfield.level ||
        ab_classic_particles_level(config.particles) != playfield.level ||
        config.start.physics_enabled != world.physics_enabled ||
        config.flying != world.flying || !ab_classic_shots_globals(playfield.shots) ||
        !config.audio || !config.settings || !config.events || !config.random ||
        !config.flying || !config.input.state || !config.finish.state ||
        !config.ending.state || !config.controller.state || !budget(&config, error))
        return error && error->message[0]
                   ? false
                   : ab_fail(error, "Invalid GameUpdate runtime source owners/state");
    const AbClassicExplosions *explosions = config.explosions;
    if (explosions &&
        (explosions->level != playfield.level ||
         explosions->visuals != playfield.visuals ||
         explosions->particles != config.particles ||
         explosions->audio != config.audio ||
         explosions->globals != ab_classic_shots_globals(playfield.shots) ||
         explosions->dead_blocks != world.dead_blocks ||
         explosions->physical_values != world.physical_values ||
         explosions->physical_count != world.physical_count))
        return ab_fail(error, "Explosion services do not share the actual game owners");
    AbClassicGameRuntime *runtime = calloc(1, sizeof(*runtime));
    if (!runtime)
        return ab_fail(error, "Cannot allocate GameUpdate runtime");
    runtime->config = config;
    runtime->playfield = playfield;
    runtime->world = world;
    runtime->dynamics = dynamics;
    runtime->globals = ab_classic_shots_globals(playfield.shots);
    if (!open_birds(runtime, error) || !open_characters(runtime, error) ||
        !open_progress(runtime, error) || !open_input(runtime, error) ||
        !initialize_phases(runtime, error) || !open_controller(runtime, error)) {
        ab_classic_game_runtime_close(runtime);
        return false;
    }
    runtime->view.conditions = &runtime->conditions;
    runtime->view.effects = &runtime->effects;
    runtime->view.input = &runtime->input;
    runtime->view.powerups = &runtime->powerups;
    runtime->view.update = &runtime->update;
    *output = runtime;
    return true;
}

void ab_classic_game_runtime_close(AbClassicGameRuntime *runtime) {
    if (!runtime)
        return;
    ab_classic_controller_close(runtime->view.controller);
    ab_classic_bird_specialty_close(runtime->view.specialty);
    ab_classic_eggs_close(runtime->view.eggs);
    ab_classic_touches_close(runtime->touches);
    ab_classic_game_progress_close(runtime->view.progress);
    ab_classic_level_ending_close(runtime->view.ending);
    ab_classic_level_finish_close(runtime->view.finish);
    ab_classic_characters_close(runtime->view.characters);
    if (runtime->view.birds)
        (void)ab_classic_playfield_bind_other_birds(runtime->config.playfield, NULL,
                                                    NULL);
    ab_classic_birds_close(runtime->view.birds);
    free(runtime);
}

AbClassicGameRuntimeView ab_classic_game_runtime_view(AbClassicGameRuntime *runtime) {
    return runtime ? runtime->view : (AbClassicGameRuntimeView){0};
}
