#ifndef AB_GAME_VIALS_H
#define AB_GAME_VIALS_H

/* VialEffects.lua blackBirdVialCollision16109. The contact callback retains
 * actual physical-table identities; vial removal/effects are separate phases. */
static bool game_vial_collision(void *context, AbDataValue name, size_t first,
                                size_t second, const AbBlockCollisionNotice *notice,
                                AbError *error) {
    (void)notice;
    PreviewGame *game = context;
    if (!menu_route_equal(name, game_text("blackBirdVialCollision")))
        return name.kind == AB_DATA_STRING
                   ? ab_fail(error, "Unresolved block collision hook: %.*s",
                             (int)(name.string.size < 128 ? name.string.size : 128),
                             name.string.data)
                   : ab_fail(error, "Unresolved block collision hook type %u",
                             (unsigned)name.kind);
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    AbClassicWorldRuntimeView world = ab_classic_world_runtime_view(game->world);
    if (first >= world.physical_count || second >= world.physical_count ||
        world.physical_values[first].kind != AB_DATA_TABLE ||
        world.physical_values[second].kind != AB_DATA_TABLE)
        return ab_fail(error, "Vial collision has no canonical physical identities");
    AbDataValue vial = world.physical_values[first];
    AbDataValue other = world.physical_values[second];
    if (menu_route_truthy(menu_route_field(other.table, "levelGoal")) &&
        !ab_string_map_set(world.dead_blocks, level.bodies->objects[first].name,
                           (uintptr_t)(world.physical_values + first), true, error))
        return false;
    AbDataTable *birds =
        ab_classic_birds_view(ab_classic_game_runtime_view(game->game).birds).birds;
    if (!birds)
        return ab_fail(error, "Vial collision requires its live bird registry");
    bool remember = false;
    for (size_t index = 0; index < birds->count; ++index)
        if (birds->entries[index].value.kind == AB_DATA_TABLE &&
            birds->entries[index].value.table == other.table) {
            remember = true;
            break;
        }
    if (!remember) {
        AbStringMap *goals =
            ab_classic_goals_map(ab_classic_dynamics_view(world.dynamics).goals);
        uintptr_t value = 0;
        bool present = false;
        if (!ab_string_map_get(goals, level.bodies->objects[second].name, &value,
                               &present, error))
            return false;
        if (present) {
            const AbDataValue *goal = (const AbDataValue *)value;
            remember =
                goal && goal->kind == AB_DATA_TABLE && goal->table == other.table;
        }
    }
    if (!remember)
        return true;
    AbObjectPropertyAccess access = {.physics_object = vial};
    return ab_object_property_set(&access, menu_span("previousCollision"), other,
                                  error);
}

#include "game_vial_common.h"
#include "game_vial_blue.h"
#include "game_vial_pink.h"
#include "game_vial_orange.h"

static bool game_vial_particles(void *context, AbDataValue parameters, AbError *error) {
    (void)parameters;
    PreviewVialObject *row = context;
    PreviewGame *game = row->owner->game;
    AbDataValue effect = menu_route_field(row->definition.table, "vialEffect");
    const char *const fields[] = {"glassBuff", "smokeBuff", "topBuff"};
    const float amounts[] = {2, 8, 1}, scales[] = {1, 5, 5};
    for (size_t index = 0; index < 3; ++index) {
        AbDataValue name = menu_route_field(effect.table, fields[index]);
        if (!menu_route_truthy(name)) {
            if (index)
                continue;
            name = game_text("vialGlassBuff");
        }
        if (name.kind != AB_DATA_STRING)
            return ab_fail(error, "Vial particles require their actual resource name");
        AbVec2 position = row->particles_position;
        float offset;
        if (index == 2) {
            if (!game_shock_optional(effect.table, "topBuffOffset", -100, &offset,
                                     error))
                return false;
            position.y += offset;
        }
        if (!ab_particles_emit(
                ab_classic_particles_engine(game->particles),
                &(AbParticleEmission){.definition = name.string,
                                      .amount = amounts[index],
                                      .position = position,
                                      .size = {scales[index], scales[index]}},
                error))
            return false;
    }
    return true;
}

static bool game_vial_removed(PreviewGame *game, AbDataTable *physical,
                              AbError *error) {
    PreviewVialObject *vial = NULL;
    if (!game->vials ||
        !game_vial_find(game->vials,
                        (AbDataValue){.kind = AB_DATA_TABLE, .table = physical}, &vial,
                        error) ||
        !game_vial_definition(game, vial->object, &vial->definition, error) ||
        !game_vial_affect_goals(vial, error) ||
        !game_shock_read(physical, "x", &vial->particles_position.x, error) ||
        !game_shock_read(physical, "y", &vial->particles_position.y, error))
        return false;
    vial->particles_position.x *= 20;
    vial->particles_position.y *= 20;
    return ab_delayed_callbacks_add(game->delayed, .1f, &vial->particles, game->absent,
                                    error);
}

static bool game_definition_removal_binding(void *context, AbDataValue key,
                                            AbUiPointerValue *output, AbError *error) {
    (void)context;
    if (menu_route_equal(key, game_text("rocketCanisterRemoved"))) {
        *output = (AbUiPointerValue){.kind = AB_UI_POINTER_FUNCTION, .function = 104};
        return true;
    }
    if (menu_route_equal(key, game_text("blackBirdOrangeVialRemoved")) ||
        menu_route_equal(key, game_text("blackBirdBlueVialRemoved")) ||
        menu_route_equal(key, game_text("blackBirdPinkVialRemoved"))) {
        *output = (AbUiPointerValue){
            .kind = AB_UI_POINTER_FUNCTION,
            .function =
                menu_route_equal(key, game_text("blackBirdOrangeVialRemoved")) ? 101
                : menu_route_equal(key, game_text("blackBirdBlueVialRemoved")) ? 102
                                                                               : 103};
        return true;
    }
    return key.kind == AB_DATA_STRING
               ? ab_fail(error, "Unresolved definition removal hook: %.*s",
                         (int)(key.string.size < 128 ? key.string.size : 128),
                         key.string.data)
               : ab_fail(error, "Unresolved definition removal hook type %u",
                         (unsigned)key.kind);
}

static bool game_definition_removal_call(void *context, AbObjectRemovalNotice notice,
                                         AbUiPointerValue function,
                                         AbObjectRemovalObject *object,
                                         AbError *error) {
    if (notice == AB_OBJECT_REMOVAL_DEFINITION && object &&
        function.kind == AB_UI_POINTER_FUNCTION && function.function == 104)
        return game_rocket_removed(context, object->context, error);
    if (notice == AB_OBJECT_REMOVAL_DEFINITION && object &&
        function.kind == AB_UI_POINTER_FUNCTION &&
        (function.function == 101 || function.function == 102 ||
         function.function == 103))
        return game_vial_removed(context, object->context, error);
    return game_removal_call(context, notice, function, object, error);
}

/* BlackBirdMode.onUpdate4559 calls updateVials before its clothing and optional
 * per-level script updates. This is that recovered subsystem phase only. */
static bool game_vial_level_update(void *context, float seconds, float time,
                                   AbError *error) {
    (void)time;
    PreviewGame *game = context;
    PreviewVials *owner = game->vials;
    if (!owner)
        return ab_fail(error, "Vial update phase has no initialized owner");
    size_t cursor = 0;
    AbStringMapEntry entry;
    while (true) {
        if (!ab_string_map_next(owner->vials, &cursor, &entry, error))
            return false;
        if (!entry.present)
            break;
        PreviewVialObject *row = (PreviewVialObject *)entry.value;
        if (!game_vial_live(row)) {
            if (!ab_string_map_set(owner->vials, entry.key, 0, false, error))
                return false;
            continue;
        }
        row->glow_time += seconds;
        if (!row->glow_animate && row->glow_time >= 0) {
            row->glow_animate = true;
            row->glow_time = 0;
        } else if (row->glow_animate && row->glow_time >= .5f) {
            float random;
            if (ab_script_random(&game->menu->random, 0, 0, 0, &random) !=
                AB_SCRIPT_RANDOM_OK)
                return ab_fail(error, "Invalid actual vial glow random state");
            row->glow_animate = false;
            row->glow_time = (-random * 2) - 1;
        }
        AbDataValue effect = menu_route_field(row->definition.table, "vialEffect");
        AbDataValue defence = menu_route_field(row->definition.table, "defence");
        float radius, x, y;
        if (!game_shock_read(effect.table, "radius", &radius, error) ||
            !game_shock_optional(effect.table, "pigDetectionRadius", radius, &radius,
                                 error) ||
            !game_shock_read(game_vial_physical(row), "x", &x, error) ||
            !game_shock_read(game_vial_physical(row), "y", &y, error))
            return false;
        AbStringMap *goals = ab_classic_goals_map(
            ab_classic_dynamics_view(
                ab_classic_world_runtime_view(game->world).dynamics)
                .goals);
        size_t goal_cursor = 0;
        AbStringMapEntry goal;
        while (true) {
            if (!ab_string_map_next(goals, &goal_cursor, &goal, error))
                return false;
            if (!goal.present)
                break;
            const AbDataValue *physical = (const AbDataValue *)goal.value;
            float target_x, target_y;
            if (!physical || physical->kind != AB_DATA_TABLE ||
                !game_shock_read(physical->table, "x", &target_x, error) ||
                !game_shock_read(physical->table, "y", &target_y, error))
                return false;
            target_x -= x;
            target_y -= y;
            if (target_x * target_x + target_y * target_y < radius * radius) {
                defence = menu_route_field(effect.table, "defenceNearPigs");
                if (!menu_route_truthy(defence))
                    defence = menu_route_number(0);
                break;
            }
        }
        if (!game_vial_write(row, "defence", defence, error))
            return false;
    }
    cursor = 0;
    while (ab_string_map_next(owner->effects, &cursor, &entry, error)) {
        if (!entry.present)
            return true;
        PreviewVialVisual *visual = (PreviewVialVisual *)entry.value;
        if (!visual->started) {
            visual->time = visual->duration;
            visual->started = true;
        }
        visual->time -= seconds * game->speed;
        if (visual->time <= 0 &&
            !ab_string_map_set(owner->effects, entry.key, 0, false, error))
            return false;
    }
    return false;
}

static bool game_vial_draw(void *context, void *parameter, AbExtraDrawInput input,
                           AbError *error) {
    (void)parameter;
    PreviewVials *owner = context;
    size_t cursor = 0;
    AbStringMapEntry entry;
    while (ab_string_map_next(owner->effects, &cursor, &entry, error)) {
        if (!entry.present)
            return true;
        PreviewVialVisual *visual = (PreviewVialVisual *)entry.value;
        PreviewVialObject *row = visual->target;
        if (!game_vial_live(row)) {
            if (!ab_string_map_set(owner->effects, entry.key, 0, false, error))
                return false;
            continue;
        }
        if (!visual->started)
            continue;
        float x, y, radius;
        if (!game_shock_read(game_vial_physical(row), "x", &x, error) ||
            !game_shock_read(game_vial_physical(row), "y", &y, error) ||
            !game_shock_read(game_vial_physical(row), "radius", &radius, error))
            return false;
        float ratio = fmaxf(0, visual->time / visual->duration);
        float theta = ratio * 0x1.921fb6p+1f;
        float wave = (float)sin((double)theta);
        float scale = ((wave + .5f) * (float)sqrt((double)(radius / 2))) * 1.25f;
        float degrees = (ratio * visual->speed) * 70;
        AbDrawCommand command;
        bool visible;
        if (!ab_game_sprite_command(
                owner->sprites + visual->kind,
                /* This callback calls physicsToWorldTransform before drawing;
                 * keep its x20 multiplication rather than dividing by .05. */
                (AbGameSpritePose){.physics_position = {x * 20, y * 20},
                                   .scale = {scale, scale},
                                   .angle =
                                       (float)((double)degrees * 0x1.1df46a2529d39p-6),
                                   .alpha = 1 - wave * .35f,
                                   .physics_scale = 1},
                input.camera, input.width, input.height, &command, &visible, error) ||
            (visible && !ab_draw_list_push(input.list, &command, error)))
            return false;
    }
    return false;
}

static bool game_vial_particle_names(PreviewGame *game, AbClassicParticleNames *names,
                                     AbError *error) {
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    game->vial_count = 0;
    for (size_t object = 0; object < level.bodies->count; ++object) {
        if (!level.bodies->objects[object].definition_name.size)
            continue;
        AbDataValue definition;
        if (!game_vial_definition(game, object, &definition, error))
            return false;
        AbDataValue effect = menu_route_field(definition.table, "vialEffect");
        if (effect.kind != AB_DATA_TABLE)
            continue;
        ++game->vial_count;
        const char *const fields[] = {"glassBuff", "smokeBuff", "topBuff"};
        for (size_t index = 0; index < 3; ++index) {
            AbDataValue value = menu_route_field(effect.table, fields[index]);
            if (!menu_route_truthy(value)) {
                if (index)
                    continue;
                value = game_text("vialGlassBuff");
            }
            if (value.kind != AB_DATA_STRING)
                return ab_fail(error, "Invalid actual vial particle dependency");
            bool present = false;
            for (size_t prior = 0; prior < names->count; ++prior)
                present |= menu_route_equal(
                    value, (AbDataValue){.kind = AB_DATA_STRING,
                                         .string = names->names[prior]});
            if (!present) {
                if (names->count == names->capacity)
                    return ab_fail(error,
                                   "Vial particle dependency capacity exhausted");
                names->names[names->count++] = value.string;
            }
        }
    }
    /* transformIntoBubble's actual floating-pig definition emits bubblePop
     * during ordinary destruction, even when no Stella bird is present. */
    if (game->vial_count) {
        AbSpan particle = menu_span("bubblePop");
        bool present = false;
        for (size_t index = 0; index < names->count; ++index)
            present |= names->names[index].size == particle.size &&
                       !memcmp(names->names[index].data, particle.data, particle.size);
        if (!present) {
            if (names->count == names->capacity)
                return ab_fail(error, "Floating-pig particle capacity exhausted");
            names->names[names->count++] = particle;
        }
    }
    return true;
}

static void game_vial_close(PreviewGame *game) {
    PreviewVials *owner = game->vials;
    if (!owner)
        return;
    ab_string_map_close(owner->vials);
    ab_string_map_close(owner->effects);
    ab_string_map_close(owner->goal_copy);
    ab_string_map_close(owner->jointed);
    ab_string_map_close(owner->capture_workspace);
    for (size_t index = 0; owner->objects && index < owner->capacity; ++index) {
        free(owner->objects[index].captured_entries);
        free(owner->objects[index].captured_indices);
    }
    free(owner->queue_entries);
    free(owner->snapshot_entries);
    free(owner->requests);
    free(owner->objects);
    free(owner);
    game->vials = NULL;
}

static bool game_vial_open(PreviewGame *game, AbError *error) {
    if (!game->vial_count)
        return true;
    size_t capacity = ab_classic_level_runtime_view(game->level).object_capacity;
    size_t per_object = sizeof(PreviewVialObject);
    if (game->vial_count >
        (SIZE_MAX - per_object) / (sizeof(PreviewVialRequest) + sizeof(AbDataEntry)))
        return ab_fail(error, "Vial callback allocation overflow");
    per_object += game->vial_count * (sizeof(PreviewVialRequest) + sizeof(AbDataEntry));
    if (capacity > (16 * 1024 * 1024) / per_object)
        return ab_fail(error, "Vial callback owner exceeds retained byte limit");
    PreviewVials *owner = calloc(1, sizeof(*owner));
    if (!owner)
        return ab_fail(error, "Cannot allocate vial owner");
    game->vials = owner;
    owner->game = game;
    owner->capacity = capacity;
    owner->request_capacity = game->vial_count;
    owner->objects = calloc(capacity, sizeof(*owner->objects));
    owner->requests = calloc(capacity * game->vial_count, sizeof(*owner->requests));
    owner->queue_entries =
        calloc(capacity * game->vial_count, sizeof(*owner->queue_entries));
    owner->snapshot_entries =
        calloc(game->vial_count, sizeof(*owner->snapshot_entries));
    owner->queue_snapshot =
        (AbDataTable){owner->snapshot_entries, 0, game->vial_count, NULL};
    size_t nodes = 1;
    while (nodes < capacity)
        nodes *= 2;
    if (!owner->objects || !owner->requests || !owner->queue_entries ||
        !owner->snapshot_entries ||
        !ab_string_map_open(0, nodes, 1024 * 1024, &owner->vials, error) ||
        !ab_string_map_open(0, nodes * 4, 1024 * 1024, &owner->effects, error) ||
        !ab_string_map_open(0, nodes, 1024 * 1024, &owner->goal_copy, error) ||
        !ab_string_map_open(0, nodes, 1024 * 1024, &owner->jointed, error) ||
        !ab_string_map_open(0, nodes, 1024 * 1024, &owner->capture_workspace, error))
        return ab_fail(error, "Cannot bind actual vial resources and registries");
    const char *const sprites[] = {"GLOOP_2", "GLOOP_1", "GLOOP_3"};
    for (size_t index = 0; index < GAME_VIAL_EFFECTS; ++index)
        if (!ab_lua_asset_sprite_bind(&game->menu->sprites.provider,
                                      menu_span(sprites[index]), owner->sprites + index,
                                      error) ||
            !owner->sprites[index].region)
            return ab_fail(error, "Missing actual vial visual sprite");
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    for (size_t object = 0; object < capacity; ++object) {
        PreviewVialObject *row = owner->objects + object;
        row->owner = owner;
        row->object = object;
        row->requests = owner->requests + object * game->vial_count;
        row->queue_entries = owner->queue_entries + object * game->vial_count;
        row->queue = (AbDataTable){row->queue_entries, 0, game->vial_count, NULL};
        row->update = (AbObjectUpdateFunction){row, game_vial_queue_update};
        row->spin = (AbObjectUpdateFunction){row, game_vial_blue_spin};
        row->floating = (AbObjectUpdateFunction){row, game_vial_pink_update};
        row->particles = (AbDelayedFunction){row, game_vial_particles};
    }
    AbClassicWorldCursor cursor = {0};
    for (;;) {
        size_t object;
        bool present;
        if (!ab_classic_level_world_next(game->playfield.level, &cursor, &object,
                                         &present, error))
            return false;
        if (!present)
            break;
        PreviewVialObject *row = owner->objects + object;
        if (!level.bodies->objects[object].definition_name.size)
            continue;
        if (!game_vial_definition(game, object, &row->definition, error))
            return false;
        if (menu_route_truthy(menu_route_field(row->definition.table, "vialEffect")) &&
            !ab_string_map_set(owner->vials, level.bodies->objects[object].name,
                               (uintptr_t)row, true, error))
            return false;
    }
    owner->draw = (AbExtraDrawFunction){.context = owner,
                                        .draw = game_vial_draw,
                                        .z = 0,
                                        .z_present = true,
                                        .before_blocks = true};
    return ab_extra_draws_set(game->extra_draws, menu_span("drawBlackBirdVials_z0"),
                              &owner->draw, error);
}

#endif
