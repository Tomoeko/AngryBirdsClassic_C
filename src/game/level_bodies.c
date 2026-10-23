#include "ab/level_bodies.h"

#include <math.h>
#include <string.h>

static AbDataValue field(const AbDataTable *table, const char *name) {
    const AbDataValue *value = ab_data_get(
        table, (AbDataValue){.kind = AB_DATA_STRING,
                             .string = {(const uint8_t *)name, strlen(name)}});
    return value ? *value : (AbDataValue){0};
}

static bool named(AbDataValue value, const char *name) {
    size_t size = strlen(name);
    return value.kind == AB_DATA_STRING && value.string.size == size &&
           (!size || value.string.data) &&
           (!size || memcmp(value.string.data, name, size) == 0);
}

static bool same_name(AbDataValue first, AbDataValue second) {
    return first.kind == AB_DATA_STRING && second.kind == AB_DATA_STRING &&
           first.string.size == second.string.size &&
           (!first.string.size || (first.string.data && second.string.data)) &&
           (!first.string.size ||
            memcmp(first.string.data, second.string.data, first.string.size) == 0);
}

static bool empty_list(const AbDataTable *level, const char *name) {
    AbDataValue value = field(level, name);
    return value.kind == AB_DATA_NIL ||
           (value.kind == AB_DATA_TABLE && value.table && value.table->count == 0);
}

bool ab_level_gravity(const AbDataTable *level, AbVec2 *output, AbError *error) {
    if (!level || !output) {
        return ab_fail(error, "Missing classic level gravity source or output");
    }
    AbDataValue value = field(level, "worldGravity");
    float gravity = 20;
    if (value.kind != AB_DATA_NIL &&
        (value.kind != AB_DATA_BOOLEAN || value.bits != 0)) {
        if (value.kind != AB_DATA_NUMBER) {
            return ab_fail(error, "Unsupported classic level gravity value");
        }
        memcpy(&gravity, &value.bits, sizeof(gravity));
        if (!isfinite(gravity)) {
            return ab_fail(error, "Nonfinite classic level gravity");
        }
    }
    *output = (AbVec2){0, gravity};
    return true;
}

bool ab_level_placement_scale(const AbDataTable *level, float physics_to_world,
                              float *output, AbError *error) {
    if (!level || !output || !isfinite(physics_to_world) || physics_to_world == 0)
        return ab_fail(error, "Invalid Museum placement-scale source or denominator");
    AbDataValue value = field(level, "physicsToWorld");
    float authored = 100;
    if (value.kind != AB_DATA_NIL) {
        if (value.kind != AB_DATA_NUMBER)
            return ab_fail(error,
                           "Museum placement scale requires a numeric author value");
        memcpy(&authored, &value.bits, sizeof(authored));
    }
    float scale = authored / physics_to_world;
    if (!isfinite(authored) || !isfinite(scale))
        return ab_fail(error, "Nonfinite Museum placement scale");
    *output = scale;
    return true;
}

static bool placement_position(const AbDataTable *placement, float scale,
                               AbVec2 *position) {
    AbDataValue x = field(placement, "x"), y = field(placement, "y");
    if (x.kind != AB_DATA_NUMBER || y.kind != AB_DATA_NUMBER) {
        return false;
    }
    memcpy(&position->x, &x.bits, sizeof(position->x));
    memcpy(&position->y, &y.bits, sizeof(position->y));
    position->x = position->x * scale;
    position->y = position->y * scale;
    return isfinite(position->x) && isfinite(position->y);
}

static bool sprite_size(const AbLevelBodySource *source,
                        const AbObjectDefinition *definition, AbVec2 *size,
                        AbError *error) {
    AbDataValue type = field(definition->source, "type");
    bool circle = named(type, "circle");
    if ((circle && field(definition->source, "radius").kind != AB_DATA_NIL) ||
        (!circle && field(definition->source, "width").kind != AB_DATA_NIL &&
         field(definition->source, "height").kind != AB_DATA_NIL)) {
        return true;
    }
    AbDataValue sprite = field(definition->source, "sprite");
    if (sprite.kind == AB_DATA_NIL) {
        AbDataValue damage = field(definition->source, "damageSprites");
        AbDataValue first = damage.kind == AB_DATA_TABLE
                                ? field(damage.table, "damage1")
                                : (AbDataValue){0};
        sprite = first.kind == AB_DATA_TABLE ? field(first.table, "sprite")
                                             : (AbDataValue){0};
    }
    if (!source->sprite_size || sprite.kind != AB_DATA_STRING || !sprite.string.size) {
        return ab_fail(error, "Missing classic object sprite bounds");
    }
    return source->sprite_size(source->sprite_context, sprite.string, size, error);
}

static bool create_object(AbPhysicsWorld *world, const AbLevelBodySource *source,
                          const AbDataEntry *entry, AbLevelObject *output,
                          bool *created, AbError *error) {
    *created = false;
    if (entry->value.kind != AB_DATA_TABLE || !entry->value.table) {
        return ab_fail(error, "Level object placement must be a table");
    }
    const AbDataTable *placement = entry->value.table;
    AbDataValue name = field(placement, "name");
    AbDataValue definition_name = field(placement, "definition");
    AbDataValue angle = field(placement, "angle");
    if (!same_name(entry->key, name) || !name.string.size ||
        definition_name.kind != AB_DATA_STRING || !definition_name.string.size ||
        !definition_name.string.data) {
        return ab_fail(error, "Unsupported classic object placement fields");
    }
    AbDataValue blocks = field(source->catalog, "blocks");
    if (blocks.kind != AB_DATA_TABLE || !blocks.table ||
        blocks.table->count > blocks.table->capacity ||
        (blocks.table->count && !blocks.table->entries)) {
        return ab_fail(error, "Invalid classic block definition catalog");
    }
    const AbDataValue *found = ab_data_get(blocks.table, definition_name);
    if (!found || found->kind == AB_DATA_NIL) {
        if (source->missing_definition) {
            source->missing_definition(source->missing_context, definition_name.string);
        }
        return true;
    }
    if (angle.kind != AB_DATA_NUMBER) {
        return ab_fail(error, "Unsupported classic object placement angle");
    }
    /* MuseumMode passes these overrides, not the whole level-object table. */
    AbDataEntry overrides[] = {
        {{.kind = AB_DATA_STRING, .string = {(const uint8_t *)"angle", 5}}, angle},
        {{.kind = AB_DATA_STRING, .string = {(const uint8_t *)"z_order", 7}},
         field(placement, "z_order")},
    };
    AbDataTable override_table = {overrides, 2, 2, NULL};
    AbObjectDefinition definition;
    if (!ab_object_definition_open(source->catalog, definition_name.string,
                                   &override_table, &definition, error)) {
        return false;
    }
    AbVec2 position, size = {0};
    if (!placement_position(placement, source->placement_scale, &position) ||
        !sprite_size(source, &definition, &size, error)) {
        return ab_fail(error, "Cannot resolve classic object position or size");
    }
    AbLevelObject result = {
        .name = name.string,
        .definition_name = definition_name.string,
        .placement = placement,
    };
    AbDataValue goal =
        ab_object_property(&definition, (AbSpan){(const uint8_t *)"levelGoal", 9});
    result.level_goal =
        goal.kind != AB_DATA_NIL && (goal.kind != AB_DATA_BOOLEAN || goal.bits != 0);
    AbError cause = {0};
    if (!ab_object_body_create(world, &definition, position, size,
                               source->physics_scale, (uintptr_t)output,
                               &result.physics, &cause)) {
        return ab_fail(error, "Classic object %.*s: %s",
                       name.string.size > 96 ? 96 : (int)name.string.size,
                       name.string.data, cause.message);
    }
    result.initial_mass = world->storage.bodies[result.physics.body].motion.mass.mass;
    *output = result;
    *created = true;
    return true;
}

bool ab_level_bodies_create(AbPhysicsWorld *world, const AbLevelBodySource *source,
                            AbLevelBodies *output, AbStringOrderWorkspace *order,
                            AbError *error) {
    if (!world || world->failed || world->locked || !world->broadphase.tree.nodes ||
        world->body_count || world->fixture_count || !source || !source->level ||
        !source->catalog || !output || !order ||
        (output->capacity && !output->objects) || !isfinite(source->placement_scale) ||
        source->placement_scale <= 0 || !isfinite(source->physics_scale) ||
        source->physics_scale <= 0 ||
        (!source->derive_ground_position && (!isfinite(source->ground_position.x) ||
                                             !isfinite(source->ground_position.y)))) {
        return ab_fail(error, "Invalid classic level body request");
    }
    output->count = 0;
    output->load_edges_present = false;
    const AbDataTable *level = source->level->globals;
    AbDataValue placements = field(level, "world");
    if (placements.kind != AB_DATA_TABLE || !placements.table ||
        !empty_list(level, "chains")) {
        return ab_fail(error, "Unsupported classic level world or chains");
    }
    const AbDataTableLayout *layout =
        ab_lua_table_layout(source->level, placements.table);
    size_t live_count = 0;
    if (!layout || !layout->no_nil_assignments ||
        !ab_data_string_order_pruned(placements.table, layout->hash_capacity, order,
                                     &live_count, error)) {
        return ab_fail(error, "Unsupported classic world constructor ordering");
    }
    bool ground_entry = field(placements.table, "ground").kind != AB_DATA_NIL;
    size_t needed = live_count;
    if (!ground_entry) {
        if (needed == SIZE_MAX) {
            return ab_fail(error, "Classic level object count overflows");
        }
        ++needed;
    }
    if (needed > output->capacity || needed > world->storage.body_capacity ||
        needed > world->storage.fixture_capacity) {
        return ab_fail(error, "Classic level body storage is too small");
    }
    float left = 100000, right = -100000, bird_counter = 0;
    for (size_t cursor = 0; cursor < live_count; ++cursor) {
        const AbDataEntry *entry = placements.table->entries + order->indices[cursor];
        if (named(entry->key, "ground")) {
            continue;
        }
        bool present;
        if (!create_object(world, source, entry, output->objects + output->count,
                           &present, error)) {
            return false;
        }
        if (!present) {
            continue;
        }
        AbLevelObject *created = output->objects + output->count;
        const AbObjectBody *object = &created->physics;
        ++output->count;
        if (object->controllable) {
            bird_counter = bird_counter + 1.0f;
            created->start_number = field(created->placement, "startNumber");
            if (created->start_number.kind == AB_DATA_NIL) {
                created->start_number.kind = AB_DATA_NUMBER;
                memcpy(&created->start_number.bits, &bird_counter,
                       sizeof(bird_counter));
            }
        }
        if (source->derive_ground_position) {
            float x =
                world->storage.bodies[object->body].motion.mass.transform.position.x;
            float lower = x - object->dimensions.width;
            float upper = x + object->dimensions.width;
            if (!isfinite(lower) || !isfinite(upper)) {
                return ab_fail(error, "Classic load bounds exceed supported range");
            }
            if (lower < left) {
                left = lower;
            }
            if (upper > right) {
                right = upper;
            }
        }
    }
    AbLevelObject ground = {.name = {(const uint8_t *)"ground", 6},
                            .definition_name = {(const uint8_t *)"Ground", 6}};
    AbDataValue placement = field(placements.table, "ground");
    if (placement.kind == AB_DATA_TABLE) {
        ground.placement = placement.table;
    }
    AbVec2 ground_position = source->ground_position;
    if (source->derive_ground_position) {
        ground_position = (AbVec2){(left + right) * 0.5f, 5};
        output->load_edges = (AbVec2){left, right};
        output->load_edges_present = true;
    }
    if (!ab_ground_body_create(world, ground_position,
                               (uintptr_t)(output->objects + output->count),
                               &ground.physics, error)) {
        return false;
    }
    ground.initial_mass = world->storage.bodies[ground.physics.body].motion.mass.mass;
    output->objects[output->count++] = ground;
    return true;
}

static bool character_order(const AbLevelBodies *bodies, const size_t *insertion_order,
                            AbLevelObjectOrder *workspace, size_t *count, bool goals,
                            AbError *error) {
    if (!bodies || !workspace || !count || bodies->count > bodies->capacity ||
        (bodies->count &&
         (!bodies->objects || !insertion_order || !workspace->entries)) ||
        bodies->count > workspace->entry_capacity) {
        return ab_fail(error, "Invalid classic character-order storage");
    }
    size_t birds = 0;
    for (size_t cursor = 0; cursor < bodies->count; ++cursor) {
        size_t index = insertion_order[cursor];
        if (index >= bodies->count) {
            return ab_fail(error, "Invalid classic character insertion index");
        }
        for (size_t previous = 0; previous < cursor; ++previous) {
            if (insertion_order[previous] == index) {
                return ab_fail(error, "Repeated classic character insertion index");
            }
        }
        const AbLevelObject *object = bodies->objects + index;
        if (goals ? object->level_goal : object->physics.controllable) {
            workspace->entries[birds++] = (AbDataEntry){
                .key = {.kind = AB_DATA_STRING, .string = object->name},
                .value = {.kind = AB_DATA_BOOLEAN, .bits = 1},
            };
        }
    }
    AbDataTable names = {workspace->entries, birds, workspace->entry_capacity, NULL};
    size_t capacity;
    if (!ab_data_string_order_growing(&names, &workspace->strings, &capacity, error)) {
        return false;
    }
    for (size_t cursor = 0; cursor < birds; ++cursor) {
        AbDataValue key = workspace->entries[workspace->strings.indices[cursor]].key;
        size_t index = 0;
        while (index < bodies->count &&
               !same_name(key, (AbDataValue){.kind = AB_DATA_STRING,
                                             .string = bodies->objects[index].name})) {
            ++index;
        }
        if (index == bodies->count) {
            return ab_fail(error, "Missing classic character-order object binding");
        }
        workspace->strings.indices[cursor] = index;
    }
    workspace->hash_capacity = capacity;
    *count = birds;
    return true;
}

bool ab_level_birds_order(const AbLevelBodies *bodies, const size_t *insertion_order,
                          AbLevelObjectOrder *workspace, size_t *count,
                          AbError *error) {
    return character_order(bodies, insertion_order, workspace, count, false, error);
}

bool ab_level_goals_order(const AbLevelBodies *bodies, const size_t *insertion_order,
                          AbLevelObjectOrder *workspace, size_t *count,
                          AbError *error) {
    return character_order(bodies, insertion_order, workspace, count, true, error);
}

bool ab_level_bodies_order(const AbLevelBodies *bodies, AbLevelObjectOrder *workspace,
                           AbError *error) {
    if (!bodies || !workspace || bodies->count > bodies->capacity ||
        (bodies->count && (!bodies->objects || !workspace->entries)) ||
        bodies->count > workspace->entry_capacity) {
        return ab_fail(error, "Invalid classic runtime object-order storage");
    }
    for (size_t index = 0; index < bodies->count; ++index) {
        workspace->entries[index] = (AbDataEntry){
            .key = {.kind = AB_DATA_STRING, .string = bodies->objects[index].name},
            .value = {.kind = AB_DATA_BOOLEAN, .bits = 1},
        };
    }
    AbDataTable names = {workspace->entries, bodies->count, workspace->entry_capacity,
                         NULL};
    return ab_data_string_order_growing(&names, &workspace->strings,
                                        &workspace->hash_capacity, error);
}
