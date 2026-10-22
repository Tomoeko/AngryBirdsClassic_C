#ifndef AB_TEST_LEVEL_IMPACT_REMOVAL_H
#define AB_TEST_LEVEL_IMPACT_REMOVAL_H

/* Bind the validated removal phases into the ordinary numeric replay. Body
 * membership, damage and score are compared to the retained native trace.
 * Script notifications, presentation/audio and the unused random result are
 * controlled boundaries here; this fixture does not implement those effects. */
static AbDataValue impact_removal_table(AbDataTable *table) {
    return (AbDataValue){.kind = AB_DATA_TABLE, .table = table};
}

static size_t impact_removal_index(const LevelImpact *impact, AbDataValue object) {
    for (size_t index = 0; object.kind == AB_DATA_TABLE && index < 23; ++index) {
        if (object.table == impact->removal.bodies + index) {
            return index;
        }
    }
    return SIZE_MAX;
}

static size_t impact_removal_name(const LevelImpact *impact, AbSpan name) {
    for (size_t index = 0; index < 23; ++index) {
        const char *expected = impact->records[index].name;
        if (name.size == strlen(expected) && !memcmp(name.data, expected, name.size)) {
            return index;
        }
    }
    return SIZE_MAX;
}

static bool impact_removal_definition(void *context, AbSpan name, AbDataValue *output,
                                      AbError *error) {
    LevelImpact *impact = context;
    (void)error;
    size_t index = impact_removal_name(impact, name);
    CHECK(index < 23 && impact->objects[index].definition.source);
    *output =
        (AbDataValue){.kind = AB_DATA_TABLE,
                      .table = (AbDataTable *)impact->objects[index].definition.source};
    return true;
}

static bool impact_removal_property_definition(void *context, AbDataValue name,
                                               AbDataValue *output, AbError *error) {
    CHECK(name.kind == AB_DATA_STRING);
    return impact_removal_definition(context, name.string, output, error);
}

static bool impact_removal_property(void *context, AbDataValue object, AbSpan key,
                                    AbDataValue *output, AbError *error) {
    LevelImpact *impact = context;
    size_t index = impact_removal_index(impact, object);
    CHECK(index < 23);
    AbObjectPropertyAccess access = {
        .name = impact->removal.body_fields[index][IMPACT_NAME].value,
        .physics_object = object};
    return ab_object_property_get(&access, key, &impact->removal.property_host, output,
                                  error);
}

static bool impact_removal_notice(void *context, AbDataValue object, AbError *error) {
    LevelImpact *impact = context;
    (void)error;
    size_t index = impact_removal_index(impact, object);
    CHECK(index < 23 && ab_world_has_body(impact->removal.registry.world,
                                          impact->records[index].instance.body));
    return true;
}

static bool impact_removal_score(void *context, AbDataValue object,
                                 AbScoreNumber *output, AbError *error) {
    LevelImpact *impact = context;
    (void)error;
    size_t index = impact_removal_index(impact, object);
    CHECK(index < 23);
    const AbObjectDefinition *definition = &impact->objects[index].definition;
    AbDataValue override = ab_object_property(definition, span("destroyedScoreInc"));
    AbDataValue goal = impact->removal.body_fields[index][IMPACT_GOAL].value;
    const AbDataValue *base = data_field(definition->source, "destroyedScoreInc");
    CHECK(!base || base->kind == AB_DATA_NUMBER);
    AbBreakScoreInput input = {
        .block_increment = 500,
        .goal_increment = 5000,
        .base_override = {.value = base ? from_bits(base->bits) : 0,
                          .present = base != NULL},
        .merged_override_present = override.kind != AB_DATA_NIL,
        .level_goal_truthy = goal.kind != AB_DATA_NIL &&
                             (goal.kind != AB_DATA_BOOLEAN || goal.bits != 0)};
    return ab_break_score(&input, output);
}

static bool impact_removal_floating(void *context,
                                    const AbBlockRemovalFloatingScore *request,
                                    AbError *error) {
    LevelImpact *impact = context;
    (void)error;
    CHECK(request->score.present && isfinite(request->score.value) &&
          request->time == 0 && request->xs == 0);
    ++impact->removal.floating_requests;
    return true;
}

static bool impact_removal_audio_name(void *context, AbDataValue sound,
                                      AbDataValue *output, AbError *error) {
    (void)context;
    (void)error;
    *output = sound;
    return true;
}

static bool impact_removal_audio(void *context, AbAudioPlay request, AbError *error) {
    (void)context;
    (void)error;
    CHECK(request.volume.kind == AB_DATA_NUMBER &&
          request.loop.kind == AB_DATA_BOOLEAN && !request.loop.bits);
    return true;
}

static bool impact_removal_sound(void *context, AbDataValue object,
                                 AbDataValue definition, AbDataValue *output,
                                 AbError *error) {
    CHECK(definition.kind == AB_DATA_TABLE &&
          impact_removal_notice(context, object, error));
    return ab_level_effect_base_sound(AB_LEVEL_EFFECT_DESTROYED_SOUND, definition,
                                      output, error);
}

static bool impact_removal_random(void *context, float lower, float upper,
                                  AbDataValue *output, AbError *error) {
    LevelImpact *impact = context;
    (void)error;
    CHECK(lower == 12 && upper == 20);
    ++impact->removal.random_requests;
    *output = number(17);
    return true;
}

static bool impact_removal_functions(void *context, AbDataValue object,
                                     AbUiPointerValue *output, AbError *error) {
    CHECK(impact_removal_notice(context, object, error));
    *output = (AbUiPointerValue){0};
    return true;
}

static bool impact_removal_new_particles(void *context, AbSpan name,
                                         AbDataValue descriptor, bool first,
                                         bool second, AbError *error) {
    (void)descriptor;
    (void)error;
    CHECK(!first && !second && impact_removal_name(context, name) < 23);
    return true;
}

static bool impact_removal_old_particles(void *context, AbSpan name,
                                         AbDataValue particles, AbDataValue amount,
                                         bool first, bool second, AbError *error) {
    (void)particles;
    (void)amount;
    return impact_removal_new_particles(context, name, (AbDataValue){0}, first, second,
                                        error);
}

static void impact_removal_unbind(void *context, AbGameObjectCore *object) {
    LevelImpact *impact = context;
    size_t index = (size_t)(object - impact->removal.physical);
    AbClassicLevelView level = ab_classic_level_view(impact->resources->owner);
    if (index >= 23) {
        impact->failed = true;
        return;
    }
    for (size_t source = 0; source < level.bodies->count; ++source) {
        if (level.bodies->objects[source].physics.body == object->body) {
            impact->failed |=
                !ab_classic_level_forget_object(impact->resources->owner, source, NULL);
            impact->removal.removed[index] = true;
            return;
        }
    }
    impact->failed = true;
}

static bool impact_removal_physical(LevelImpact *impact, AbSpan name, AbError *error) {
    size_t index = impact_removal_name(impact, name);
    CHECK(index < 23);
    bool removed = false;
    CHECK(ab_game_object_remove(&impact->removal.native, name, &removed, error) &&
          removed && !impact->failed);
    impact->objects[index].dead = false;
    return true;
}

static bool impact_removal_native(void *context, AbSpan name, AbError *error) {
    LevelImpact *impact = context;
    CHECK(impact_removal_physical(impact, name, error));
    ++impact->removals;
    return true;
}

static bool impact_removal_pig(void *context, AbSpan name, AbError *error) {
    (void)error;
    CHECK(impact_removal_name(context, name) < 23);
    return true;
}

static void impact_removal_sync(LevelImpact *impact, const AbPhysicsWorld *world) {
    for (size_t index = 0; index < 23; ++index) {
        AbDataEntry *fields = impact->removal.body_fields[index];
        fields[IMPACT_STRENGTH].value =
            impact->objects[index].damage.has_strength
                ? number(impact->objects[index].damage.strength)
                : (AbDataValue){0};
        size_t body = impact->records[index].instance.body;
        if (ab_world_has_body(world, body)) {
            AbVec2 position =
                world->storage.bodies[body].motion.mass.transform.position;
            fields[IMPACT_X].value = number(position.x);
            fields[IMPACT_Y].value = number(position.y);
        }
    }
}

#include "level_bird_removal.h"

static bool impact_removal_open(LevelImpact *impact, AbPhysicsWorld *world) {
    LevelImpactResources *resources = impact->resources;
    AbClassicLevelView level = ab_classic_level_view(resources->owner);
    CHECK(level.world == world && level.bodies->count == 23);
    for (size_t index = 0; index < IMPACT_MAP_COUNT; ++index) {
        size_t maximum = index == IMPACT_TELEPORTER_MAP ? 0 : 32;
        CHECK(ab_string_map_open(index == IMPACT_PHYSICAL_MAP ? 32 : 0, maximum, 16384,
                                 resources->maps + index, NULL));
    }
    LevelImpactRemoval *removal = &impact->removal;
    removal->registry = (AbGameObjectCoreRegistry){
        .world = world,
        .objects = resources->maps[IMPACT_PHYSICAL_MAP],
        .rolling_levels = resources->maps[IMPACT_ROLLING_MAP],
        .birds = removal->birds,
        .bird_capacity = 23};
    removal->object_host = (AbGameObjectCoreHost){
        .context = impact, .destroy_object = impact_removal_unbind};
    removal->native =
        (AbGameObjectRemoval){.objects = &removal->registry,
                              .joints = level.joints,
                              .teleporters = resources->maps[IMPACT_TELEPORTER_MAP],
                              .object_host = &removal->object_host};
    const char *const names[] = {
        "name",      "strength", "definition",       "levelGoal",
        "x",         "y",        "specialty",        "killSilently",
        "isGiftBox", "rocket",   "gameObjectScript", "inBubble"};
    for (size_t index = 0; index < 23; ++index) {
        const AbObjectDefinition *definition = &impact->objects[index].definition;
        AbSpan name = span(impact->records[index].name);
        AbDataEntry *fields = removal->body_fields[index];
        for (size_t field = 0; field < IMPACT_FIELD_COUNT; ++field) {
            fields[field].key = string(names[field]);
        }
        fields[IMPACT_NAME].value = string(impact->records[index].name);
        fields[IMPACT_DEFINITION].value = string(impact->records[index].definition);
        const size_t metadata[] = {IMPACT_GOAL, IMPACT_SPECIALTY, IMPACT_SILENT,
                                   IMPACT_GIFT, IMPACT_ROCKET};
        for (size_t field = 0; field < sizeof(metadata) / sizeof(*metadata); ++field) {
            size_t key = metadata[field];
            fields[key].value = ab_object_property(definition, span(names[key]));
        }
        removal->bodies[index] =
            (AbDataTable){fields, IMPACT_FIELD_COUNT, IMPACT_FIELD_COUNT, NULL};
        removal->values[index] = impact_removal_table(removal->bodies + index);
        removal->world_fields[index] = removal->dynamic_fields[index] =
            (AbDataEntry){string(impact->records[index].name), removal->values[index]};
        removal->physical[index] =
            (AbGameObjectCore){.name = name,
                               .body = impact->records[index].instance.body,
                               .collider = (AbGameColliderType)(unsigned)from_bits(
                                   impact->records[index].instance.collider.bits)};
        const AbDataValue *rolling = data_field(definition->material, "rollingSound");
        if (rolling && rolling->kind == AB_DATA_STRING) {
            removal->physical[index].rolling_sound = rolling->string;
        }
        CHECK(ab_string_map_set(removal->registry.objects, name,
                                (uintptr_t)(removal->physical + index), true, NULL));
        AbDataValue goal = fields[IMPACT_GOAL].value;
        if (goal.kind != AB_DATA_NIL && (goal.kind != AB_DATA_BOOLEAN || goal.bits)) {
            CHECK(ab_string_map_set(resources->maps[IMPACT_GOAL_MAP], name, 1, true,
                                    NULL));
        }
        if (removal->physical[index].collider == AB_COLLIDER_BIRD ||
            removal->physical[index].collider == AB_COLLIDER_EAGLE) {
            removal->birds[removal->registry.bird_count++] = removal->physical + index;
            /* Controlled script receivers are separate from physical objects. */
            fields[IMPACT_SCRIPT].value =
                impact_removal_table(removal->scripts + index);
        }
    }
    removal->world = (AbDataTable){removal->world_fields, 23, 23, NULL};
    removal->dynamic = (AbDataTable){removal->dynamic_fields, 23, 23, NULL};
    removal->container_fields[0] =
        (AbDataEntry){string("world"), impact_removal_table(&removal->world)};
    removal->container_fields[1] =
        (AbDataEntry){string("dynamic"), impact_removal_table(&removal->dynamic)};
    removal->container = (AbDataTable){removal->container_fields, 2, 2, NULL};
    removal->state = (AbBlockRemovalState){impact_removal_table(&removal->container),
                                           resources->maps[IMPACT_DEAD_MAP]};
    removal->scores = (AbBlockRemovalScores){
        .total = {.present = true}, .entries = removal->individual, .capacity = 23};
    const AbDataValue *materials = data_field(impact->catalog, "materials");
    CHECK(materials && materials->kind == AB_DATA_TABLE);
    removal->property_host = (AbObjectPropertyAccessHost){
        impact, impact_removal_property_definition, *materials};
    removal->scoring =
        (AbBlockRemovalScoreHost){.context = impact,
                                  .destroy = impact_removal_notice,
                                  .definition = impact_removal_definition,
                                  .score = impact_removal_score,
                                  .audio_name = impact_removal_audio_name,
                                  .audio = impact_removal_audio,
                                  .floating = impact_removal_floating,
                                  .floating_scaling = {.value = 1, .present = true},
                                  .scores = &removal->scores};
    removal->effects =
        (AbBlockRemovalEffectsHost){.context = impact,
                                    .state = &removal->state,
                                    .property = impact_removal_property,
                                    .destroyed_sound = impact_removal_sound,
                                    .random = impact_removal_random,
                                    .audio_name = impact_removal_audio_name,
                                    .audio = impact_removal_audio,
                                    .update_destroyed = impact_removal_notice};
    removal->particles =
        (AbBlockRemovalParticlesHost){.context = impact,
                                      .notify = impact_removal_notice,
                                      .functions = impact_removal_functions,
                                      .property = impact_removal_property,
                                      .new_particles = impact_removal_new_particles,
                                      .old_particles = impact_removal_old_particles};
    removal->detach = (AbBlockRemovalDetachHost){
        impact,
        (AbDataValue){.kind = AB_DATA_TABLE, .table = (AbDataTable *)impact->catalog},
        impact_removal_native};
    removal->cleanup_state = (AbBlockRemovalCleanupState){
        &removal->state, resources->maps[IMPACT_GOAL_MAP], string("Level1")};
    removal->cleanup = (AbBlockRemovalCleanupHost){.context = impact,
                                                   .remove_pig = impact_removal_pig};
    removal->pass = (AbBlockRemovalPassHost){.scoring = &removal->scoring,
                                             .effects = &removal->effects,
                                             .particles = &removal->particles,
                                             .detach = &removal->detach,
                                             .cleanup_state = &removal->cleanup_state,
                                             .cleanup = &removal->cleanup};
    impact_removal_sync(impact, world);
    return impact_bird_open(impact);
}

static void impact_removal_resources_close(LevelImpactResources *resources) {
    for (size_t index = 0; index < IMPACT_MAP_COUNT; ++index) {
        ab_string_map_close(resources->maps[index]);
        resources->maps[index] = NULL;
    }
}

static bool impact_removal_membership(const LevelImpact *impact) {
    const LevelImpactRemoval *removal = &impact->removal;
    AbStringMapLayout dead = {0};
    CHECK(ab_string_map_layout(impact->resources->maps[IMPACT_DEAD_MAP], &dead) &&
          dead.count == 0);
    CHECK(removal->scores.count == impact->removals &&
          removal->floating_requests == impact->removals &&
          removal->random_requests == impact->removals);
    for (size_t index = 0; index < 23; ++index) {
        const char *name = impact->records[index].name;
        uintptr_t physical = 0;
        bool present = false;
        CHECK(ab_string_map_get(removal->registry.objects, span(name), &physical,
                                &present, NULL));
        CHECK(present == !removal->removed[index]);
        if (present) {
            CHECK(physical == (uintptr_t)(removal->physical + index));
        }
        const AbDataValue *world = data_field(&removal->world, name);
        const AbDataValue *dynamic = data_field(&removal->dynamic, name);
        CHECK(world && dynamic);
        if (removal->removed[index]) {
            CHECK(world->kind == AB_DATA_NIL && dynamic->kind == AB_DATA_NIL);
            CHECK(ab_string_map_get(impact->resources->maps[IMPACT_GOAL_MAP],
                                    span(name), &physical, &present, NULL) &&
                  !present);
        } else {
            CHECK(world->kind == AB_DATA_TABLE && dynamic->kind == AB_DATA_TABLE &&
                  world->table == removal->bodies + index &&
                  dynamic->table == removal->bodies + index);
        }
    }
    return true;
}

#endif
