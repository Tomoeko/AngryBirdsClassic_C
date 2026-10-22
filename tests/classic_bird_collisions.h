#ifndef AB_TEST_CLASSIC_BIRD_COLLISIONS_H
#define AB_TEST_CLASSIC_BIRD_COLLISIONS_H

#include "ab/classic_bird_collisions.h"
#include "ab/classic_collision_achievements.h"
#include "ab/classic_particles.h"
#include "ab/classic_collision_hooks.h"

typedef struct {
    ShotTest *shot;
    AbClassicBirdCollisions *birds;
    AbClassicCollisionAchievements *achievements;
    AbClassicParticles *particles;
    AbClassicVisuals *visuals;
    AbClassicMuseumCollisions museum;
    AbClassicCollisionHooks collision_hooks;
    AbDataValue red_special;
    size_t first, second, prints, bindings, specialty_calls, start_queries;
    size_t events, begins, ends;
    const AbDataTable *event;
    size_t packet_calls;
    size_t particle_calls, particle_lookups;
    bool damaging;
} BirdCollisionTest;

static bool bird_test_particle_sprite(void *context, AbSpan name, uintptr_t *output,
                                      AbError *error) {
    (void)error;
    BirdCollisionTest *test = context;
    ++test->particle_lookups;
    AbClassicVisualView view = ab_classic_visuals_view(test->visuals);
    AbSpriteAtlasBinding binding;
    *output =
        (view.provider ? view.provider->sprite(view.provider->context, name, &binding)
                       : ab_sprite_atlas_find(view.atlas, name, &binding))
            ? (uintptr_t)binding.image
            : 0;
    return true;
}

static bool bird_test_particles(void *context, size_t object, AbDataValue descriptor,
                                AbError *error) {
    BirdCollisionTest *test = context;
    CHECK(object == test->first);
    ++test->particle_calls;
    return ab_classic_particles_contact(test->particles, object, descriptor, error);
}

static bool bird_test_packet(void *context, const AbUiPointerValue *arguments,
                             size_t count, AbError *error) {
    (void)error;
    BirdCollisionTest *test = context;
    CHECK(
        count == 8 && arguments[0].kind == AB_UI_POINTER_OBJECT &&
        arguments[0].object ==
            (uintptr_t)&test->shot->level.properties->objects[test->first].physical &&
        arguments[1].kind == AB_UI_POINTER_OBJECT &&
        arguments[1].object ==
            (uintptr_t)&test->shot->level.properties->objects[test->second].physical &&
        arguments[2].number_bits == bits(1) && arguments[3].number_bits == bits(2) &&
        arguments[4].number_bits == bits(3) && arguments[5].number_bits == bits(4) &&
        arguments[6].kind == AB_UI_POINTER_NONE &&
        arguments[7].kind == AB_UI_POINTER_NONE);
    ++test->packet_calls;
    return true;
}

static bool bird_test_receiver(BirdCollisionTest *test, AbClassicBehaviors *behaviors,
                               AbError *error) {
    size_t receiver = test->shot->level.bird_order[0];
    if (receiver == test->first)
        receiver = test->shot->level.bird_order[1];
    const AbStringMap *groups = ab_classic_behaviors_groups(behaviors, receiver);
    size_t cursor = 0;
    AbStringMapEntry entry;
    CHECK(ab_string_map_next(groups, &cursor, &entry, error) && entry.present &&
          entry.value);
    AbGameBehaviorGroup *group = (AbGameBehaviorGroup *)entry.value;
    CHECK(group->capacity && group->items && group->items[0]);
    AbGameBehaviorMethod method = {span("onBirdCollision"), true, test,
                                   bird_test_packet};
    AbGameBehavior observer = {&method, 1};
    AbGameBehavior *saved = group->items[0];
    group->items[0] = &observer;
    AbBlockCollisionNotice notice = {
        .force = 1, .damage = 2, .normal = {3, 4}, .normal_present = true};
    bool okay = ab_classic_behaviors_bird_collision(behaviors, receiver, test->first,
                                                    test->second, &notice, error);
    group->items[0] = saved;
    CHECK(okay && test->packet_calls == 1);
    return true;
}

static bool bird_test_start(void *context, bool global_class, AbVec2 *output,
                            AbError *error) {
    (void)error;
    BirdCollisionTest *test = context;
    CHECK(!global_class);
    ++test->start_queries;
    *output = (AbVec2){0};
    return true;
}

static bool bird_test_achievement(void *context, size_t first, size_t second,
                                  AbError *error) {
    BirdCollisionTest *test = context;
    return ab_classic_collision_achievements_call(test->achievements, first, second,
                                                  error);
}

static bool bird_test_event(void *context, AbDataTable *event, AbError *error) {
    (void)error;
    BirdCollisionTest *test = context;
    CHECK(damage_name_equal(field(event, "id"),
                            (AbDataValue){.kind = AB_DATA_STRING,
                                          .string = span("EID_ACHIEVEMENT_BULLSEYE")}));
    const AbDataTable *first =
        &test->shot->level.properties->objects[test->first].physical;
    CHECK(field(first, "hasCollided").kind == AB_DATA_BOOLEAN &&
          field(first, "hasCollided").bits && test->event != event);
    test->event = event;
    ++test->events;
    return true;
}

static bool bird_test_log(void *context, AbUiPointerValue value, AbError *error) {
    (void)error;
    BirdCollisionTest *test = context;
    if (!test->prints)
        CHECK(value.kind == AB_UI_POINTER_TEXT && value.text.size == 21 &&
              !memcmp(value.text.data, "flyingRedBirdCollided", 21));
    else
        CHECK(value.kind == AB_UI_POINTER_FUNCTION &&
              value.function == (uintptr_t)&test->collision_hooks.red_builtin);
    ++test->prints;
    return true;
}

static bool bird_test_print(void *context, AbUiPointerValue value, AbError *error) {
    BirdCollisionTest *test = context;
    return ab_classic_collision_hooks_print(&test->collision_hooks, value, error);
}

static bool bird_test_binding(void *context, AbDataValue key, AbUiPointerValue *output,
                              AbError *error) {
    BirdCollisionTest *test = context;
    CHECK(
        damage_name_equal(key, (AbDataValue){.kind = AB_DATA_STRING,
                                             .string = span("flyingRedBirdCollided")}));
    CHECK(
        ab_classic_collision_hooks_binding(&test->collision_hooks, key, output, error));
    ++test->bindings;
    return true;
}

static bool bird_test_specialty(void *context, AbUiPointerValue function,
                                const size_t *flying,
                                const AbBirdCollisionNamedInput *contact,
                                AbError *error) {
    BirdCollisionTest *test = context;
    CHECK(function.kind == AB_UI_POINTER_FUNCTION &&
          function.function == (uintptr_t)&test->collision_hooks.red_builtin &&
          flying && *flying == test->first && contact->force == 0 &&
          contact->damage == 2.75f);
    const AbDataTable *first =
        &test->shot->level.properties->objects[test->first].physical;
    CHECK(field(first, "hasCollided").bits &&
          damage_name_equal(field(first, "sprite"),
                            (AbDataValue){.kind = AB_DATA_STRING,
                                          .string = span("BIRD_RED_COLLISION")}));
    ++test->specialty_calls;
    return ab_classic_collision_hooks_call(&test->collision_hooks, function, flying,
                                           contact, error);
}

static bool bird_test_callback(void *context, const AbBirdCollisionNotifyInput *input,
                               AbError *error) {
    BirdCollisionTest *test = context;
    return ab_classic_bird_collisions_call(test->birds, input, error);
}

static bool bird_test_contact(void *context, AbGameContactCall *call, AbError *error) {
    BirdCollisionTest *test = context;
    CHECK(test->shot->level.world->locked);
    ++test->begins;
    return ab_classic_museum_collisions_contact(&test->museum, call, error);
}

static bool bird_test_end(void *context, AbSpan first, AbSpan second, AbError *error) {
    BirdCollisionTest *test = context;
    ++test->ends;
    return ab_classic_contacts_play_mode_end(context, first, second, error);
}

static bool bird_test_physics(BirdCollisionTest *test, AbError *error) {
    ShotTest *shot = test->shot;
    size_t begins = test->begins, ends = test->ends;
    for (size_t index = 0; index < shot->level.bodies->count; ++index) {
        AbVec2 position = index == test->first ? (AbVec2){0, 0}
                          : index == test->second
                              ? (AbVec2){0, 1}
                              : (AbVec2){20000 + (float)index * 100, 0};
        CHECK(ab_classic_level_set_position(shot->owner, index, position, error) &&
              ab_world_set_body_type(
                  shot->level.world, shot->level.bodies->objects[index].physics.body,
                  index == test->first ? AB_BODY_DYNAMIC : AB_BODY_STATIC, error));
    }
    CHECK(ab_level_property_set(shot->level.properties, test->first, span("xVel"),
                                (AbDataValue){.kind = AB_DATA_NUMBER,
                                              .bits = bits(test->damaging ? 1000 : 0)},
                                error) &&
          ab_level_property_set(shot->level.properties, test->first, span("yVel"),
                                (AbDataValue){.kind = AB_DATA_NUMBER}, error));
    size_t body = shot->level.bodies->objects[test->first].physics.body;
    CHECK(ab_body_velocity_set(&shot->level.world->storage.bodies[body].motion,
                               (AbVec2){0}));
    AbClassicContactObject objects[32] = {0};
    for (size_t index = 0; index < shot->level.bodies->count; ++index)
        objects[index].script_table_present = true;
    AbGameContactRoutes routes;
    AbClassicGluePair pairs[2];
    AbClassicGlueQueue glue = {pairs, 0, 2};
    CHECK(ab_game_contact_routes_initialize(&routes));
    AbClassicContacts contacts;
    CHECK(ab_classic_contacts_open(
        shot->owner, objects, shot->level.bodies->count,
        (AbClassicContactsConfig){.routes = &routes,
                                  .glue = &glue,
                                  .context = test,
                                  .collision = bird_test_contact,
                                  .end = bird_test_end},
        &contacts, error));
    bool stepped = ab_world_step(shot->level.world, 1.0f / 60, 8, 3, error);
    bool separated = stepped &&
                     ab_classic_level_set_position(shot->owner, test->first,
                                                   (AbVec2){0, -100}, error) &&
                     ab_world_step(shot->level.world, 0, 8, 3, error);
    bool closed = ab_classic_contacts_close(&contacts, error);
    if (!stepped || !separated || !closed || test->begins != begins + 1 ||
        test->ends != ends + 1)
        fprintf(
            stderr,
            "Bird contact: stepped=%d separated=%d closed=%d begin=%zu end=%zu %s\n",
            stepped, separated, closed, test->begins, test->ends, error->message);
    CHECK(stepped && separated && closed && test->begins == begins + 1 &&
          test->ends == ends + 1 && !glue.count);
    return true;
}

/* This controlled composition follows the earlier native shot checks. Use
 * newly constructed behavior instances so there is no unbound trajectory
 * renderer lifetime. The direct damage callback has force zero. Separate world
 * contacts use zero and then deliberately lethal cached momentum while keeping
 * initial engine velocity zero; the latter reaches real audio and particles.
 * This is C integration evidence, not a native complete-frame comparison. */
#include "classic_removals.h"

static bool shot_bird_collisions(ShotTest *shot, const AbZip *archive,
                                 AbClassicVisuals *visuals,
                                 AbClassicBehaviors *behaviors, AbError *error) {
    CHECK(shot->level.bodies->count <= 32 && shot->level.goal_count == 1);
    BirdCollisionTest test = {.shot = shot,
                              .visuals = visuals,
                              .first = shot->bird,
                              .second = shot->level.goal_order[0]};
    CHECK(ab_classic_collision_hooks_initialize(
        (AbClassicCollisionHookConfig){.red_special_available = &test.red_special,
                                       .print_context = &test,
                                       .print = bird_test_log},
        &test.collision_hooks, error));
    shot->globals.specialty_available =
        (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1};
    AbStringMap *hats = NULL, *dead = NULL;
    AbFloatingScores *floating = NULL;
    AbSettingsStatistics *settings = NULL;
    AbEventManager *events = NULL;
    AbClassicBlockCollisions *blocks = NULL;
    AbClassicScores *root_scores = NULL;
    AbEngineRandom engine_random = {0};
    AbSpan particle_names[] = {span("redBuff"), span("smokeBuffBirdsDefault"),
                               span("smokeBuff")};
    AbBirdCollisionScoreEntry bird_scores[32] = {0};
    AbBirdCollisionGrenades grenades = {.present = true};
    AbDataValue delta = {.kind = AB_DATA_BOOLEAN},
                processor = {.kind = AB_DATA_BOOLEAN, .bits = 1};
    AbDataValue no_eagle = {.kind = AB_DATA_BOOLEAN}, no_mode = {0};
    float threshold = 0, scaling = 1, bird_bonus = 0, entries[8];
    AbBlockCollisionScores scores = {.total = 80, .entries = entries, .capacity = 8};
    size_t indices[16];
    AbStringOrderNode nodes[16];
    AbStringOrderWorkspace workspace = {indices, 16, nodes, 16};
    AbBlockEventObject attachments[32] = {0};
    for (size_t index = 0; index < shot->level.bodies->count; ++index) {
        const AbLevelObject *object = shot->level.bodies->objects + index;
        attachments[index].name = object->name;
        attachments[index].definition = object->definition_name;
        if (index + 1 == shot->level.bodies->count)
            continue;
        AbObjectDefinition definition;
        CHECK(ab_object_definition_open(shot->level.catalog, object->definition_name,
                                        NULL, &definition, error));
        for (unsigned kind = 0; kind < AB_BLOCK_COMPONENT_COUNT; ++kind) {
            const AbDataTable *component = NULL;
            CHECK(ab_block_component(definition.source, (AbBlockComponentKind)kind,
                                     &component, error) &&
                  !component);
        }
        attachments[index].scripts_present = true;
    }
    CHECK(ab_string_map_open(0, 32, 8192, &hats, error) &&
          ab_string_map_open(0, 32, 8192, &dead, error) &&
          ab_floating_scores_open((AbFloatingScoreLimits){8, 32, 4096}, &floating,
                                  error) &&
          ab_settings_statistics_open((AbSettingsStatisticsConfig){span("fixture"),
                                                                   span("fixture"), 8,
                                                                   16, 65536},
                                      &settings, error) &&
          ab_event_manager_open((AbEventManagerLimits){8, 8, 8, 0, 16, 32768},
                                (AbEventManagerHost){0}, &events, error));
    AbEventListener observer = {span("CollisionObserver"), &test, bird_test_event};
    CHECK(
        ab_event_manager_add(events,
                             (AbDataValue){.kind = AB_DATA_STRING,
                                           .string = span("EID_ACHIEVEMENT_BULLSEYE")},
                             &observer, error) &&
        ab_classic_collision_achievements_open(
            (AbClassicCollisionAchievementConfig){.level = shot->owner,
                                                  .settings = settings,
                                                  .events = events,
                                                  .eagle_bait_launched = &no_eagle,
                                                  .special_game_mode = &no_mode,
                                                  .context = &test,
                                                  .start_position = bird_test_start,
                                                  .notifications = 8,
                                                  .storage_bytes = 4096},
            &test.achievements, error));
    CHECK(ab_classic_particles_open(
        archive,
        (AbClassicParticleConfig){.level = shot->owner,
                                  .definitions = particle_names,
                                  .definition_count = 3,
                                  .resource_bytes = 16 * 1024 * 1024,
                                  .storage_bytes = 65536,
                                  .capacity = 32,
                                  .level_capacity = 32,
                                  .engine_random = &engine_random,
                                  .script_random = &shot->audio_random,
                                  .admission = {40, 125, 0.5f},
                                  .workspace = &workspace,
                                  .context = &test,
                                  .sprite = bird_test_particle_sprite},
        &test.particles, error));
    CHECK(ab_classic_scores_open(
        (AbClassicScoreConfig){.level = shot->owner,
                               .blocks = &scores,
                               .birds = &bird_bonus,
                               .objects = bird_scores,
                               .count = shot->level.bodies->count,
                               .storage_bytes = 65536},
        &root_scores, error));
    AbClassicBirdCollisionConfig config = {
        .level = shot->owner,
        .root_scores = root_scores,
        .visuals = visuals,
        .behaviors = behaviors,
        .audio = shot->audio,
        .floating_scores = floating,
        .script_objects = attachments,
        .script_object_count = shot->level.bodies->count,
        .hats = hats,
        .grenades = &grenades,
        .bird_scores = bird_scores,
        .bird_score_count = shot->level.bodies->count,
        .scores = &scores,
        .flying_object = &shot->flying,
        .specialty_available = &shot->globals.specialty_available,
        .delta_based_slingshot = &delta,
        .achievement_processor = &processor,
        .floating_score_scaling = &scaling,
        .sound_force_threshold = &threshold,
        .sprite_workspace = &workspace,
        .context = &test,
        .achievement = bird_test_achievement,
        .print = bird_test_print,
        .binding = bird_test_binding,
        .specialty = bird_test_specialty,
        .contact_particles = bird_test_particles,
        .damage_particles = {.context = test.particles,
                             .emit = ab_classic_particles_ambient},
        .storage_bytes = 2 * 1024 * 1024};
    CHECK(ab_classic_bird_collisions_open(config, &test.birds, error) &&
          ab_classic_bird_collisions_bytes(test.birds) <= config.storage_bytes &&
          bird_test_receiver(&test, behaviors, error));
    AbDataTable *first = &shot->level.properties->objects[test.first].physical;
    const AbDataTable *second = &shot->level.properties->objects[test.second].physical;
    shot->flying = (AbDataValue){.kind = AB_DATA_TABLE, .table = first};
    CHECK(ab_level_property_set(shot->level.properties, test.first, span("collision"),
                                (AbDataValue){0}, error) &&
          ab_level_property_set(shot->level.properties, test.first, span("hasCollided"),
                                (AbDataValue){0}, error) &&
          ab_level_property_set(shot->level.properties, test.second, span("strength"),
                                (AbDataValue){.kind = AB_DATA_NUMBER, .bits = bits(2)},
                                error) &&
          ab_classic_level_set_position(shot->owner, test.second, (AbVec2){76, 0},
                                        error));
    AbBirdCollisionNotifyInput input = {
        .contact = {.first = attachments[test.first].name,
                    .second = attachments[test.second].name,
                    .force = 0,
                    .damage = 2.75f,
                    .normal = {2, 3}},
        .point_present = false};
    size_t pending = ab_audio_mixer_view(shot->mixer).pending_count;
    CHECK(ab_classic_bird_collisions_call(test.birds, &input, error) &&
          test.events == 1 && test.start_queries == 1 && test.prints == 2 &&
          test.bindings == 2 && test.specialty_calls == 1 &&
          field(first, "hasCollided").bits && field(first, "collision").bits &&
          shot->globals.specialty_available.kind == AB_DATA_BOOLEAN &&
          !shot->globals.specialty_available.bits &&
          field(ab_settings_statistics_root(settings), "bullsEye").bits &&
          scores.total == 80 && scores.count == 1 && scores.entries[0] == 20 &&
          bird_scores[test.first].present && bird_scores[test.first].score == 20 &&
          ab_floating_scores_count(floating) == 1 &&
          ab_audio_mixer_view(shot->mixer).pending_count == pending &&
          damage_name_equal(field(second, "sprite"), field(second, "damageSprite")));
    AbBirdCollisionFloatingScore *score = ab_floating_scores_at(floating, 0);
    CHECK(score && score->score == 20 && score->text.size == 2 &&
          !memcmp(score->text.data, "20", 2));
    AbDataValue physical_values[32];
    CHECK(ab_classic_block_collisions_open(
              (AbClassicBlockCollisionConfig){.level = shot->owner,
                                              .visuals = visuals,
                                              .behaviors = behaviors,
                                              .audio = shot->audio,
                                              .script_objects = attachments,
                                              .script_object_count =
                                                  shot->level.bodies->count,
                                              .scores = &scores,
                                              .dead_blocks = dead,
                                              .sound_force_threshold = &threshold,
                                              .sprite_workspace = &workspace,
                                              .storage_bytes = 65536},
              &blocks, error) &&
          ab_classic_museum_collisions_open(
              (AbClassicMuseumCollisionConfig){.level = shot->owner,
                                               .blocks = blocks,
                                               .dead_blocks = dead,
                                               .physical_values = physical_values,
                                               .physical_capacity = 32,
                                               .context = &test,
                                               .bird = bird_test_callback},
              &test.museum, error) &&
          bird_test_physics(&test, error) && scores.count == 1 &&
          ab_floating_scores_count(floating) == 1 &&
          ab_floating_scores_update(floating, 0.125f, error) && score->xs > 0);
    test.damaging = true;
    pending = ab_audio_mixer_view(shot->mixer).pending_count;
    CHECK(bird_test_physics(&test, error) && scores.count == 2 &&
          scores.entries[1] == 20 && scores.total == 80 &&
          bird_scores[test.first].score == 40 &&
          ab_floating_scores_count(floating) == 2 &&
          field(second, "strength").bits == bits(0) &&
          shot->level.character_objects[test.second].goal.strength == 0 &&
          test.particle_calls == 1 && test.particle_lookups >= 4 &&
          ab_particles_count(ab_classic_particles_engine(test.particles), false) ==
              test.particle_lookups &&
          ab_audio_mixer_view(shot->mixer).pending_count > pending);
    uintptr_t dead_value = 0;
    bool dead_present = false;
    CHECK(ab_string_map_get(dead, attachments[test.second].name, &dead_value,
                            &dead_present, error) &&
          dead_present && dead_value &&
          ((const AbDataValue *)dead_value)->table == second &&
          ab_particles_update(ab_classic_particles_engine(test.particles), 0.125f,
                              false, error));
    /* Projectile prefix skips the whole continuation before unused NaN force.
     * The shared score/text/state owners must remain unchanged. */
    CHECK(ab_level_property_set(
        shot->level.properties, test.first, span("projectileFeather"),
        (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1}, error));
    input.contact.force = NAN;
    CHECK(ab_classic_bird_collisions_call(test.birds, &input, error) &&
          scores.count == 2 && ab_floating_scores_count(floating) == 2 &&
          test.events == 1 &&
          ab_level_property_set(shot->level.properties, test.first,
                                span("projectileFeather"), (AbDataValue){0}, error));
    /* A separate stopped callback retains score/floating effects when its real
     * achievement processor is present but the service is deliberately absent.
     * It must not continue into the later collided/sprite phases. */
    ab_classic_bird_collisions_close(test.birds);
    test.birds = NULL;
    config.achievement = NULL;
    CHECK(ab_classic_bird_collisions_open(config, &test.birds, error) &&
          ab_level_property_set(shot->level.properties, test.first, span("hasCollided"),
                                (AbDataValue){0}, error));
    input.contact.force = 0;
    AbError expected = {0};
    bool rejected = !ab_classic_bird_collisions_call(test.birds, &input, &expected);
    CHECK(rejected && strstr(expected.message, "achievement service") &&
          scores.count == 3 && scores.entries[2] == 20 && scores.total == 80 &&
          bird_scores[test.first].score == 60 &&
          ab_floating_scores_count(floating) == 3 &&
          field(first, "hasCollided").kind == AB_DATA_NIL && test.events == 1);
    CHECK(bird_test_removal(&test, behaviors, floating, settings, events, dead, &scores,
                            archive, hats, root_scores, error));
    float complete_score = 0;
    CHECK(ab_classic_scores_update(root_scores, &complete_score, error) &&
          complete_score == 15640 && bird_bonus == 10000 &&
          bird_scores[test.first].score == 60 && scores.total == 5580 &&
          scores.count == 3);
    CHECK(ab_classic_scores_update(root_scores, &complete_score, error) &&
          complete_score == 15640 && scores.total == 5580 && scores.count == 3);
    ab_string_map_close(dead);
    ab_classic_block_collisions_close(blocks);
    ab_classic_bird_collisions_close(test.birds);
    ab_classic_scores_close(root_scores);
    ab_classic_particles_close(test.particles);
    ab_event_manager_close(events);
    ab_classic_collision_achievements_close(test.achievements);
    ab_settings_statistics_close(settings);
    ab_floating_scores_close(floating);
    ab_string_map_close(hats);
    puts(
        "Owned bird callback composes canonical flags/art, real score/floating storage "
        "and collision-achievement delivery. Zero-momentum and lethal world contacts "
        "complete Museum dispatch/full callback, with real decoded audio and owned "
        "feather/smoke emission; complete-frame native parity remains open");
    return true;
}

#endif
