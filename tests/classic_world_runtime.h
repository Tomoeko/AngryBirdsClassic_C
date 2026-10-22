#ifndef AB_TEST_CLASSIC_WORLD_RUNTIME_H
#define AB_TEST_CLASSIC_WORLD_RUNTIME_H

#include "ab/classic_world_runtime.h"
#include "ab/classic_collision_achievements.h"
#include "ab/classic_collision_hooks.h"

typedef struct {
    AbClassicPlayfieldView playfield;
    AbClassicParticles *particles;
    AbClassicCollisionAchievements *achievements;
    AbClassicCollisionHooks hooks;
} FreshWorld;

static bool fresh_particle_sprite(void *context, AbSpan name, uintptr_t *output,
                                  AbError *error) {
    FreshWorld *world = context;
    const AbSpriteProvider *provider =
        ab_classic_visuals_view(world->playfield.visuals).provider;
    AbSpriteAtlasBinding sprite;
    if (!provider || !provider->sprite)
        return ab_fail(error, "Missing fresh-world sprite provider");
    *output = provider->sprite(provider->context, name, &sprite)
                  ? (uintptr_t)sprite.image
                  : 0;
    return true;
}

static bool fresh_sound(void *context, AbBlockCollisionSound kind, size_t object,
                        const AbObjectDefinition *definition, AbDataValue *output,
                        AbError *error) {
    (void)context;
    (void)object;
    (void)error;
    *output = ab_object_property(
        definition, span(kind == AB_BLOCK_COLLISION_CONTACT_SOUND ? "collisionSound"
                                                                  : "damageSound"));
    return true;
}

static bool fresh_achievement(void *context, size_t first, size_t second,
                              AbError *error) {
    return ab_classic_collision_achievements_call(((FreshWorld *)context)->achievements,
                                                  first, second, error);
}

static bool fresh_start_position(void *context, bool global_class, AbVec2 *output,
                                 AbError *error) {
    (void)global_class;
    (void)error;
    *output = ab_classic_sling_start_pose(((FreshWorld *)context)->playfield.sling)
                  .level_start;
    return true;
}

static bool fresh_print(void *context, AbUiPointerValue value, AbError *error) {
    (void)context;
    return fprintf(stderr, "Collision callback kind %u\n", (unsigned)value.kind) >= 0 ||
           ab_fail(error, "Cannot write collision diagnostic");
}

static bool fresh_binding(void *context, AbDataValue key, AbUiPointerValue *output,
                          AbError *error) {
    return ab_classic_collision_hooks_binding(&((FreshWorld *)context)->hooks, key,
                                              output, error);
}

static bool fresh_specialty(void *context, AbUiPointerValue function,
                            const size_t *flying,
                            const AbBirdCollisionNamedInput *contact, AbError *error) {
    return ab_classic_collision_hooks_call(&((FreshWorld *)context)->hooks, function,
                                           flying, contact, error);
}

static bool fresh_contact_particles(void *context, size_t object,
                                    AbDataValue descriptor, AbError *error) {
    return ab_classic_particles_contact(((FreshWorld *)context)->particles, object,
                                        descriptor, error);
}

static bool fresh_removal_level(void *context, AbDataValue name,
                                AbObjectRemovalLevel **output, AbError *error) {
    (void)context;
    /* level_scripts.lua's actual root has no Level1 registration. This is a
     * selected source lookup, not a policy for other levels or missing hosts. */
    if (name.kind != AB_DATA_STRING || name.string.size != 6 ||
        memcmp(name.string.data, "Level1", 6))
        return ab_fail(error, "Unexpected fresh-world level-script lookup");
    *output = NULL;
    return true;
}

#include "classic_game_runtime.h"

/* Exercise fresh world and GameUpdate construction with real input and draw
 * producers. Root services, GPU submission and complete GUI remain separate. */
static bool fresh_world_runtime(PlayfieldTest *test, AbClassicPlayfield *owner,
                                AbClassicGameGlobals *globals, AbDataValue *flying,
                                AbError *error) {
    AbClassicPlayfieldView playfield = ab_classic_playfield_view(owner);
    FreshWorld services = {.playfield = playfield};
    AbEngineRandom engine;
    size_t indices[64];
    AbStringOrderNode nodes[64];
    AbStringOrderWorkspace workspace = {indices, 64, nodes, 64};
    AbSpan particle_names[64];
    AbClassicParticleNames names = {particle_names, 0, 64, 16384};
    AbDataValue disabled = {.kind = AB_DATA_BOOLEAN}, absent = {0},
                enabled = {.kind = AB_DATA_BOOLEAN, .bits = 1},
                name = {.kind = AB_DATA_STRING, .string = span("Level1")},
                model = {.kind = AB_DATA_STRING, .string = span("android")},
                goal_sent = {0};
    float threshold = 3, scaling = 1, total_blocks = 0;
    bool physics = false;
    CHECK(ab_engine_random_seed(&engine, 7) &&
          ab_classic_particle_names(playfield.level, &names, error) && names.count &&
          ab_classic_particles_open(
              &test->archive,
              (AbClassicParticleConfig){.level = playfield.level,
                                        .definitions = particle_names,
                                        .definition_count = names.count,
                                        .resource_bytes = 16 * 1024 * 1024,
                                        .storage_bytes = 256 * 1024,
                                        .capacity = 512,
                                        .level_capacity = 512,
                                        .engine_random = &engine,
                                        .script_random = &test->random,
                                        .admission = {75, 150, 0.5f},
                                        .workspace = &workspace,
                                        .context = &services,
                                        .sprite = fresh_particle_sprite},
              &services.particles, error) &&
          ab_classic_collision_hooks_initialize(
              (AbClassicCollisionHookConfig){.red_special_available = &disabled,
                                             .print = fresh_print},
              &services.hooks, error) &&
          ab_classic_collision_achievements_open(
              (AbClassicCollisionAchievementConfig){.level = playfield.level,
                                                    .settings = test->statistics,
                                                    .events = test->events,
                                                    .eagle_bait_launched = &disabled,
                                                    .special_game_mode = &absent,
                                                    .context = &services,
                                                    .start_position =
                                                        fresh_start_position,
                                                    .notifications = 16,
                                                    .storage_bytes = 65536},
              &services.achievements, error));
    AbObjectRemovalNotifyHost notifications = {.level = fresh_removal_level};
    AbClassicWorldRuntimeConfig config = {
        .playfield = owner,
        .particles = services.particles,
        .audio = test->audio,
        .settings = test->statistics,
        .events = test->events,
        .globals = globals,
        .random = &test->random,
        .physics_enabled = &physics,
        .blocks = {.sound_force_threshold = &threshold,
                   .sprite_workspace = &workspace,
                   .sound = fresh_sound,
                   .particles = {services.particles, NULL,
                                 ab_classic_particles_ambient},
                   .storage_bytes = 256 * 1024},
        .birds = {.flying_object = flying,
                  .specialty_available = &globals->specialty_available,
                  .delta_based_slingshot = &disabled,
                  .achievement_processor = &enabled,
                  .floating_score_scaling = &scaling,
                  .sound_force_threshold = &threshold,
                  .sprite_workspace = &workspace,
                  .context = &services,
                  .achievement = fresh_achievement,
                  .print = fresh_print,
                  .binding = fresh_binding,
                  .specialty = fresh_specialty,
                  .contact_particles = fresh_contact_particles,
                  .damage_particles = {services.particles, NULL,
                                       ab_classic_particles_ambient},
                  .storage_bytes = 2 * 1024 * 1024},
        .dynamics = {.simulation = {.minimum_x = -100000,
                                    .maximum_x = 100000,
                                    .velocity_iterations = 10,
                                    .position_iterations = 10,
                                    .maximum_steps = 4,
                                    .limits = {.storage_bytes = 256 * 1024,
                                               .glue_pairs = 32}},
                     .physical = {.storage_bytes = 256 * 1024},
                     .goals = {.disable_check = &disabled,
                               .notification_sent = &goal_sent,
                               .notifications = 8,
                               .storage_bytes = 65536},
                     .statistics = {.eagle_bait_launched = &disabled,
                                    .device_model = &model,
                                    .gamer_service = &enabled,
                                    .total_blocks = &total_blocks,
                                    .notifications = 64,
                                    .event_fields = 8,
                                    .storage_bytes = 65536},
                     .removals = {.level_name = &name,
                                  .notifications = &notifications,
                                  .block_increment = 500,
                                  .goal_increment = 5000,
                                  .floating_scaling = 1,
                                  .definition_fields = 128,
                                  .storage_bytes = 1024 * 1024},
                     .contacts = {.end = ab_classic_contacts_play_mode_end},
                     .effect = AB_CLASSIC_DYNAMICS_BASE_EFFECT,
                     .storage_bytes = 2 * 1024 * 1024},
        .map_nodes = 128,
        .map_bytes = 32768,
        .score_entries = 2048,
        .script_bytes = 65536,
        .floating = {128, 64, 65536},
        .storage_bytes = 8 * 1024 * 1024};
    AbClassicWorldRuntime *runtime = NULL;
    CHECK(ab_classic_world_runtime_open(config, &runtime, error));
    AbClassicWorldRuntimeView view = ab_classic_world_runtime_view(runtime);
    AbClassicDynamicsView dynamics = ab_classic_dynamics_view(view.dynamics);
    AbClassicLevelView level = ab_classic_level_view(playfield.level);
    CHECK(view.scores && view.floating && view.scripts && dynamics.contacts);
    FreshGameResult game = {0};
    CHECK(fresh_game_runtime(test, owner, runtime, services.particles, globals, flying,
                             &physics, &game, error));
    float total_score = 0;
    CHECK(ab_classic_scores_update(view.scores, &total_score, error));
    size_t removed = 0;
    for (size_t index = 0; index < level.bodies->count; ++index)
        removed += level.removed_objects[index] ? 1 : 0;
    fprintf(stderr,
            "Fresh game frames=%zu steps=%u commands=%zu score=%g ended=%u won=%u\n",
            game.frames, game.steps, game.commands, (double)total_score,
            (unsigned)game.ended, (unsigned)game.won);
    CHECK(game.steps > 120 && !level.world->failed && removed &&
          view.destruction_scores->count && view.block_scores->count &&
          total_score > 0 && game.launched && game.reloaded);
    CHECK(ab_classic_world_runtime_close(&runtime, error) && !runtime);
    ab_classic_collision_achievements_close(services.achievements);
    ab_classic_particles_close(services.particles);
    return true;
}

#endif
