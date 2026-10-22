#ifndef AB_TEST_CLASSIC_BLOCK_COLLISIONS_H
#define AB_TEST_CLASSIC_BLOCK_COLLISIONS_H

#include "ab/classic_block_collisions.h"
#include "ab/block_component.h"
#include "ab/classic_contacts.h"
#include "ab/classic_museum_collisions.h"
#include "ab/classic_characters.h"

static bool block_animation_lose(void *context, bool *output, AbError *error) {
    (void)error;
    *output = *(bool *)context;
    return true;
}

static bool block_damage_animation(AbClassicCharacters *characters,
                                   AbClassicLevelView level, AbClassicVisuals *visuals,
                                   size_t pig, bool *lost, AbError *error) {
    AbClassicCharacterView view = ab_classic_characters_view(characters);
    AbCharacterAnimationObject saved[32];
    CHECK(level.bodies->count <= 32);
    memcpy(saved, level.character_objects,
           level.bodies->count * sizeof(*level.character_objects));
    for (size_t index = 0; index < level.bird_count; ++index)
        level.character_objects[level.bird_order[index]].shot = true;
    AbGoalAnimation *goal = &level.character_objects[pig].goal;
    goal->special_expression = false;
    goal->oink_timer = 1000;
    goal->blink_on = false;
    const AbDataTable *physical = &level.properties->objects[pig].physical;
    AbDataValue damaged = field(physical, "damageSprite");
    CHECK(!damage_name_equal(damaged, level.initial_sprites[pig].sprite));
    for (size_t frame = 0; frame < 12; ++frame) {
        goal->blink_timer = -1;
        CHECK(ab_classic_characters_update(characters, 1.0f / 60, error));
        AbDataValue expected =
            field(physical, goal->blink_on ? "blinkSprite" : "damageSprite");
        CHECK(damage_name_equal(field(physical, "sprite"), expected) &&
              damage_name_equal(field(physical, "damageSprite"), damaged));
    }
    *lost = true;
    CHECK(ab_classic_characters_update(characters, 1.0f / 60, error) &&
          damage_name_equal(field(physical, "sprite"), field(physical, "smileSprite")));
    *lost = false;
    CHECK(ab_classic_visuals_set_sprite(visuals, pig, damaged.string, error));
    memcpy(level.character_objects, saved,
           level.bodies->count * sizeof(*level.character_objects));
    CHECK(view.count == level.bodies->count);
    return true;
}

typedef struct {
    ShotTest *test;
    AbClassicBlockCollisions *owner;
    AbClassicMuseumCollisions *museum;
    size_t pig, other, level_calls, event_calls, damage_sounds, contact_sounds;
    size_t particles, method_calls, contact_calls, end_calls, bird_calls;
    size_t event_order[16];
    float target_strength;
    bool change_strength, suppress_score, actual_contact, bird_pair;
    AbBlockCollisionInput input;
} BlockCollisionTest;

static bool block_test_level(void *context, size_t first, size_t second,
                             const AbBlockCollisionNotice *notice, AbDataValue *result,
                             AbError *error) {
    BlockCollisionTest *test = context;
    CHECK(first != second && !notice->normal_present && !notice->point_present);
    ++test->level_calls;
    CHECK(!ab_classic_block_collisions_call(test->owner, &test->input, NULL));
    if (test->change_strength)
        CHECK(ab_level_property_set(
            test->test->level.properties, test->pig, span("strength"),
            (AbDataValue){.kind = AB_DATA_NUMBER, .bits = bits(test->target_strength)},
            error));
    *result = test->suppress_score
                  ? (AbDataValue){.kind = AB_DATA_NUMBER, .bits = bits(0)}
                  : (AbDataValue){0};
    return true;
}

static bool block_test_event(AbBlockEventScript *script, AbBlockEventObject *object,
                             const AbDataTable *definition,
                             const AbDataValue *arguments, size_t count,
                             AbError *error) {
    (void)error;
    BlockCollisionTest *test = script->context;
    CHECK(test->event_calls < 16 && count == 5 && definition);
    AbSpan pig_name = test->test->level.bodies->objects[test->pig].name;
    size_t index = object->name.size == pig_name.size &&
                           !memcmp(object->name.data, pig_name.data, pig_name.size)
                       ? test->pig
                       : test->other;
    size_t other = index == test->pig ? test->other : test->pig;
    CHECK(arguments[0].kind == AB_DATA_TABLE &&
          arguments[0].table ==
              &test->test->level.properties->objects[other].physical &&
          arguments[1].bits == bits(test->input.force) &&
          arguments[2].bits == bits(test->input.damage));
    /* Each invocation visits its first side before the negated second side,
     * including the swapped recursive sensor invocation. */
    float direction = test->event_calls % 2 ? -1 : 1;
    if (test->actual_contact)
        direction = test->input.first.size == object->name.size &&
                            !memcmp(test->input.first.data, object->name.data,
                                    object->name.size)
                        ? 1
                        : -1;
    CHECK(arguments[3].kind == AB_DATA_NUMBER &&
          arguments[3].bits == bits(direction * test->input.normal.x) &&
          arguments[4].bits == bits(direction * test->input.normal.y));
    test->event_order[test->event_calls++] = index;
    return true;
}

static bool block_test_sound(void *context, AbBlockCollisionSound kind, size_t object,
                             const AbObjectDefinition *definition, AbDataValue *output,
                             AbError *error) {
    (void)error;
    BlockCollisionTest *test = context;
    CHECK(definition && definition->source);
    if (kind == AB_BLOCK_COLLISION_DAMAGE_SOUND) {
        ++test->damage_sounds;
        CHECK(object == test->pig);
        const AbDataTable *physical =
            &test->test->level.properties->objects[object].physical;
        CHECK(damage_name_equal(field(physical, "sprite"),
                                field(physical, "damageSprite")));
    } else {
        ++test->contact_sounds;
    }
    /* This fixture explicitly selects the base effect's field policy, using
     * the constructor-normalized definition rather than losing material audio. */
    *output = ab_object_property(
        definition, span(kind == AB_BLOCK_COLLISION_DAMAGE_SOUND ? "damageSound"
                                                                 : "collisionSound"));
    return true;
}

static bool block_test_particles(void *context,
                                 const AbAmbientParticleEmission *emission,
                                 AbError *error) {
    (void)error;
    BlockCollisionTest *test = context;
    const AbDataTable *physical =
        &test->test->level.properties->objects[test->pig].physical;
    CHECK(emission->particles.data && emission->particles.size &&
          emission->amount > 0 &&
          emission->position.x == from_bits(field(physical, "x").bits) * 20 &&
          emission->position.y == from_bits(field(physical, "y").bits) * 20);
    ++test->particles;
    return true;
}

static bool block_test_method(void *context, const AbUiPointerValue *arguments,
                              size_t count, AbError *error) {
    (void)error;
    BlockCollisionTest *test = context;
    CHECK(count == 7 && arguments[0].kind == AB_UI_POINTER_TEXT &&
          damage_name_equal(
              (AbDataValue){.kind = AB_DATA_STRING, .string = arguments[0].text},
              (AbDataValue){.kind = AB_DATA_STRING, .string = test->input.second}) &&
          arguments[1].number_bits == bits(test->input.force) &&
          arguments[2].number_bits == bits(test->input.damage) &&
          arguments[3].kind == AB_UI_POINTER_NONE &&
          arguments[4].kind == AB_UI_POINTER_NONE &&
          arguments[5].number_bits == bits(test->input.point.x) &&
          arguments[6].number_bits == bits(test->input.point.y));
    ++test->method_calls;
    return true;
}

static bool block_test_contact(void *context, AbGameContactCall *call, AbError *error) {
    BlockCollisionTest *test = context;
    CHECK(test->test->level.world->locked);
    test->input.first = call->first;
    test->input.second = call->second;
    test->input.normal = call->normal;
    test->input.point = call->point;
    ++test->contact_calls;
    bool okay = test->museum
                    ? ab_classic_museum_collisions_contact(test->museum, call, error)
                    : ab_classic_block_collisions_contact(test->owner, call, error);
    if (!okay)
        fprintf(stderr, "Owned damage contact %.*s / %.*s: %s\n", (int)call->first.size,
                call->first.data, (int)call->second.size, call->second.data,
                error ? error->message : "");
    return okay;
}

static bool block_test_end_contact(void *context, AbSpan first, AbSpan second,
                                   AbError *error) {
    BlockCollisionTest *test = context;
    CHECK(test->test->level.world->locked);
    ++test->end_calls;
    return ab_classic_contacts_play_mode_end(context, first, second, error);
}

#include "classic_museum_collisions.h"

static bool block_physics_contact(BlockCollisionTest *test,
                                  AbClassicBlockCollisionConfig config,
                                  AbError *error) {
    ShotTest *shot = test->test;
    size_t ground = shot->level.bodies->count - 1;
    test->other = ground;
    test->actual_contact = true;
    test->change_strength = false;
    test->event_calls = 0;
    test->damage_sounds = 0;
    test->input.damage = 4;
    const AbDataTable *physical = &shot->level.properties->objects[test->pig].physical;
    float mass = from_bits(field(physical, "mass").bits);
    float momentum = mass * 1000;
    test->input.force = sqrtf(momentum * momentum) * 0.1f;
    CHECK(test->input.force > 4);
    /* Preserve zero engine velocity while deliberately changing only the native
     * cached script velocity. The contact must consume the latter. */
    CHECK(ab_level_property_set(
        shot->level.properties, test->pig, span("xVel"),
        (AbDataValue){.kind = AB_DATA_NUMBER, .bits = bits(1000)}, error));
    for (size_t index = 0; index < shot->level.bodies->count; ++index) {
        AbVec2 position = index == test->pig ? (AbVec2){0, 4.75f}
                          : index == ground  ? (AbVec2){0, 10}
                                             : (AbVec2){10000 + (float)index * 100, 0};
        CHECK(ab_classic_level_set_position(shot->owner, index, position, error) &&
              ab_world_set_body_type(
                  shot->level.world, shot->level.bodies->objects[index].physics.body,
                  index == test->pig ? AB_BODY_DYNAMIC : AB_BODY_STATIC, error));
    }
    AbStringMap *dead = NULL;
    CHECK(ab_string_map_open(0, 32, 8192, &dead, error));
    config.dead_blocks = dead;
    config.level_hook = NULL;
    config.sound = block_test_sound;
    CHECK(ab_classic_block_collisions_open(config, &test->owner, error));
    AbDataValue physical_values[32];
    AbClassicMuseumCollisionConfig museum_config = {.level = shot->owner,
                                                    .blocks = test->owner,
                                                    .dead_blocks = dead,
                                                    .physical_values = physical_values,
                                                    .physical_capacity = 32,
                                                    .context = test};
    CHECK(museum_numeric_boundaries(test, museum_config, error));
    AbClassicMuseumCollisions museum;
    CHECK(ab_classic_museum_collisions_open(museum_config, &museum, error));
    test->museum = &museum;
    AbClassicContactObject objects[32] = {0};
    for (size_t index = 0; index < shot->level.bodies->count; ++index)
        objects[index].script_table_present = true;
    AbGameContactRoutes routes;
    CHECK(ab_game_contact_routes_initialize(&routes));
    AbClassicGluePair pairs[2];
    AbClassicGlueQueue glue = {pairs, 0, 2};
    AbClassicContacts contacts = {0};
    bool opened = ab_classic_contacts_open(
        shot->owner, objects, shot->level.bodies->count,
        (AbClassicContactsConfig){.routes = &routes,
                                  .glue = &glue,
                                  .context = test,
                                  .collision = block_test_contact,
                                  .end = block_test_end_contact},
        &contacts, error);
    bool stepped = opened && ab_world_step(shot->level.world, 1.0f / 60, 8, 3, error);
    bool separated = stepped &&
                     ab_classic_level_set_position(shot->owner, test->pig,
                                                   (AbVec2){0, -100}, error) &&
                     ab_world_step(shot->level.world, 0, 8, 3, error);
    bool closed = !opened || ab_classic_contacts_close(&contacts, error);
    uintptr_t value = 0;
    bool present = false;
    bool found = ab_string_map_get(dead, shot->level.bodies->objects[test->pig].name,
                                   &value, &present, error);
    bool state = stepped && separated && closed && test->contact_calls == 1 &&
                 test->end_calls == 1 && test->event_calls == 1 &&
                 test->damage_sounds == 1 &&
                 field(physical, "strength").bits == bits(0) &&
                 shot->level.character_objects[test->pig].goal.strength == 0 &&
                 config.scores->total == 80 && config.scores->count == 3 &&
                 config.scores->entries[2] == 40 && !glue.count && found && present &&
                 value && ((const AbDataValue *)value)->kind == AB_DATA_TABLE &&
                 ((const AbDataValue *)value)->table == physical;
    ab_string_map_close(dead);
    test->museum = NULL;
    ab_classic_block_collisions_close(test->owner);
    test->owner = NULL;
    if (!state)
        fprintf(stderr,
                "Physics block contact: step=%d close=%d calls=%zu "
                "events=%zu damage=%zu score=%g error=%s\n",
                stepped, closed, test->contact_calls, test->event_calls,
                test->damage_sounds, (double)config.scores->total,
                error ? error->message : "");
    CHECK(state);
    return true;
}

static bool block_method_packet(BlockCollisionTest *test, AbClassicBehaviors *owner) {
    const AbStringMap *groups = ab_classic_behaviors_groups(owner, test->test->bird);
    size_t cursor = 0;
    AbStringMapEntry entry;
    CHECK(ab_string_map_next(groups, &cursor, &entry, NULL) && entry.present &&
          entry.value);
    AbGameBehaviorGroup *group = (AbGameBehaviorGroup *)entry.value;
    CHECK(group->capacity && group->items && group->items[0]);
    AbGameBehaviorMethod method = {span("onBlockCollision"), true, test,
                                   block_test_method};
    AbGameBehavior observer = {&method, 1};
    AbGameBehavior *saved = group->items[0];
    group->items[0] = &observer;
    AbBlockCollisionNotice notice = {.other_name = test->input.second,
                                     .force = test->input.force,
                                     .damage = test->input.damage,
                                     .point = test->input.point,
                                     .point_present = true};
    bool okay =
        ab_classic_behaviors_block_collision(owner, test->test->bird, &notice, NULL);
    group->items[0] = saved;
    CHECK(okay && test->method_calls == 1);
    return true;
}

static bool shot_block_collisions(ShotTest *shot, AbClassicVisuals *visuals,
                                  AbClassicBehaviors *behaviors, AbError *error) {
    CHECK(shot->level.bodies->count <= 32 && shot->level.goal_count == 1);
    BlockCollisionTest test = {.test = shot,
                               .pig = shot->level.goal_order[0],
                               .other = SIZE_MAX,
                               .target_strength = 2,
                               .change_strength = true};
    AbBlockEventObject attachments[32] = {0};
    for (size_t index = 0; index < shot->level.bodies->count; ++index) {
        const AbLevelObject *object = shot->level.bodies->objects + index;
        attachments[index].name = object->name;
        attachments[index].definition = object->definition_name;
        if (index + 1 == shot->level.bodies->count)
            continue; /* The generated ground bypasses ObjectCreation. */
        AbObjectDefinition definition;
        CHECK(ab_object_definition_open(shot->level.catalog, object->definition_name,
                                        NULL, &definition, error));
        /* Empty initial attachments are justified only after all shipped
         * component predicates reject these actual level definitions. */
        for (unsigned kind = 0; kind < AB_BLOCK_COMPONENT_COUNT; ++kind) {
            const AbDataTable *component = NULL;
            CHECK(ab_block_component(definition.source, (AbBlockComponentKind)kind,
                                     &component, error) &&
                  !component);
        }
        attachments[index].scripts_present = true;
        if (index != test.pig && !object->physics.controllable)
            test.other = index;
    }
    CHECK(test.other != SIZE_MAX);
    test.input = (AbBlockCollisionInput){.first = attachments[test.pig].name,
                                         .second = attachments[test.other].name,
                                         .force = 1,
                                         .damage = 2.75f,
                                         .normal = {0.25f, -0.5f},
                                         .point = {2, 3}};
    CHECK(block_method_packet(&test, behaviors));
    AbBlockEventBinding binding = {span("onCollision"), block_test_event};
    AbBlockEventScript recorder = {&binding, 1, &test};
    AbBlockEventScript *scripts[] = {&recorder};
    attachments[test.pig].scripts = attachments[test.other].scripts = scripts;
    attachments[test.pig].count = attachments[test.other].count = 1;
    attachments[test.pig].capacity = attachments[test.other].capacity = 1;
    float score_entries[8], threshold = 0;
    AbBlockCollisionScores scores = {.entries = score_entries, .capacity = 8};
    size_t indices[16];
    AbStringOrderNode nodes[16];
    AbStringOrderWorkspace workspace = {indices, 16, nodes, 16};
    AbClassicBlockCollisionConfig config = {
        .level = shot->owner,
        .visuals = visuals,
        .behaviors = behaviors,
        .audio = shot->audio,
        .script_objects = attachments,
        .script_object_count = shot->level.bodies->count,
        .scores = &scores,
        .sound_force_threshold = &threshold,
        .sprite_workspace = &workspace,
        .context = &test,
        .level_hook = block_test_level,
        .sound = block_test_sound,
        .particles = {.context = &test, .emit = block_test_particles},
        .storage_bytes = 65536};
    CHECK(ab_classic_block_collisions_open(config, &test.owner, error) &&
          ab_classic_block_collisions_bytes(test.owner) <= config.storage_bytes);
    AbRandom48 animation_random;
    AbClassicCharacters *characters = NULL;
    bool lost = false;
    CHECK(ab_random48_seed(&animation_random, 17) &&
          ab_classic_characters_open(
              shot->owner, visuals, &animation_random,
              (AbClassicCharacterHost){.context = &lost,
                                       .lose_condition = block_animation_lose},
              65536, &characters, error));
    const AbDataTable *physical = &shot->level.properties->objects[test.pig].physical;
    AbDataValue original = field(physical, "sprite");
    AbDataValue strength = field(physical, "strength");
    AbDataValue script_physical =
        shot->level.properties->objects[test.pig].script_entries[1].value;
    size_t pending = ab_audio_mixer_view(shot->mixer).pending_count;
    CHECK(
        ab_classic_block_collisions_call(test.owner, &test.input, error) &&
        test.level_calls == 1 && test.event_calls == 2 && test.damage_sounds == 1 &&
        test.contact_sounds == 2 && scores.total == 20 && scores.count == 1 &&
        scores.entries[0] == 20 &&
        !damage_name_equal(field(physical, "sprite"), original) &&
        damage_name_equal(field(physical, "sprite"), field(physical, "damageSprite")));
    CHECK(block_damage_animation(characters, shot->level, visuals, test.pig, &lost,
                                 error));
    ab_classic_characters_close(characters);
    AbAudioMixerView audio = ab_audio_mixer_view(shot->mixer);
    CHECK(audio.pending_count > pending);
    for (size_t index = pending; index < audio.pending_count; ++index)
        CHECK(audio.pending[index].track == 2 && !audio.pending[index].flags);
    test.change_strength = false;
    test.input.force = 0;
    size_t emitted = test.particles;
    shot->level.properties->objects[test.pig].script_entries[1].value =
        (AbDataValue){0};
    bool canonical = ab_classic_block_collisions_call(test.owner, &test.input, error);
    shot->level.properties->objects[test.pig].script_entries[1].value = script_physical;
    CHECK(canonical && scores.total == 40 && scores.count == 2 &&
          test.damage_sounds == 1 && test.particles == emitted);
    test.suppress_score = true;
    CHECK(ab_classic_block_collisions_call(test.owner, &test.input, error) &&
          scores.total == 40 && scores.count == 2);
    test.suppress_score = false;
    test.input.damage = 0;
    test.event_calls = 0;
    size_t calls = test.level_calls;
    CHECK(ab_level_property_set(shot->level.properties, test.other, span("sensor"),
                                (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1},
                                error) &&
          ab_classic_block_collisions_call(test.owner, &test.input, error) &&
          test.level_calls == calls + 2 && test.event_calls == 4 &&
          test.event_order[0] == test.other && test.event_order[1] == test.pig &&
          test.event_order[2] == test.pig && test.event_order[3] == test.other);
    CHECK(ab_level_property_set(shot->level.properties, test.pig, span("sensor"),
                                (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1},
                                error));
    calls = test.level_calls;
    test.input.force = NAN;
    CHECK(ab_classic_block_collisions_call(test.owner, &test.input, error) &&
          test.level_calls == calls);
    ab_classic_block_collisions_close(test.owner);
    test.owner = NULL;
    CHECK(ab_level_property_set(shot->level.properties, test.other, span("sensor"),
                                (AbDataValue){0}, error) &&
          ab_level_property_set(shot->level.properties, test.pig, span("sensor"),
                                (AbDataValue){0}, error));
    /* Missing sound must fail after actual sprite/cache publication and before
     * particles or score. This is a separate deliberately failing session. */
    config.sound = NULL;
    test.change_strength = true;
    test.target_strength = from_bits(strength.bits);
    test.input.force = 0;
    test.input.damage = 2.75f;
    CHECK(ab_classic_block_collisions_open(config, &test.owner, error));
    AbDataValue previous = field(physical, "damageSprite");
    AbError expected = {0};
    bool failed = !ab_classic_block_collisions_call(test.owner, &test.input, &expected);
    ab_classic_block_collisions_close(test.owner);
    test.owner = NULL;
    if (!failed || !strstr(expected.message, "level-effect"))
        fprintf(stderr,
                "Owned block failure control: failed=%d score=%g "
                "count=%zu error=%s\n",
                failed, (double)scores.total, scores.count, expected.message);
    CHECK(
        failed && strstr(expected.message, "level-effect") && scores.total == 40 &&
        scores.count == 2 && test.particles == emitted &&
        !damage_name_equal(field(physical, "damageSprite"), previous) &&
        damage_name_equal(field(physical, "sprite"), field(physical, "damageSprite")));
    CHECK(ab_level_property_set(shot->level.properties, test.pig, span("strength"),
                                strength, error));
    CHECK(block_physics_contact(&test, config, error));
    puts("Owned block collision executes canonical damage visuals, real audio, "
         "script packets, sensor recursion and ordered failure retention. An actual "
         "physics contact publishes health/death and scores without duplication");
    return true;
}

#endif
