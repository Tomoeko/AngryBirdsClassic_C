#include "ab/classic_level.h"
#include "ab/hash.h"
#include "ab/object_transform.h"
#include "ab/string_map.h"
#include "ab/target_data.h"

#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

enum {
    OBJECTS,
    FRAME_OBJECTS,
    STARTUP_TIMERS,
    BODIES,
    FIXTURES,
    MASS,
    CONTACTS,
    PAIRS,
    ISLAND_BODIES,
    ISLAND_CONTACTS,
    BODY_ORDER,
    CONTACT_ORDER,
    STACK,
    HEADS,
    EDGES,
    POSITIONS,
    VELOCITIES,
    VELOCITY_CONTACTS,
    POSITION_CONTACTS,
    ORDER_ENTRIES,
    ORDER_INDICES,
    ORDER_NODES,
    BIRD_ENTRIES,
    BIRD_INDICES,
    BIRD_NODES,
    GOAL_ENTRIES,
    GOAL_INDICES,
    GOAL_NODES,
    CHARACTER_OBJECTS,
    INITIAL_SPRITES,
    MOTION_PROPERTIES,
    JOINTS,
    ISLAND_JOINTS,
    JOINT_ORDER,
    JOINT_HEADS,
    JOINT_EDGES,
    JOINT_RECORDS,
    GLUE_RECORDS,
    JOINT_CONSTRUCTOR_INDICES,
    JOINT_CONSTRUCTOR_NODES,
    REMOVED_OBJECTS,
    PROPERTY_OBJECTS,
    PROPERTY_WORLD_ENTRIES,
    PROPERTY_DYNAMIC_ENTRIES,
    ARRAY_COUNT
};

struct AbClassicLevel {
    AbPhysicsWorld world;
    AbClassicJoints joints;
    AbStringOrderWorkspace joint_order;
    AbWorldSolveWorkspace solve;
    AbLevelBodies bodies;
    AbGameFrame frame;
    AbLevelObjectOrder order;
    AbStringMap *world_names;
    size_t world_count, array_bytes;
    AbLevelObjectOrder bird_order;
    size_t bird_count;
    AbLevelObjectOrder goal_order;
    size_t goal_count;
    AbCharacterAnimationObject *character_objects;
    AbDamageSpriteNames *initial_sprites;
    AbClassicMotionProperties *motion_properties;
    bool *removed_objects;
    AbLevelProperties properties;
    AbDataEntry *dynamic_entries;
    bool sprites_initialized;
    AbGameCollisionMatrix collisions;
    const AbLuaData *data;
    const AbDataTable *catalog;
    const AbLuaData *catalog_source;
    AbTargetData owned_data;
    AbTargetData owned_catalog;
    AbSpriteSet *sprites;
    AbClassicObjectTimers *timers;
    bool timers_initialized;
    uint8_t *arrays;
};

typedef struct {
    size_t count;
    size_t element_size;
} ArrayLayout;

static bool array_offsets(const ArrayLayout layout[ARRAY_COUNT], size_t limit,
                          size_t offsets[ARRAY_COUNT], size_t *total) {
    size_t used = 0;
    const size_t alignment = _Alignof(max_align_t);
    for (size_t index = 0; index < ARRAY_COUNT; ++index) {
        size_t padding = (alignment - used % alignment) % alignment;
        if (padding > limit - used) {
            return false;
        }
        used += padding;
        offsets[index] = used;
        if (layout[index].count > (limit - used) / layout[index].element_size) {
            return false;
        }
        used += layout[index].count * layout[index].element_size;
    }
    *total = used;
    return true;
}

static bool hash_capacity(const AbLevelBodySource *source, size_t object_capacity,
                          size_t *output) {
    AbDataValue key = {.kind = AB_DATA_STRING, .string = {(const uint8_t *)"world", 5}};
    const AbDataValue *world = ab_data_get(source->level->globals, key);
    if (!world || world->kind != AB_DATA_TABLE || !world->table) {
        return false;
    }
    const AbDataTableLayout *layout = ab_lua_table_layout(source->level, world->table);
    if (!layout || !layout->no_nil_assignments) {
        return false;
    }
    size_t capacity = 1;
    while (capacity < object_capacity) {
        if (capacity > SIZE_MAX / 2) {
            return false;
        }
        capacity *= 2;
    }
    *output = capacity > layout->hash_capacity ? capacity : layout->hash_capacity;
    return true;
}

static bool allocate_arrays(AbClassicLevel *level, AbClassicLevelLimits limits,
                            size_t hash_nodes,
                            AbLevelJointRequirements joint_requirements,
                            AbWorldStorage *storage, AbError *error) {
    size_t bodies = limits.objects, contacts = limits.contacts;
    const ArrayLayout layout[ARRAY_COUNT] = {
        {bodies, sizeof(AbLevelObject)},
        {bodies, sizeof(AbGameFrameObject)},
        {bodies, sizeof(AbClassicObjectTimers)},
        {bodies, sizeof(AbWorldSolveBody)},
        {bodies, sizeof(AbWorldFixture)},
        {bodies, sizeof(AbFixtureMass)},
        {contacts, sizeof(AbWorldSolveContact)},
        {contacts, sizeof(AbFixturePair)},
        {bodies, sizeof(AbBodyMotion)},
        {contacts, sizeof(AbIslandContact)},
        {bodies, sizeof(size_t)},
        {contacts, sizeof(size_t)},
        {bodies, sizeof(size_t)},
        {bodies, sizeof(size_t)},
        {contacts, 2 * sizeof(size_t)},
        {bodies, sizeof(AbPositionState)},
        {bodies, sizeof(AbVelocity)},
        {contacts, sizeof(AbVelocityContact)},
        {contacts, sizeof(AbPositionConstraint)},
        {bodies, sizeof(AbDataEntry)},
        {bodies, sizeof(size_t)},
        {hash_nodes, sizeof(AbStringOrderNode)},
        {bodies, sizeof(AbDataEntry)},
        {bodies, sizeof(size_t)},
        {hash_nodes, sizeof(AbStringOrderNode)},
        {bodies, sizeof(AbDataEntry)},
        {bodies, sizeof(size_t)},
        {hash_nodes, sizeof(AbStringOrderNode)},
        {bodies, sizeof(AbCharacterAnimationObject)},
        {bodies, sizeof(AbDamageSpriteNames)},
        {bodies, sizeof(AbClassicMotionProperties)},
        {limits.joints, sizeof(AbWorldSolveJoint)},
        {limits.joints, sizeof(AbIslandJoint)},
        {limits.joints, sizeof(size_t)},
        {bodies, sizeof(size_t)},
        {limits.joints, 2 * sizeof(size_t)},
        {limits.joints, sizeof(AbClassicJointRecord)},
        {limits.glue_joints, sizeof(AbClassicJointRecord)},
        {joint_requirements.immediate + joint_requirements.deferred, sizeof(size_t)},
        {joint_requirements.hash_capacity, sizeof(AbStringOrderNode)},
        {bodies, sizeof(bool)},
        {bodies, sizeof(AbLevelPropertyObject)},
        {bodies, sizeof(AbDataEntry)},
        {bodies, sizeof(AbDataEntry)},
    };
    size_t offsets[ARRAY_COUNT], total;
    if (!array_offsets(layout, limits.storage_bytes - sizeof(*level), offsets,
                       &total)) {
        return ab_fail(error, "Classic level storage exceeds its byte limit");
    }
    level->arrays = calloc(1, total);
    if (!level->arrays) {
        return ab_fail(error, "Cannot allocate classic level arrays");
    }
    level->array_bytes = total;
    void *arrays[ARRAY_COUNT];
    for (size_t index = 0; index < ARRAY_COUNT; ++index) {
        arrays[index] = level->arrays + offsets[index];
    }
    level->solve = (AbWorldSolveWorkspace){
        .bodies = arrays[ISLAND_BODIES],
        .contacts = arrays[ISLAND_CONTACTS],
        .body_order = arrays[BODY_ORDER],
        .contact_order = arrays[CONTACT_ORDER],
        .stack = arrays[STACK],
        .body_heads = arrays[HEADS],
        .edge_next = arrays[EDGES],
        .body_capacity = bodies,
        .contact_capacity = contacts,
        .joints = arrays[ISLAND_JOINTS],
        .joint_order = arrays[JOINT_ORDER],
        .joint_heads = arrays[JOINT_HEADS],
        .joint_edge_next = arrays[JOINT_EDGES],
        .joint_capacity = limits.joints,
        .solver = {arrays[POSITIONS], arrays[VELOCITIES], arrays[VELOCITY_CONTACTS],
                   arrays[POSITION_CONTACTS], bodies, contacts},
    };
    *storage = (AbWorldStorage){
        .bodies = arrays[BODIES],
        .fixtures = arrays[FIXTURES],
        .contacts = arrays[CONTACTS],
        .pairs = arrays[PAIRS],
        .contact_capacity = contacts,
        .mass_scratch = arrays[MASS],
        .mass_capacity = bodies,
        .solve = &level->solve,
        .body_capacity = bodies,
        .fixture_capacity = bodies,
        .joints = arrays[JOINTS],
        .joint_capacity = limits.joints,
    };
    level->joints.storage = (AbClassicJointStorage){
        arrays[JOINT_RECORDS], limits.joints, arrays[GLUE_RECORDS], limits.glue_joints};
    level->joint_order = (AbStringOrderWorkspace){
        arrays[JOINT_CONSTRUCTOR_INDICES],
        joint_requirements.immediate + joint_requirements.deferred,
        arrays[JOINT_CONSTRUCTOR_NODES], joint_requirements.hash_capacity};
    level->bodies = (AbLevelBodies){arrays[OBJECTS], bodies, 0};
    level->properties =
        (AbLevelProperties){.objects = arrays[PROPERTY_OBJECTS],
                            .world_entries = arrays[PROPERTY_WORLD_ENTRIES],
                            .capacity = bodies};
    level->dynamic_entries = arrays[PROPERTY_DYNAMIC_ENTRIES];
    level->frame.objects = arrays[FRAME_OBJECTS];
    level->timers = arrays[STARTUP_TIMERS];
    level->order = (AbLevelObjectOrder){
        arrays[ORDER_ENTRIES],
        bodies,
        {arrays[ORDER_INDICES], bodies, arrays[ORDER_NODES], hash_nodes},
        0};
    level->bird_order = (AbLevelObjectOrder){
        arrays[BIRD_ENTRIES],
        bodies,
        {arrays[BIRD_INDICES], bodies, arrays[BIRD_NODES], hash_nodes},
        0};
    level->goal_order = (AbLevelObjectOrder){
        arrays[GOAL_ENTRIES],
        bodies,
        {arrays[GOAL_INDICES], bodies, arrays[GOAL_NODES], hash_nodes},
        0};
    level->character_objects = arrays[CHARACTER_OBJECTS];
    level->initial_sprites = arrays[INITIAL_SPRITES];
    level->motion_properties = arrays[MOTION_PROPERTIES];
    level->removed_objects = arrays[REMOVED_OBJECTS];
    level->frame.removed_objects = level->removed_objects;
    return true;
}

static bool collider_index(AbDataValue value, uint32_t *output) {
    float number;
    memcpy(&number, &value.bits, sizeof(number));
    if (value.kind != AB_DATA_NUMBER || !isfinite(number) || number < 0 ||
        number >= AB_GAME_COLLIDER_TYPES || number != (float)(uint32_t)number) {
        return false;
    }
    *output = (uint32_t)number;
    return true;
}

static bool pair_allowed(void *context, const AbPhysicsWorld *world, size_t first,
                         size_t second) {
    const AbClassicLevel *level = context;
    const AbWorldFixture *a = world->storage.fixtures + first;
    const AbWorldFixture *b = world->storage.fixtures + second;
    const AbLevelObject *object_a = (const void *)a->user_data;
    const AbLevelObject *object_b = (const void *)b->user_data;
    AbGameCollisionObject metadata_a = {0}, metadata_b = {0};
    if ((object_a &&
         !collider_index(object_a->physics.collider, &metadata_a.collider)) ||
        (object_b &&
         !collider_index(object_b->physics.collider, &metadata_b.collider))) {
        return false;
    }
    metadata_a.enabled = object_a && object_a->physics.collisions_enabled;
    metadata_b.enabled = object_b && object_b->physics.collisions_enabled;
    return ab_game_collision_allowed(a->filter, b->filter,
                                     object_a ? &metadata_a : NULL,
                                     object_b ? &metadata_b : NULL, &level->collisions);
}

static bool initialize_character(AbClassicLevel *level, size_t index, AbError *error) {
    const AbLevelObject *object = level->bodies.objects + index;
    AbCharacterAnimationObject *character = level->character_objects + index;
    AbBodyPose pose = level->frame.objects[index].rendered;
    character->jump.position = pose.position;
    character->jump.rotation = pose.angle;
    if (index + 1 == level->bodies.count) {
        /* MuseumMode prototype 2711, instructions 1018..1019. */
        character->goal.strength = 30;
        return true;
    }
    AbObjectDefinition definition;
    if (!ab_object_definition_open(level->catalog, object->definition_name, NULL,
                                   &definition, error)) {
        return false;
    }
    AbDataValue strength =
        ab_object_property(&definition, (AbSpan){(const uint8_t *)"strength", 8});
    if (strength.kind != AB_DATA_NIL) {
        if (strength.kind != AB_DATA_NUMBER) {
            return ab_fail(error, "Unsupported initial character strength");
        }
        memcpy(&character->goal.strength, &strength.bits, sizeof(strength.bits));
        if (!isfinite(character->goal.strength)) {
            return ab_fail(error, "Nonfinite initial character strength");
        }
    }
    AbDataValue roll =
        ab_object_property(&definition, (AbSpan){(const uint8_t *)"allowRoll", 9});
    character->allow_roll = roll.kind == AB_DATA_BOOLEAN && roll.bits != 0;
    return true;
}

static bool initialize_objects(AbClassicLevel *level, bool queue, AbError *error) {
    for (size_t cursor = 0; cursor < level->bodies.count; ++cursor) {
        size_t index = level->order.strings.indices[cursor];
        const AbObjectBody *object = &level->bodies.objects[index].physics;
        uint32_t collider;
        if (!collider_index(object->collider, &collider)) {
            return ab_fail(error, "Unsupported classic level collider");
        }
        if ((queue && object->controllable &&
             !ab_object_body_queue(&level->world, object, error)) ||
            !ab_game_frame_object_reset(&level->world, object->body, collider,
                                        level->frame.objects + index, error) ||
            !initialize_character(level, index, error)) {
            return false;
        }
    }
    level->frame.count = level->bodies.count;
    return true;
}

static AbDataValue property_key(const char *name) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)name, strlen(name)}};
}

static AbDataValue property_number(float value) {
    AbDataValue result = {.kind = AB_DATA_NUMBER};
    memcpy(&result.bits, &value, sizeof(value));
    return result;
}

static bool initial_property(AbClassicLevel *level, size_t index, const char *name,
                             AbDataValue value, AbError *error) {
    if (value.kind == AB_DATA_NIL)
        return true;
    AbObjectPropertyAccess access = {
        .physics_object = {.kind = AB_DATA_TABLE,
                           .table = &level->properties.objects[index].physical}};
    return ab_object_property_set(&access, property_key(name).string, value, error);
}

static bool initialize_physical_properties(AbClassicLevel *level, size_t index,
                                           AbDataValue order, AbError *error) {
    const AbLevelObject *object = level->bodies.objects + index;
    const AbWorldFixture *fixture =
        level->world.storage.fixtures + object->physics.fixture;
    /* GameObject::setupLuaTable, ELF 0x344b74 (BN view base 0x10000).
     * Script mass is captured before queueing can make a bird static. */
    const AbDataEntry fields[] = {
        {property_key("angle"),
         property_number(level->character_objects[index].jump.rotation)},
        {property_key("density"), property_number(fixture->density)},
        {property_key("friction"), property_number(fixture->friction)},
        {property_key("restitution"), property_number(fixture->restitution)},
        {property_key("mass"), property_number(object->initial_mass)},
        {property_key("xVel"), property_number(0)},
        {property_key("yVel"), property_number(0)},
        {property_key("z_order"), order},
    };
    for (size_t field = 0; field < sizeof(fields) / sizeof(*fields); ++field) {
        AbObjectPropertyAccess access = {
            .physics_object = {.kind = AB_DATA_TABLE,
                               .table = &level->properties.objects[index].physical}};
        if (!ab_object_property_set(&access, fields[field].key.string,
                                    fields[field].value, error))
            return false;
    }
    return ab_level_properties_publish_position(
        &level->properties, index, level->character_objects[index].jump.position,
        error);
}

static bool initialize_ground_properties(AbClassicLevel *level, size_t index,
                                         AbError *error) {
    /* MuseumMode.loadLevel, prototype 2711, instructions 1008..1025.
     * The generated ground bypasses ObjectCreation's definition/material copy. */
    return initialize_physical_properties(level, index, property_number(0), error) &&
           initial_property(level, index, "type", property_key("box"), error) &&
           initial_property(level, index, "materialName", property_key("staticGround"),
                            error) &&
           initial_property(level, index, "strength", property_number(30), error) &&
           initial_property(level, index, "defence", property_number(1000000), error);
}

static bool initialize_definition_properties(AbClassicLevel *level, size_t index,
                                             AbError *error) {
    const AbLevelObject *object = level->bodies.objects + index;
    const AbDataValue *order = ab_data_get(object->placement, property_key("z_order"));
    AbDataEntry override = {property_key("z_order"), order ? *order : (AbDataValue){0}};
    AbDataTable overrides = {&override, 1, 1, NULL};
    AbObjectDefinition definition;
    if (!ab_object_definition_open(level->catalog, object->definition_name, &overrides,
                                   &definition, error) ||
        !initialize_physical_properties(level, index, definition.z_order, error))
        return false;
    /* ObjectCreation prototype 883 copies these resolved values into the raw
     * physical table. Keep nil absent and false distinct from nil. Table values
     * below remain immutable catalog-backed descriptors. Mutable animation and
     * damage-range construction are separate ownership work. */
    static const char *const fields[] = {
        "type",
        "materialName",
        "controllable",
        "strength",
        "defence",
        "levelGoal",
        "forceX",
        "forceY",
        "frictionXRelative",
        "frictionYRelative",
        "forceRelative",
        "targetVelocity",
        "ignoreGroundDamage",
        "noCollisionDamage",
        "collisionSound",
        "underwaterCollisionSound",
        "damageSound",
        "destroyedSound",
        "underwaterDestroyedSound",
        "rollingSound",
        "portalDelay",
        "portalMinSpeedOut",
        "rocket",
        "rocketTrailParticles",
        "rocketTrailSpawnDelay",
        "floatability",
        "passThroughSound",
        "passThroughParticles",
        "isGiftBox",
    };
    for (size_t field = 0; field < sizeof(fields) / sizeof(*fields); ++field) {
        AbDataValue value =
            ab_object_property(&definition, property_key(fields[field]).string);
        if (!initial_property(level, index, fields[field], value, error))
            return false;
    }
    AbDataValue feather =
        ab_object_property(&definition, property_key("isMightyFeather").string);
    if (feather.kind == AB_DATA_NIL ||
        (feather.kind == AB_DATA_BOOLEAN && !feather.bits))
        feather = (AbDataValue){.kind = AB_DATA_BOOLEAN};
    if (!initial_property(level, index, "isMightyFeather", feather, error) ||
        !initial_property(level, index, "collider", definition.collider, error) ||
        !initial_property(level, index, "startNumber", object->start_number, error))
        return false;
    if (object->physics.controllable)
        return initial_property(
            level, index, "damageFactors",
            ab_object_property(&definition, property_key("damageFactors").string),
            error);
    return true;
}

static bool initialize_property_values(AbClassicLevel *level, AbError *error) {
    for (size_t index = 0; index < level->bodies.count; ++index) {
        bool okay = index + 1 == level->bodies.count
                        ? initialize_ground_properties(level, index, error)
                        : initialize_definition_properties(level, index, error);
        if (!okay)
            return false;
    }
    return true;
}

static bool initialize_world_names(AbClassicLevel *level, size_t maximum_nodes,
                                   size_t storage_bytes, AbError *error) {
    size_t used = sizeof(*level) + level->array_bytes;
    if (used > storage_bytes ||
        !ab_string_map_open(0, maximum_nodes, storage_bytes - used, &level->world_names,
                            error))
        return false;
    for (size_t index = 0; index < level->bodies.count; ++index)
        if (!ab_string_map_set(level->world_names, level->bodies.objects[index].name,
                               index, true, error))
            return false;
    level->world_count = level->bodies.count;
    return true;
}

bool ab_classic_level_open(const AbLevelBodySource *source,
                           AbWorldDefinition definition, AbClassicLevelLimits limits,
                           bool queue_controllables, AbClassicLevel **output,
                           AbError *error) {
    if (!output) {
        return ab_fail(error, "Missing classic level output");
    }
    *output = NULL;
    if (!source || !source->level || !source->level->globals || !source->catalog ||
        !limits.objects || limits.storage_bytes <= sizeof(AbClassicLevel)) {
        return ab_fail(error, "Invalid classic level source or storage limits");
    }
    size_t nodes;
    AbLevelJointRequirements joint_requirements;
    if (!hash_capacity(source, limits.objects, &nodes) ||
        !ab_level_joint_requirements(source->level, &joint_requirements, error)) {
        return ab_fail(error, "Unsupported classic level constructor layout");
    }
    if (!limits.joints) {
        limits.joints = joint_requirements.immediate + joint_requirements.deferred;
    }
    if (!limits.glue_joints) {
        limits.glue_joints = joint_requirements.deferred;
    }
    if (limits.joints < joint_requirements.immediate ||
        limits.glue_joints < joint_requirements.deferred) {
        return ab_fail(error, "Classic level joint capacities are too small");
    }
    AbClassicLevel *level = calloc(1, sizeof(*level));
    if (!level) {
        return ab_fail(error, "Cannot allocate classic level owner");
    }
    AbWorldStorage storage;
    if (!allocate_arrays(level, limits, nodes, joint_requirements, &storage, error) ||
        !ab_game_collision_initialize(&level->collisions) ||
        !ab_world_open(&level->world, definition, storage, error) ||
        !ab_classic_joints_open(&level->joints, &level->world, level->joints.storage,
                                error)) {
        goto release_level;
    }
    level->world.listener = (AbWorldListener){.context = level, .filter = pair_allowed};
    level->data = source->level;
    level->catalog = source->catalog;
    if (!ab_level_bodies_create(&level->world, source, &level->bodies,
                                &level->order.strings, error) ||
        !ab_level_joints_create(&level->joints, source->level, &level->bodies,
                                &level->joint_order, error) ||
        !ab_level_bodies_order(&level->bodies, &level->order, error) ||
        !ab_level_birds_order(&level->bodies, level->order.strings.indices,
                              &level->bird_order, &level->bird_count, error) ||
        !ab_level_goals_order(&level->bodies, level->order.strings.indices,
                              &level->goal_order, &level->goal_count, error) ||
        !initialize_objects(level, queue_controllables, error) ||
        !ab_level_properties_initialize(&level->properties, &level->bodies,
                                        level->catalog, error) ||
        !ab_level_properties_bind_dynamic(&level->properties, &level->bodies,
                                          level->dynamic_entries,
                                          level->bodies.capacity, error) ||
        !initialize_property_values(level, error) ||
        !initialize_world_names(level, nodes, limits.storage_bytes, error)) {
        goto release_level;
    }
    *output = level;
    return true;

release_level:
    ab_classic_level_close(level);
    return false;
}

static bool random_timer_pair(AbRandom48 *random, int32_t minimum, float *first,
                              float *second, AbError *error) {
    float a, b;
    if (ab_script_random(random, 2, minimum, 30, &a) != AB_SCRIPT_RANDOM_OK ||
        ab_script_random(random, 2, minimum, 30, &b) != AB_SCRIPT_RANDOM_OK) {
        return ab_fail(error, "Cannot generate classic object startup timers");
    }
    *first = a / 10.0f;
    *second = b / 10.0f;
    return true;
}

bool ab_classic_level_initialize_objects(AbClassicLevel *level, AbRandom48 *random,
                                         bool queue_controllables, AbError *error) {
    if (!level || !random || random->value > UINT64_C(0xffffffffffff) ||
        level->timers_initialized || level->world.failed || level->world.locked ||
        level->frame.clock.time != 0 || level->frame.clock.accumulator != 0 ||
        level->frame.history_index != 0) {
        return ab_fail(error, "Invalid classic object initialization state");
    }
    for (size_t cursor = 0; cursor < level->bodies.count; ++cursor) {
        size_t index = level->order.strings.indices[cursor];
        const AbLevelObject *object = level->bodies.objects + index;
        AbClassicObjectTimers *timers = level->timers + index;
        if (!ab_world_has_body(&level->world, object->physics.body)) {
            return ab_fail(error, "Missing initial classic object body");
        }
        if (object->physics.controllable) {
            if (!random_timer_pair(random, 10, &timers->animation, &timers->jump,
                                   error)) {
                return false;
            }
            timers->present |= AB_STARTUP_BIRD_TIMERS;
            AbCharacterAnimationObject *character = level->character_objects + index;
            character->bird.timer = timers->animation;
            character->jump.timer = timers->jump;
            character->animation_enabled = true;
            if (queue_controllables &&
                (!ab_object_body_queue(&level->world, &object->physics, error) ||
                 !ab_game_frame_object_reset(&level->world, object->physics.body,
                                             level->frame.objects[index].collider,
                                             level->frame.objects + index, error))) {
                return false;
            }
            if (queue_controllables) {
                AbBodyPose pose = level->frame.objects[index].rendered;
                character->jump.position = pose.position;
                character->jump.rotation = pose.angle;
            }
        }
        /* The generated ground is always the final constructor object. Its
         * metadata is authored separately and has no levelGoal field. */
        if (index + 1 == level->bodies.count) {
            continue;
        }
        if (object->level_goal) {
            if (!random_timer_pair(random, 5, &timers->blink, &timers->oink, error)) {
                return false;
            }
            timers->present |= AB_STARTUP_GOAL_TIMERS;
            level->character_objects[index].goal.blink_timer = timers->blink;
            level->character_objects[index].goal.oink_timer = timers->oink;
        }
    }
    level->timers_initialized = true;
    return true;
}

static size_t data_bytes(const AbTargetData *data) {
    return data->bytes.size + data->chunk.allocated_bytes + data->data.allocated_bytes;
}

static bool resource_sprite_size(void *context, AbSpan name, AbVec2 *size,
                                 AbError *error) {
    AbSpriteBinding binding;
    if (!ab_sprite_set_find(context, name, &binding)) {
        return ab_fail(error, "Classic object sprite is absent from selected metadata");
    }
    *size = (AbVec2){binding.region->width, binding.region->height};
    return true;
}

static bool initialize_sprite_names(AbClassicLevel *level, const AbLuaData *catalog,
                                    AbError *error) {
    /* Startup-only reuse of solver indices and completed table-construction
     * nodes. Cached world/character permutations remain untouched. */
    AbStringOrderWorkspace workspace = {
        .indices = level->solve.body_order,
        .index_capacity = level->world.storage.body_capacity,
        .nodes = level->order.strings.nodes,
        .node_capacity = level->order.strings.node_capacity,
    };
    for (size_t index = 0; index < level->bodies.count; ++index) {
        AbObjectDefinition definition;
        if (!ab_object_definition_open(level->catalog,
                                       level->bodies.objects[index].definition_name,
                                       NULL, &definition, error) ||
            !ab_object_initial_sprites(&definition, catalog, &workspace,
                                       level->initial_sprites + index, error)) {
            return false;
        }
        const AbDamageSpriteNames *names = level->initial_sprites + index;
        if (!initial_property(level, index, "damageSprite", names->sprite, error) ||
            !initial_property(level, index, "blinkSprite", names->blink, error) ||
            !initial_property(level, index, "smileSprite", names->smile, error))
            return false;
    }
    level->sprites_initialized = true;
    return true;
}

bool ab_classic_level_load(const AbZip *archive, const AbClassicLevelAssets *assets,
                           AbWorldDefinition definition, AbClassicLevelLimits limits,
                           bool queue_controllables, AbClassicLevel **output,
                           AbError *error) {
    if (!output) {
        return ab_fail(error, "Missing loaded classic level output");
    }
    *output = NULL;
    if (!archive || !assets || !assets->level_path || !assets->catalog_path ||
        (assets->sprite_size && (assets->sprite_paths || assets->sprite_sheet_count))) {
        return ab_fail(error, "Missing classic level archive or resource paths");
    }
    AbClassicLevelSourceView shared = {0};
    if (assets->source &&
        (strcmp(assets->catalog_path, "assets/data/scripts/blocks.lua") ||
         !ab_classic_level_source_view(assets->source, archive, &shared, error)))
        return error && error->message[0]
                   ? false
                   : ab_fail(error, "Shared level source requires the blocks catalog");
    uint8_t digest[32], key[32] = {0};
    char hash[65];
    if (!assets->source && !ab_sha256(archive->input, digest, error)) {
        return false;
    }
    if (!assets->source)
        ab_digest_hex(digest, hash);
    if (!assets->source &&
        strcmp(hash,
               "b524b6b4b0ad999f46828546dee1bb047d6e00f724285a2e8c9f4b34a8bee7ad") !=
            0) {
        return ab_fail(error, "Classic level loading requires the pinned APK");
    }
    AbTargetData catalog = {0}, data = {0};
    AbSpriteSet *sprites = NULL;
    AbClassicLevel *level = NULL;
    bool okay = false;
    size_t remaining = assets->resource_bytes;
    const AbLuaData *catalog_data = shared.catalog;
    AbSpan resource_key = shared.key;
    if (!assets->source) {
        if (!ab_target_resource_key(archive, remaining, key, error) ||
            !ab_target_data_open(archive, assets->catalog_path,
                                 (AbSpan){key, sizeof(key)}, remaining, NULL, &catalog,
                                 error))
            goto release_resources;
        remaining -= data_bytes(&catalog);
        catalog_data = &catalog.data;
        resource_key = (AbSpan){key, sizeof(key)};
    }
    if (!ab_target_data_open(archive, assets->level_path, resource_key, remaining,
                             catalog_data->globals, &data, error)) {
        goto release_resources;
    }
    remaining -= data_bytes(&data);
    if (assets->prepare &&
        !assets->prepare(assets->prepare_context, &data.data, catalog_data, error))
        goto release_resources;
    float placement_scale = assets->placement_scale;
    if (assets->derive_placement_scale &&
        !ab_level_placement_scale(data.data.globals, assets->physics_to_world,
                                  &placement_scale, error))
        goto release_resources;
    if (assets->derive_world_gravity &&
        !ab_level_gravity(data.data.globals, &definition.gravity, error)) {
        goto release_resources;
    }
    if (!assets->sprite_size &&
        !ab_sprite_set_open(archive, assets->sprite_paths, assets->sprite_sheet_count,
                            remaining, &sprites, error)) {
        goto release_resources;
    }
    AbLevelBodySource source = {
        .level = &data.data,
        .catalog = catalog_data->globals,
        .sprite_context = assets->sprite_size ? assets->sprite_context : sprites,
        .sprite_size = assets->sprite_size ? assets->sprite_size : resource_sprite_size,
        .placement_scale = placement_scale,
        .physics_scale = assets->physics_scale,
        .ground_position = assets->ground_position,
        .derive_ground_position = assets->derive_ground_position,
        .missing_context = assets->missing_context,
        .missing_definition = assets->missing_definition};
    if (!ab_classic_level_open(&source, definition, limits,
                               queue_controllables && !assets->random, &level, error)) {
        goto release_resources;
    }
    if (!initialize_sprite_names(level, catalog_data, error)) {
        goto release_resources;
    }
    AbRandom48 random = assets->random ? *assets->random : (AbRandom48){0};
    if (assets->random && !ab_classic_level_initialize_objects(
                              level, &random, queue_controllables, error)) {
        goto release_resources;
    }
    level->owned_data = data;
    level->owned_catalog = catalog;
    level->sprites = sprites;
    level->data = &level->owned_data.data;
    level->catalog_source =
        assets->source ? shared.catalog : &level->owned_catalog.data;
    level->catalog = level->catalog_source->globals;
    *output = level;
    if (assets->random) {
        *assets->random = random;
    }
    okay = true;

release_resources:
    memset(key, 0, sizeof(key));
    if (!okay) {
        ab_classic_level_close(level);
        ab_sprite_set_close(sprites);
        ab_target_data_close(&data);
        ab_target_data_close(&catalog);
    }
    return okay;
}

void ab_classic_level_close(AbClassicLevel *level) {
    if (level) {
        ab_string_map_close(level->world_names);
        ab_classic_joints_close(&level->joints);
        ab_world_close(&level->world);
        free(level->arrays);
        ab_sprite_set_close(level->sprites);
        ab_target_data_close(&level->owned_data);
        ab_target_data_close(&level->owned_catalog);
        free(level);
    }
}

static bool synchronize_world_names(AbClassicLevel *level, AbError *error) {
    if (level->properties.count != level->bodies.count ||
        level->properties.world.count != level->bodies.count ||
        level->properties.world.entries != level->properties.world_entries)
        return ab_fail(error, "Rebound canonical world table storage");
    for (size_t index = 0; index < level->bodies.count; ++index) {
        AbSpan name = level->bodies.objects[index].name;
        const AbDataEntry *entry = level->properties.world_entries + index;
        if (entry->key.kind != AB_DATA_STRING || entry->key.string.size != name.size ||
            (name.size && memcmp(entry->key.string.data, name.data, name.size)))
            return ab_fail(error, "Rebound canonical world table key");
        const AbDataValue *value = &entry->value;
        bool present = value->kind != AB_DATA_NIL;
        if (present && (value->kind != AB_DATA_TABLE ||
                        value->table != &level->properties.objects[index].physical))
            return ab_fail(error, "Rebound dynamic circle world identity");
        uintptr_t previous = 0;
        bool previous_present = false;
        if (!ab_string_map_get(level->world_names, name, &previous, &previous_present,
                               error))
            return false;
        /* A later resize may already have discarded a removed nil key. Do not
         * turn synchronization into another source nil assignment/rehash. */
        if (!present && !previous_present)
            continue;
        if (!ab_string_map_set(level->world_names, name, index, present, error))
            return false;
    }
    return true;
}

bool ab_classic_level_world_next(AbClassicLevel *level, AbClassicWorldCursor *cursor,
                                 size_t *object, bool *present, AbError *error) {
    if (!level || !cursor || !object || !present || !level->world_names ||
        level->world.failed)
        return ab_fail(error, "Invalid canonical world iterator");
    if (!synchronize_world_names(level, error))
        return false;
    size_t slot = 0;
    if (cursor->started &&
        !ab_string_map_resume(level->world_names, cursor->name, &slot, error))
        return false;
    AbStringMapEntry entry;
    if (!ab_string_map_next(level->world_names, &slot, &entry, error))
        return false;
    if (entry.present) {
        if (entry.value >= level->bodies.count)
            return ab_fail(error, "Foreign canonical world iterator identity");
        cursor->name = entry.key;
        cursor->started = true;
    }
    *object = entry.present ? (size_t)entry.value : SIZE_MAX;
    *present = entry.present;
    return true;
}

static bool refresh_world_order(AbClassicLevel *level, AbError *error) {
    size_t cursor = 0, used = 0;
    AbStringMapEntry entry;
    do {
        if (!ab_string_map_next(level->world_names, &cursor, &entry, error))
            return false;
        if (entry.present)
            level->order.strings.indices[used++] = entry.value;
    } while (entry.present);
    level->world_count = used;
    for (size_t index = 0; index < level->bodies.count; ++index) {
        bool present = false;
        uintptr_t unused = 0;
        if (!ab_string_map_get(level->world_names, level->bodies.objects[index].name,
                               &unused, &present, error))
            return false;
        if (!present)
            level->order.strings.indices[used++] = index;
    }
    AbStringMapLayout layout;
    if (used != level->bodies.count ||
        !ab_string_map_layout(level->world_names, &layout))
        return ab_fail(error, "Invalid dynamic circle world permutation");
    level->order.hash_capacity = layout.capacity;
    return true;
}

bool ab_classic_level_create_circle(AbClassicLevel *level, AbGameCircleInput input,
                                    size_t *output, AbError *error) {
    if (!level || !output || !level->world_names || level->world.failed ||
        level->world.locked || level->bodies.count >= level->bodies.capacity ||
        level->frame.count != level->bodies.count ||
        level->properties.count != level->bodies.count ||
        level->properties.world.entries != level->properties.world_entries ||
        level->properties.world.count != level->bodies.count || !input.name.data ||
        !input.name.size)
        return ab_fail(error, "Invalid or exhausted dynamic circle level binding");
    for (size_t index = 0; index < level->bodies.count; ++index) {
        AbSpan name = level->bodies.objects[index].name;
        if (name.size == input.name.size &&
            !memcmp(name.data, input.name.data, name.size))
            return ab_fail(error, "Dynamic circle name has already been used");
    }
    if (!synchronize_world_names(level, error))
        return false;
    size_t index = level->bodies.count;
    AbLevelObject *object = level->bodies.objects + index;
    AbGameCircle circle;
    if (!ab_game_circle_create(&level->world, input, (uintptr_t)object, &circle, error))
        return false;
    *object = (AbLevelObject){.name = input.name,
                              .physics = circle.physics,
                              .initial_mass = circle.initial_mass};
    level->frame.objects[index] = circle.frame;
    AbLevelPropertyObject *properties = level->properties.objects + index;
    *properties = circle.properties;
    properties->physical.entries = properties->physical_entries;
    level->properties.world_entries[index] =
        (AbDataEntry){{.kind = AB_DATA_STRING, .string = input.name},
                      {.kind = AB_DATA_TABLE, .table = &properties->physical}};
    ++level->bodies.count;
    ++level->properties.count;
    ++level->properties.world.count;
    ++level->frame.count;
    level->character_objects[index].jump.position = input.position;
    if (!ab_string_map_set(level->world_names, input.name, index, true, error) ||
        !refresh_world_order(level, error))
        return false;
    *output = index;
    return true;
}

bool ab_classic_level_raycast(const AbClassicLevel *level, AbVec2 first, AbVec2 second,
                              AbLevelRaycast *output, AbError *error) {
    if (!level) {
        return ab_fail(error, "Missing classic level for raycast");
    }
    return ab_level_bodies_raycast(&level->world, &level->bodies,
                                   level->removed_objects, first, second, output,
                                   error);
}

bool ab_classic_level_predict_impact(const AbClassicLevel *level,
                                     AbBirdImpactInput *input,
                                     AbBirdImpactWorldHost *host,
                                     AbBirdImpactPrediction *output, AbError *error) {
    if (!level) {
        return ab_fail(error, "Missing classic level for impact prediction");
    }
    AbBirdImpactWorld world = {&level->world, &level->bodies, level->removed_objects};
    return ab_bird_impact_predict_world(input, &world, host, output, error);
}

typedef struct {
    AbClassicLevel *level;
    size_t object;
    AbClassicImpactServices *services;
} ClassicImpactBinding;

static bool impact_object_radius(void *context, float *output, AbError *error) {
    ClassicImpactBinding *binding = context;
    AbDataValue value = {0};
    if (!ab_level_property_get(&binding->level->properties, binding->object,
                               (AbSpan){(const uint8_t *)"radius", 6}, &value, error)) {
        return false;
    }
    if (value.kind != AB_DATA_NUMBER) {
        return ab_fail(error, "Unsupported nonnumeric prediction object radius");
    }
    memcpy(output, &value.bits, sizeof(*output));
    return true;
}

static bool impact_object_print(void *context, size_t raycasts, AbError *error) {
    AbClassicImpactServices *services = ((ClassicImpactBinding *)context)->services;
    return services->print
               ? services->print(services->context, raycasts, error)
               : ab_fail(error, "Missing object prediction diagnostic service");
}

static bool impact_object_observe(void *context, AbVec2 first, AbVec2 second,
                                  const AbLevelRaycast *result, AbError *error) {
    AbClassicImpactServices *services = ((ClassicImpactBinding *)context)->services;
    return !services->observe ||
           services->observe(services->context, first, second, result, error);
}

bool ab_classic_level_predict_object_impact(AbClassicLevel *level, size_t object,
                                            AbBirdImpactInput *input,
                                            AbClassicImpactServices *services,
                                            AbBirdImpactPrediction *output,
                                            AbError *error) {
    if (!level || !services || object >= level->bodies.count) {
        return ab_fail(error, "Invalid owned prediction object or services");
    }
    ClassicImpactBinding binding = {level, object, services};
    AbBirdImpactWorldHost host = {&binding, services->maximum_raycasts,
                                  impact_object_radius, impact_object_print,
                                  impact_object_observe};
    return ab_classic_level_predict_impact(level, input, &host, output, error);
}

static bool valid_transform_object(const AbClassicLevel *level, size_t object) {
    return level && object < level->bodies.count &&
           level->frame.count == level->bodies.count &&
           level->frame.objects[object].body ==
               level->bodies.objects[object].physics.body;
}

bool ab_classic_level_forget_object(AbClassicLevel *level, size_t object,
                                    AbError *error) {
    if (!valid_transform_object(level, object)) {
        return ab_fail(error, "Invalid classic object lifetime binding");
    }
    level->removed_objects[object] = true;
    return true;
}

bool ab_classic_level_set_position(AbClassicLevel *level, size_t object,
                                   AbVec2 position, AbError *error) {
    if (!valid_transform_object(level, object)) {
        return ab_fail(error, "Invalid classic object position binding");
    }
    if (level->removed_objects[object]) {
        return true;
    }
    if (!ab_object_position_set(&level->world, level->frame.objects + object,
                                &level->character_objects[object].jump.position,
                                position, error))
        return false;
    return !ab_world_has_body(&level->world, level->frame.objects[object].body) ||
           ab_level_properties_publish_position(&level->properties, object, position,
                                                error);
}

bool ab_classic_level_set_rotation(AbClassicLevel *level, size_t object, float angle,
                                   AbError *error) {
    if (!valid_transform_object(level, object)) {
        return ab_fail(error, "Invalid classic object rotation binding");
    }
    if (level->removed_objects[object]) {
        return true;
    }
    if (!ab_object_rotation_set(&level->world, level->frame.objects + object,
                                &level->character_objects[object].jump.rotation, angle,
                                error))
        return false;
    return !ab_world_has_body(&level->world, level->frame.objects[object].body) ||
           ab_level_properties_publish_angle(
               &level->properties, object,
               level->character_objects[object].jump.rotation, error);
}

bool ab_classic_level_apply_jump(void *context, size_t object,
                                 const AbBirdJumpFrame *frame, AbError *error) {
    AbClassicLevel *level = context;
    if (!valid_transform_object(level, object) || !frame ||
        frame->event_count > sizeof(frame->events) / sizeof(frame->events[0])) {
        return ab_fail(error, "Invalid classic jump binding or event count");
    }
    for (size_t index = 0; index < frame->event_count; ++index) {
        const AbBirdJumpEvent *event = frame->events + index;
        if (!isfinite(event->value.x) ||
            (event->kind == AB_BIRD_JUMP_POSITION && !isfinite(event->value.y)) ||
            (event->kind != AB_BIRD_JUMP_POSITION &&
             event->kind != AB_BIRD_JUMP_ROTATION)) {
            return ab_fail(error, "Unsupported classic jump transform event");
        }
    }
    for (size_t index = 0; index < frame->event_count; ++index) {
        const AbBirdJumpEvent *event = frame->events + index;
        bool okay =
            event->kind == AB_BIRD_JUMP_POSITION
                ? ab_classic_level_set_position(level, object, event->value, error)
                : ab_classic_level_set_rotation(level, object, event->value.x, error);
        if (!okay) {
            return false;
        }
    }
    return true;
}

bool ab_classic_level_publish_properties(AbClassicLevel *level, size_t object,
                                         const AbObjectProperties *properties,
                                         AbError *error) {
    if (!level || object >= level->bodies.count || !properties) {
        return ab_fail(error, "Invalid classic object-property binding");
    }
    if (level->removed_objects[object]) {
        return true;
    }
    level->character_objects[object].jump.position = properties->pose.position;
    level->character_objects[object].jump.rotation = properties->pose.angle;
    AbClassicMotionProperties *motion = level->motion_properties + object;
    motion->velocity = properties->velocity;
    motion->sleeping = properties->sleeping;
    motion->outside_boundaries = properties->outside_boundaries;
    motion->initialized = true;
    return ab_level_properties_publish_motion(&level->properties, object, properties,
                                              error);
}

static bool valid_property_object(const AbClassicLevel *level, size_t object) {
    return valid_transform_object(level, object) && !level->world.failed &&
           !level->removed_objects[object] && !level->world.locked &&
           ab_world_has_body(&level->world, level->frame.objects[object].body);
}

bool ab_classic_level_property_plan(AbClassicLevel *level, size_t object,
                                    int32_t minimum_x, int32_t maximum_x,
                                    AbObjectPropertyPlan *output, AbError *error) {
    if (!valid_property_object(level, object)) {
        return ab_fail(error, "Invalid classic property-plan body");
    }
    const AbGameFrameObject *frame = level->frame.objects + object;
    const AbBodyMotion *body = &level->world.storage.bodies[frame->body].motion;
    return ab_object_properties_plan(body, frame,
                                     level->motion_properties[object].previous_awake,
                                     minimum_x, maximum_x, output, error);
}

bool ab_classic_level_finish_object(AbClassicLevel *level, size_t object,
                                    AbError *error) {
    if (!valid_property_object(level, object)) {
        return ab_fail(error, "Invalid classic object-finish body");
    }
    size_t body = level->frame.objects[object].body;
    level->motion_properties[object].previous_awake =
        level->world.storage.bodies[body].motion.awake;
    return true;
}

AbClassicLevelView ab_classic_level_view(AbClassicLevel *level) {
    if (!level) {
        return (AbClassicLevelView){0};
    }
    return (AbClassicLevelView){.world = &level->world,
                                .joints = &level->joints,
                                .bodies = &level->bodies,
                                .frame = &level->frame,
                                .collisions = &level->collisions,
                                .object_order = level->order.strings.indices,
                                .order_hash_capacity = level->order.hash_capacity,
                                .world_count = level->world_count,
                                .data = level->data,
                                .catalog = level->catalog,
                                .catalog_data = level->catalog_source,
                                .sprites = level->sprites,
                                .timers = level->timers,
                                .timers_initialized = level->timers_initialized,
                                .bird_order = level->bird_order.strings.indices,
                                .bird_count = level->bird_count,
                                .bird_hash_capacity = level->bird_order.hash_capacity,
                                .goal_order = level->goal_order.strings.indices,
                                .goal_count = level->goal_count,
                                .goal_hash_capacity = level->goal_order.hash_capacity,
                                .character_objects = level->character_objects,
                                .initial_sprites = level->initial_sprites,
                                .sprites_initialized = level->sprites_initialized,
                                .motion_properties = level->motion_properties,
                                .removed_objects = level->removed_objects,
                                .properties = &level->properties};
}
