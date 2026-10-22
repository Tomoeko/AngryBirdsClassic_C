#include "ab/trajectory_follow.h"
#include "ab/rotation.h"

#include <math.h>
#include <string.h>

static const AbSpan default_sprites[] = {{(const uint8_t *)"TRAIL_WHITE_1", 13},
                                         {(const uint8_t *)"TRAIL_WHITE_2", 13},
                                         {(const uint8_t *)"TRAIL_WHITE_3", 13}};

static bool valid(const AbTrajectoryFollow *follow, AbError *error) {
    if (!follow || follow->count > follow->capacity ||
        follow->capacity > UINT32_C(0x1000000) || (follow->capacity && !follow->tracks))
        return ab_fail(error, "Invalid trajectory follow storage");
    return true;
}

static bool position(const AbDataTable *object, bool world, AbVec2 *output,
                     AbError *error) {
    if (!object || object->count > object->capacity ||
        (object->count && !object->entries))
        return ab_fail(error, "Missing trajectory object table");
    AbVec2 point;
    float *components[] = {&point.x, &point.y};
    const char *keys[] = {"x", "y"};
    for (size_t index = 0; index < 2; ++index) {
        AbDataValue key = {.kind = AB_DATA_STRING,
                           .string = {(const uint8_t *)keys[index], 1}};
        const AbDataValue *value = ab_data_get(object, key);
        if (!value || value->kind != AB_DATA_NUMBER)
            return ab_fail(error, "Trajectory position is not numeric");
        memcpy(components[index], &value->bits, sizeof(value->bits));
        if (!world)
            *components[index] *= 20;
        if (!isfinite(*components[index]))
            return ab_fail(error, "Unsupported nonfinite trajectory position");
    }
    *output = point;
    return true;
}

static bool sprite_list(const AbSpan *sprites, size_t count, AbError *error) {
    if (!sprites || !count || count > UINT32_C(0x1000000))
        return ab_fail(error, "Unsupported trajectory sprite list");
    for (size_t index = 0; index < count; ++index) {
        if (sprites[index].size > UINT32_MAX ||
            (sprites[index].size && !sprites[index].data))
            return ab_fail(error, "Invalid trajectory sprite string");
    }
    return true;
}

bool ab_trajectory_follow_add(AbTrajectoryFollow *follow, AbDataTable *object,
                              const AbSpan *sprites, size_t sprite_count,
                              bool world_coordinates, bool velocity_angle,
                              AbError *error) {
    if (!valid(follow, error))
        return false;
    if (follow->count == follow->capacity)
        return ab_fail(error, "Trajectory follow storage exhausted");
    if (!sprites) {
        sprites = default_sprites;
        sprite_count = 3;
    }
    AbVec2 initial;
    if (!sprite_list(sprites, sprite_count, error) ||
        !position(object, false, &initial, error))
        return false;
    follow->tracks[follow->count++] =
        (AbTrajectoryTrack){.object = object,
                            .sprites = sprites,
                            .sprite_count = sprite_count,
                            .last_position = initial,
                            .world_coordinates = world_coordinates,
                            .velocity_angle = velocity_angle};
    return true;
}

bool ab_trajectory_follow_remove(AbTrajectoryFollow *follow, AbDataTable *object,
                                 AbError *error) {
    if (!valid(follow, error))
        return false;
    if (!follow->count)
        return true;
    size_t selected = follow->count - 1;
    for (size_t index = 0; index < follow->count; ++index) {
        if (follow->tracks[index].object == object)
            selected = index;
    }
    --follow->count;
    memmove(follow->tracks + selected, follow->tracks + selected + 1,
            (follow->count - selected) * sizeof(*follow->tracks));
    follow->tracks[follow->count] = (AbTrajectoryTrack){0};
    return true;
}

bool ab_trajectory_follow_update(AbTrajectoryFollow *follow, AbError *error) {
    if (!valid(follow, error))
        return false;
    const float spacing = (float)sqrt(1000.0);
    for (size_t index = 0; index < follow->count; ++index) {
        AbTrajectoryTrack *track = follow->tracks + index;
        AbVec2 current;
        if (!position(track->object, track->world_coordinates, &current, error))
            return false;
        float dx = current.x - track->last_position.x;
        float dy = current.y - track->last_position.y;
        float squared = dx * dx + dy * dy;
        if (!isfinite(squared))
            return ab_fail(error, "Unsupported nonfinite trajectory displacement");
        float distance = (float)sqrt((double)squared);
        if (!(distance > spacing))
            continue;
        float angle = 0;
        if (track->velocity_angle && !ab_script_angle_from_components(dy, dx, &angle))
            return ab_fail(error, "Unsupported trajectory angle");
        if (!track->sprites || !track->sprite_count ||
            track->sprite_count > UINT32_C(0x1000000) ||
            track->latest_sprite > track->sprite_count)
            return ab_fail(error, "Invalid live trajectory sprite cycle");
        track->latest_sprite =
            track->latest_sprite == track->sprite_count ? 1 : track->latest_sprite + 1;
        AbTrajectoryPoint point = {.object = track->object,
                                   .sprite = track->sprites[track->latest_sprite - 1],
                                   .position = current,
                                   .angle = angle,
                                   .layer = 2};
        if (!follow->emit)
            return ab_fail(error, "Missing trajectory point renderer");
        if (!follow->emit(follow->context, &point, error))
            return false;
        track->last_position = current;
    }
    return true;
}
