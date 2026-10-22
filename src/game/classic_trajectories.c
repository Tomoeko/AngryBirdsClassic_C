#include "ab/classic_trajectories.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    AbLuaAssetRenderer *renderer;
    AbDataTable *object;
    bool active;
} TrajectoryPoint;

struct AbClassicTrajectories {
    AbClassicTrajectoryConfig config;
    AbClassicLevelView level;
    AbTrajectoryFollow follow;
    TrajectoryPoint *points;
    size_t used, active, bytes;
    bool busy, failed;
};

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL && (value.kind != AB_DATA_BOOLEAN || value.bits);
}

static bool begin(AbClassicTrajectories *owner, AbError *error) {
    if (!owner || owner->busy || owner->failed || owner->level.world->failed)
        return ab_fail(error, "Invalid, failed or reentrant trajectory owner");
    owner->busy = true;
    return true;
}

static bool finish(AbClassicTrajectories *owner, bool okay) {
    owner->busy = false;
    owner->failed = !okay;
    return okay;
}

static bool canonical(AbClassicTrajectories *owner, AbDataValue value, bool allow_nil,
                      AbDataTable **output, AbError *error) {
    if (value.kind == AB_DATA_NIL && allow_nil) {
        *output = NULL;
        return true;
    }
    if (value.kind == AB_DATA_TABLE) {
        for (size_t index = 0; index < owner->level.bodies->count; ++index) {
            if (value.table == &owner->level.properties->objects[index].physical) {
                *output = value.table;
                return true;
            }
        }
    }
    return ab_fail(error, "Foreign ordinary trajectory object");
}

static bool create_point(AbClassicTrajectories *owner, AbDataTable *object,
                         AbSpan sprite, AbVec2 position, float angle, float layer,
                         AbError *error) {
    if (owner->used == owner->config.points)
        return ab_fail(error, "Owned trajectory point storage exhausted");
    if ((sprite.size && !sprite.data) || !isfinite(position.x) ||
        !isfinite(position.y) || !isfinite(angle) || !isfinite(layer))
        return ab_fail(error, "Unsupported ordinary trajectory point");
    AbLuaAssetRenderer *renderer = NULL;
    if (!ab_classic_render_scene_create_lua(owner->config.scene, sprite, layer,
                                            &renderer, error))
        return false;
    renderer->pose.world_position = position;
    renderer->pose.angle_radians = angle;
    /* renderers[id] overwrites a rounded numeric key without removing the old
     * scene renderer. Keep those two memberships separate. */
    for (size_t index = 0; index < owner->used; ++index) {
        TrajectoryPoint *point = owner->points + index;
        if (point->active &&
            point->renderer->id.script_number == renderer->id.script_number) {
            point->active = false;
            --owner->active;
        }
    }
    owner->points[owner->used++] = (TrajectoryPoint){renderer, object, true};
    ++owner->active;
    return true;
}

static bool emit(void *context, const AbTrajectoryPoint *point, AbError *error) {
    return create_point(context, point->object, point->sprite, point->position,
                        point->angle, (float)point->layer, error);
}

bool ab_classic_trajectories_open(AbClassicTrajectoryConfig config,
                                  AbClassicTrajectories **output, AbError *error) {
    if (!output)
        return ab_fail(error, "Missing owned trajectory output");
    *output = NULL;
    AbClassicLevelView level = ab_classic_level_view(config.level);
    if (!level.bodies || !level.properties ||
        ab_classic_render_scene_level(config.scene) != config.level ||
        config.tracks > UINT32_C(16777216) || config.points > UINT32_C(16777216) ||
        config.storage_bytes < sizeof(AbClassicTrajectories))
        return ab_fail(error, "Invalid owned trajectory configuration");
    size_t bytes = sizeof(AbClassicTrajectories);
    if (config.tracks > (config.storage_bytes - bytes) / sizeof(AbTrajectoryTrack))
        return ab_fail(error, "Owned trajectory track byte limit exceeded");
    bytes += config.tracks * sizeof(AbTrajectoryTrack);
    if (config.points > (config.storage_bytes - bytes) / sizeof(TrajectoryPoint))
        return ab_fail(error, "Owned trajectory point byte limit exceeded");
    bytes += config.points * sizeof(TrajectoryPoint);
    AbClassicTrajectories *owner = calloc(1, sizeof(*owner));
    if (!owner)
        return ab_fail(error, "Cannot allocate ordinary trajectory owner");
    owner->config = config;
    owner->level = level;
    owner->bytes = bytes;
    owner->follow =
        (AbTrajectoryFollow){.capacity = config.tracks, .context = owner, .emit = emit};
    if (config.tracks)
        owner->follow.tracks = calloc(config.tracks, sizeof(*owner->follow.tracks));
    if (config.points)
        owner->points = calloc(config.points, sizeof(*owner->points));
    if ((config.tracks && !owner->follow.tracks) || (config.points && !owner->points)) {
        ab_classic_trajectories_close(owner);
        return ab_fail(error, "Cannot allocate ordinary trajectory storage");
    }
    *output = owner;
    return true;
}

void ab_classic_trajectories_close(AbClassicTrajectories *owner) {
    if (owner) {
        free(owner->follow.tracks);
        free(owner->points);
        free(owner);
    }
}

size_t ab_classic_trajectories_bytes(const AbClassicTrajectories *owner) {
    return owner ? owner->bytes : 0;
}

AbClassicTrajectoryView
ab_classic_trajectories_view(const AbClassicTrajectories *owner) {
    return owner ? (AbClassicTrajectoryView){owner->follow.count, owner->active,
                                             owner->used}
                 : (AbClassicTrajectoryView){0};
}

bool ab_classic_trajectories_add(AbClassicTrajectories *owner, AbDataValue object,
                                 const AbSpan *sprites, size_t count,
                                 bool world_coordinates, bool velocity_angle,
                                 AbError *error) {
    if (!begin(owner, error))
        return false;
    AbDataTable *physical = NULL;
    bool okay = canonical(owner, object, false, &physical, error) &&
                ab_trajectory_follow_add(&owner->follow, physical, sprites, count,
                                         world_coordinates, velocity_angle, error);
    return finish(owner, okay);
}

bool ab_classic_trajectories_remove(AbClassicTrajectories *owner, AbDataValue object,
                                    AbError *error) {
    if (!begin(owner, error))
        return false;
    /* An unknown table/nil is a legitimate absent-match token: original
     * table.remove(nil-index) then removes the last track. Do not resolve it. */
    bool okay =
        ((object.kind == AB_DATA_TABLE && object.table) || object.kind == AB_DATA_NIL)
            ? ab_trajectory_follow_remove(
                  &owner->follow, object.kind == AB_DATA_TABLE ? object.table : NULL,
                  error)
            : ab_fail(error, "Unsupported trajectory removal token");
    return finish(owner, okay);
}

bool ab_classic_trajectories_update(AbClassicTrajectories *owner, AbError *error) {
    return begin(owner, error) &&
           finish(owner, ab_trajectory_follow_update(&owner->follow, error));
}

static bool dispose(AbClassicTrajectories *owner, TrajectoryPoint *point,
                    AbError *error) {
    bool found = false;
    return ab_classic_render_scene_remove_number(
        owner->config.scene, point->renderer->id.script_number, &found, error);
}

bool ab_classic_trajectories_keep(AbClassicTrajectories *owner,
                                  const AbDataValue *objects, size_t count,
                                  AbError *error) {
    if (!begin(owner, error))
        return false;
    if ((count && !objects) || count > UINT32_C(16777216))
        return finish(owner, ab_fail(error, "Invalid ordinary trajectory keep list"));
    for (size_t index = 0; index < count; ++index) {
        AbDataTable *physical = NULL;
        if (!canonical(owner, objects[index], true, &physical, error))
            return finish(owner, false);
    }
    for (size_t index = 0; index < owner->used; ++index) {
        TrajectoryPoint *point = owner->points + index;
        if (!point->active)
            continue;
        bool keep = false;
        for (size_t slot = 0; slot < count; ++slot)
            if (objects[slot].kind == AB_DATA_TABLE &&
                objects[slot].table == point->object)
                keep = true;
        if (keep)
            continue;
        if (!dispose(owner, point, error))
            return finish(owner, false);
        point->active = false;
        --owner->active;
    }
    return finish(owner, true);
}

bool ab_classic_trajectories_clear(AbClassicTrajectories *owner, AbError *error) {
    if (!begin(owner, error))
        return false;
    for (size_t index = 0; index < owner->used; ++index)
        if (owner->points[index].active &&
            !dispose(owner, owner->points + index, error))
            return finish(owner, false);
    for (size_t index = 0; index < owner->used; ++index)
        owner->points[index].active = false;
    owner->active = 0;
    owner->follow.count = 0;
    return finish(owner, true);
}

bool ab_classic_trajectories_puff(AbClassicTrajectories *owner, AbVec2 position,
                                  AbDataValue object, AbDataValue sprite,
                                  AbDataValue layer, AbError *error) {
    if (!begin(owner, error))
        return false;
    AbDataTable *physical = NULL;
    AbSpan name = {(const uint8_t *)"BIRD_SPECIAL", 12};
    float z = 3;
    if (!canonical(owner, object, true, &physical, error))
        return finish(owner, false);
    if (truthy(sprite)) {
        if (sprite.kind != AB_DATA_STRING)
            return finish(owner, ab_fail(error, "Unsupported ordinary puff sprite"));
        name = sprite.string;
    }
    if (truthy(layer)) {
        if (layer.kind != AB_DATA_NUMBER)
            return finish(owner, ab_fail(error, "Unsupported ordinary puff layer"));
        memcpy(&z, &layer.bits, sizeof(z));
    }
    return finish(owner, create_point(owner, physical, name, position, 0, z, error));
}

bool ab_classic_trajectories_museum(AbClassicTrajectories *owner,
                                    const AbMuseumBirdCall *call, AbError *error) {
    if (!call)
        return ab_fail(error, "Missing Museum trajectory call");
    switch (call->operation) {
        case AB_MUSEUM_TRAJECTORY_ADD:
            return ab_classic_trajectories_add(owner, call->object, NULL, 0, false,
                                               false, error);
        case AB_MUSEUM_TRAJECTORY_REMOVE:
            return ab_classic_trajectories_remove(owner, call->object, error);
        case AB_MUSEUM_TRAJECTORY_KEEP:
            return ab_classic_trajectories_keep(owner, &call->object, 1, error);
        default:
            return ab_fail(error,
                           "Museum camera call requires its actual camera service");
    }
}

bool ab_classic_trajectories_blues(void *context, AbBluesTrajectoryCall *call,
                                   AbError *error) {
    AbClassicTrajectories *owner = context;
    if (!owner || !call)
        return ab_fail(error, "Invalid Blues trajectory call");
    switch (call->operation) {
        case AB_BLUES_TRAJECTORY_ADD:
            return ab_classic_trajectories_add(owner, call->object, NULL, 0, false,
                                               false, error);
        case AB_BLUES_TRAJECTORY_REMOVE:
            return ab_classic_trajectories_remove(owner, call->object, error);
        case AB_BLUES_TRAJECTORY_KEEP:
            if (call->keep_slots > 4)
                return ab_fail(error, "Invalid Blues trajectory keep constructor");
            return ab_classic_trajectories_keep(owner, call->keep, call->keep_slots,
                                                error);
        case AB_BLUES_TRAJECTORY_GET_OBJECT:
            if (owner->busy || owner->failed)
                return ab_fail(error, "Failed or reentrant Blues object lookup");
            return ab_object_world_lookup(&owner->level.properties->lookup, call->name,
                                          &call->result, error);
    }
    return ab_fail(error, "Unknown Blues trajectory operation");
}
