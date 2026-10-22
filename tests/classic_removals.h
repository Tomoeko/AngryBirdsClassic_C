#ifndef AB_TEST_CLASSIC_REMOVALS_H
#define AB_TEST_CLASSIC_REMOVALS_H

#include "ab/classic_removals.h"
#include "ab/particle_draw.h"
#include "ab/classic_physics_phases.h"
#include "ab/classic_dynamics.h"
#include "ab/classic_goals.h"
#include "ab/classic_destruction_statistics.h"
#include "ab/classic_post_physics.h"
#include "ab/level_effect.h"

#include "classic_birds.h"

typedef struct {
    BirdCollisionTest *collision;
    AbClassicGoals *goals;
    AbClassicDestructionStatistics *statistics;
    AbSettingsStatistics *settings;
    AbDataValue *goal_sent;
    size_t pig_events, aggregate_events, goal_events;
    const AbDataTable *aggregate, *cleared;
} RemovalTest;

static bool removal_sprite(void *context, AbSpan name, AbSpriteAtlasBinding *output) {
    return ab_sprite_atlas_find(context, name, output);
}

static bool removal_texture(void *context, const AbImage *image, uint32_t *output) {
    AbSpriteAtlas *atlas = context;
    for (size_t index = 0; index < ab_sprite_atlas_count(atlas); ++index) {
        AbSpriteAtlasImage view;
        if (ab_sprite_atlas_image(atlas, index, &view) && view.image == image) {
            *output = (uint32_t)index + 1;
            return true;
        }
    }
    return false;
}

static bool removal_event(void *context, AbDataTable *event, AbError *error) {
    RemovalTest *test = context;
    AbDataValue id = field(event, "id"), count;
    CHECK(ab_settings_statistics_get_local(test->settings, span("pigsDestroyed"),
                                           &count, error));
    if (damage_name_equal(id, (AbDataValue){.kind = AB_DATA_STRING,
                                            .string = span("EID_PIG_POPPED")})) {
        CHECK(count.bits == bits(0) && field(event, "amount").bits == bits(1));
        ++test->pig_events;
    } else if (damage_name_equal(
                   id, (AbDataValue){.kind = AB_DATA_STRING,
                                     .string = span("EID_BLOCKS_DESTROYED")})) {
        CHECK(count.bits == bits(1) && field(event, "pigsDestroyed").bits == bits(1) &&
              field(event, "totalBlocks").bits == bits(0));
        test->aggregate = event;
        ++test->aggregate_events;
    } else {
        CHECK(damage_name_equal(
            id, (AbDataValue){.kind = AB_DATA_STRING,
                              .string = span("EID_LEVEL_GOALS_CLEARED")}));
        ShotTest *shot = test->collision->shot;
        size_t index = test->collision->second;
        CHECK(!test->goal_sent->bits && shot->level.removed_objects[index] &&
              !ab_world_has_body(shot->level.world,
                                 shot->level.bodies->objects[index].physics.body));
        test->cleared = event;
        ++test->goal_events;
    }
    return true;
}

static bool bird_test_removal(BirdCollisionTest *collision,
                              AbClassicBehaviors *behaviors, AbFloatingScores *floating,
                              AbSettingsStatistics *settings, AbEventManager *events,
                              AbStringMap *dead, AbBlockCollisionScores *scores,
                              const AbZip *archive, AbStringMap *hats,
                              AbClassicScores *root_scores, AbError *error) {
    ShotTest *shot = collision->shot;
    CHECK(shot->level.goal_count == 1);
    AbClassicDynamics *dynamics = NULL;
    AbPhysicsScriptStep scripts = {.event = span("onUpdateBlockPhysicsStep")};
    AbClassicContactObject contact_objects[32] = {0};
    AbClassicRenderScene *scene = NULL;
    AbStringMap *teleporters = NULL;
    shot->globals.time = (AbDataValue){.kind = AB_DATA_NUMBER, .bits = bits(3)};
    shot->globals.score = (AbDataValue){.kind = AB_DATA_NUMBER, .bits = bits(80)};
    AbDataValue disabled = {0}, sent = {0}, level_name = {0}, eagle = {0},
                device = {.kind = AB_DATA_STRING, .string = span("android")},
                gamer = {.kind = AB_DATA_BOOLEAN, .bits = 1};
    RemovalTest test = {
        .collision = collision, .settings = settings, .goal_sent = &sent};
    float total = 99;
    float prior_total = scores->total;
    size_t prior_count = scores->count;
    float destruction_entries[8];
    AbBlockRemovalScores destruction = {.entries = destruction_entries, .capacity = 8};
    AbClassicVisualView visuals = ab_classic_visuals_view(collision->visuals);
    const AbSpriteProvider *provider = visuals.provider;
    CHECK(provider);
    uint32_t ids = UINT32_C(0x1000000);
    CHECK(ab_classic_render_scene_open(
              shot->owner,
              (AbClassicRenderSceneConfig){.provider = provider,
                                           .id_counter = &ids,
                                           .extra_renderers = 32,
                                           .storage_bytes = 65536,
                                           .object_blend = AB_BLEND_PREMULTIPLIED},
              &scene, error) &&
          ab_string_map_open(0, 0, 4096, &teleporters, error));
    for (size_t index = 0; index < shot->level.bodies->count; ++index)
        contact_objects[index].script_table_present = true;
    AbObjectRemovalNotifyHost notifications = {0};
    AbClassicDynamicsConfig config = {
        .level = shot->owner,
        .visuals = collision->visuals,
        .scene = scene,
        .behaviors = behaviors,
        .audio = shot->audio,
        .settings = settings,
        .events = events,
        .globals = &shot->globals,
        .random = &shot->audio_random,
        .simulation = {.minimum_x = -100000,
                       .maximum_x = 100000,
                       .velocity_iterations = 8,
                       .position_iterations = 3,
                       .maximum_steps = 4,
                       .limits = {.storage_bytes = 128 * 1024, .glue_pairs = 2}},
        .physical = {.teleporters = teleporters, .storage_bytes = 65536},
        .goals = {.disable_check = &disabled,
                  .notification_sent = &sent,
                  .notifications = 4,
                  .storage_bytes = 65536},
        .statistics = {.eagle_bait_launched = &eagle,
                       .device_model = &device,
                       .gamer_service = &gamer,
                       .total_blocks = &total,
                       .notifications = 4,
                       .event_fields = 8,
                       .storage_bytes = 65536},
        .removals = {.particles = collision->particles,
                     .floating = floating,
                     .scores = scores,
                     .destruction_scores = &destruction,
                     .dead_blocks = dead,
                     .level_name = &level_name,
                     .notifications = &notifications,
                     .block_increment = 500,
                     .goal_increment = 5000,
                     .floating_scaling = 1,
                     .definition_fields = 96,
                     .storage_bytes = 256 * 1024},
        .contacts = {.context = collision,
                     .collision = bird_test_contact,
                     .end = bird_test_end},
        .contact_objects = contact_objects,
        .contact_count = shot->level.bodies->count,
        .scripts = &scripts,
        .effect = AB_CLASSIC_DYNAMICS_BASE_EFFECT,
        .storage_bytes = 1024 * 1024};
    AbWorldListener previous = shot->level.world->listener;
    AbClassicDynamics *invalid = (AbClassicDynamics *)(uintptr_t)1;
    AbClassicDynamicsConfig bad = config;
    bad.storage_bytes = 1;
    CHECK(!ab_classic_dynamics_open(bad, &invalid, NULL) && !invalid &&
          !ab_classic_dynamics_bytes(NULL));
    bad = config;
    bad.removals.storage_bytes = 1;
    CHECK(!ab_classic_dynamics_open(bad, &invalid, NULL) && !invalid &&
          shot->level.world->listener.context == previous.context &&
          ab_classic_dynamics_open(config, &dynamics, error) &&
          ab_classic_dynamics_bytes(dynamics) <= config.storage_bytes);
    AbClassicDynamicsView owned = ab_classic_dynamics_view(dynamics);
    AbClassicSimulation *simulation = owned.simulation;
    AbClassicObjectRemoval *physical = owned.physical;
    AbClassicRemovals *removals = owned.removals;
    test.goals = owned.goals;
    test.statistics = owned.statistics;
    CHECK(simulation && physical && removals && test.goals && test.statistics &&
          owned.contacts && owned.phases && !owned.phases->water.enabled);
    /* A replaced listener must preserve the dynamics handle and its storage. */
    void *bound_context = shot->level.world->listener.context;
    shot->level.world->listener.context = NULL;
    CHECK(!ab_classic_dynamics_close(&dynamics, NULL) && dynamics &&
          ab_classic_dynamics_view(dynamics).simulation == simulation);
    shot->level.world->listener.context = bound_context;
    AbEventListener observer = {span("RemovalObserver"), &test, removal_event};
    const char *const event_names[] = {"EID_PIG_POPPED", "EID_BLOCKS_DESTROYED",
                                       "EID_LEVEL_GOALS_CLEARED"};
    for (size_t index = 0; index < 3; ++index)
        CHECK(ab_event_manager_add(
            events,
            (AbDataValue){.kind = AB_DATA_STRING, .string = span(event_names[index])},
            &observer, error));
    bool cleared = true;
    CHECK(ab_classic_goals_check(test.goals, &cleared, error) && !cleared &&
          !sent.bits);
    size_t before = ab_floating_scores_count(floating);
    size_t particles =
        ab_particles_count(ab_classic_particles_engine(collision->particles), false);
    size_t members = ab_classic_render_scene_view(scene).count;
    shot->level.frame->clock.accumulator = 0;
    AbFrameClockPlan plan;
    CHECK(ab_classic_simulation_advance(
              simulation,
              &(AbFrameClockInput){.seconds = 0x1.111112p-5f, .physics_enabled = true},
              &plan, error) &&
          plan.steps == 1 && scores->total == prior_total + 5000 &&
          scores->count == prior_count && destruction.count == 1 &&
          destruction.entries[0] == 5000 && test.pig_events == 1 &&
          test.aggregate_events == 1 && test.goal_events == 1 && sent.bits &&
          shot->globals.last_score_time.bits == bits(3) &&
          shot->globals.old_score.bits == bits(80) && total == 0 &&
          ab_floating_scores_count(floating) == before + 1 &&
          ab_particles_count(ab_classic_particles_engine(collision->particles),
                             false) == particles + 1 &&
          !ab_classic_visuals_object(collision->visuals, collision->second)->attached &&
          ab_classic_render_scene_view(scene).count == members - 1 &&
          ab_classic_render_scene_sync(scene, error));
    AbBirdCollisionFloatingScore *floating_score =
        ab_floating_scores_at(floating, before);
    CHECK(floating_score && !floating_score->text_present &&
          floating_score->score == 5000 && floating_score->lifetime == 0.9f &&
          damage_name_equal(
              floating_score->sprite,
              (AbDataValue){.kind = AB_DATA_STRING, .string = span("5K_GREEN")}));
    AbSpan name = shot->level.bodies->objects[collision->second].name;
    AbDataValue key = {.kind = AB_DATA_STRING, .string = name};
    const AbDataValue *world = ab_data_get(&shot->level.properties->world, key);
    const AbDataValue *dynamic = ab_data_get(&shot->level.properties->dynamic, key);
    uintptr_t value;
    bool present;
    CHECK((!world || world->kind == AB_DATA_NIL) &&
          (!dynamic || dynamic->kind == AB_DATA_NIL) &&
          ab_string_map_get(dead, name, &value, &present, error) && !present &&
          ab_string_map_get(ab_classic_goals_map(test.goals), name, &value, &present,
                            error) &&
          !present &&
          ab_string_map_get(ab_classic_object_removal_registry(physical)->objects, name,
                            &value, &present, error) &&
          !present && ab_classic_removals_run(removals, error) &&
          scores->count == prior_count && destruction.count == 1 &&
          ab_classic_goals_check(test.goals, &cleared, error) && cleared &&
          test.goal_events == 1 &&
          field(test.aggregate, "pigsDestroyed").bits == bits(1) &&
          field(test.cleared, "id").kind == AB_DATA_STRING);
    /* Exercise the separate live-object removePig prefix. Source accepts any
     * named world object; an ordinary block makes its +500/no-floating/no-stats
     * behavior visible independently of the completed pig destruction above. */
    size_t block = 0;
    while (block < shot->level.bodies->count &&
           (shot->level.frame->objects[block].collider != AB_COLLIDER_BASIC ||
            shot->level.bodies->objects[block].physics.controllable ||
            shot->level.removed_objects[block]))
        ++block;
    CHECK(block < shot->level.bodies->count);
    AbSpan block_name = shot->level.bodies->objects[block].name;
    size_t floating_before = ab_floating_scores_count(floating);
    CHECK(ab_classic_removals_remove_pig(removals, block_name, error) &&
          shot->level.removed_objects[block] && scores->total == prior_total + 5500 &&
          scores->count == prior_count && destruction.count == 2 &&
          destruction.entries[1] == 500 &&
          ab_floating_scores_count(floating) == floating_before &&
          test.pig_events == 1 && test.aggregate_events == 1 && test.goal_events == 1 &&
          ab_classic_removals_remove_pig(removals, block_name, error) &&
          scores->count == prior_count && destruction.count == 2 &&
          ab_classic_simulation_advance(
              simulation,
              &(AbFrameClockInput){.seconds = 0x1.111112p-5f, .physics_enabled = true},
              &plan, error) &&
          plan.steps == 1 && ab_classic_render_scene_sync(scene, error));
    AbDrawCommand particle_commands[64];
    AbDrawList particle_list = {particle_commands, 0, 64};
    CHECK(ab_particles_draw_world(ab_classic_particles_engine(collision->particles),
                                  (AbParticleDrawStyle){.provider = provider,
                                                        .camera = {{-32, -64}, {1, 1}},
                                                        .width = 480,
                                                        .height = 320,
                                                        .blend = AB_BLEND_PREMULTIPLIED,
                                                        .color = {1, 1, 1, 1},
                                                        .opacity = 1},
                                  &particle_list, error) &&
          particle_list.count > 0);
    size_t shared_count = scores->count;
    float shared_total = scores->total;
    CHECK(bird_test_lifecycle(collision, archive, behaviors, physical, scene, hats,
                              root_scores, floating, removals,
                              (LifecycleGoals){test.goals, events, &sent}, provider,
                              error) &&
          scores->count == shared_count && scores->total == shared_total &&
          ab_classic_simulation_advance(
              simulation,
              &(AbFrameClockInput){.seconds = 0x1.111112p-5f, .physics_enabled = true},
              &plan, error) &&
          plan.steps == 1 && ab_classic_render_scene_sync(scene, error));
    for (size_t index = 0; index < 3; ++index)
        CHECK(ab_event_manager_remove(
            events,
            (AbDataValue){.kind = AB_DATA_STRING, .string = span(event_names[index])},
            &observer, error));
    CHECK(ab_classic_dynamics_close(&dynamics, error) && !dynamics &&
          shot->level.world->listener.context == previous.context &&
          shot->level.world->listener.begin == previous.begin &&
          shot->level.world->listener.end == previous.end);
    ab_classic_render_scene_close(scene);
    ab_string_map_close(teleporters);
    return true;
}

#endif
