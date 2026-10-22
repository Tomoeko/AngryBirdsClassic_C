#include "ab/coordinates.h"

#include <math.h>

static bool finite_vector(AbVec2 value) {
    return isfinite(value.x) && isfinite(value.y);
}

static bool world_to_screen(const AbCoordinateCamera *camera, AbVec2 point,
                            AbVec2 *output) {
    if (!camera || !finite_vector(camera->top_left) || !finite_vector(camera->zoom)) {
        return false;
    }
    AbVec2 relative = {point.x - camera->top_left.x, point.y - camera->top_left.y};
    AbVec2 result = {relative.x * camera->zoom.x, relative.y * camera->zoom.y};
    if (!finite_vector(relative) || !finite_vector(result)) {
        return false;
    }
    *output = result;
    return true;
}

static bool screen_to_world(const AbCoordinateCamera *camera, AbVec2 point,
                            AbVec2 *output) {
    if (!camera || !finite_vector(camera->top_left) || !finite_vector(camera->zoom) ||
        camera->zoom.x == 0 || camera->zoom.y == 0) {
        return false;
    }
    AbVec2 relative = {point.x / camera->zoom.x, point.y / camera->zoom.y};
    AbVec2 result = {relative.x + camera->top_left.x, relative.y + camera->top_left.y};
    if (!finite_vector(relative) || !finite_vector(result)) {
        return false;
    }
    *output = result;
    return true;
}

bool ab_coordinate_point(const AbCoordinateCamera *camera, AbCoordinateSpace source,
                         AbCoordinateSpace target, AbVec2 point, AbVec2 *output) {
    if (!output || (unsigned)source > AB_SPACE_PHYSICS ||
        (unsigned)target > AB_SPACE_PHYSICS || !finite_vector(point)) {
        return false;
    }
    if (source == target) {
        *output = point;
        return true;
    }
    AbVec2 world = point;
    if (source == AB_SPACE_SCREEN) {
        if (!screen_to_world(camera, point, &world)) {
            return false;
        }
    } else if (source == AB_SPACE_PHYSICS) {
        world = (AbVec2){point.x * 20.0f, point.y * 20.0f};
    }
    if (!finite_vector(world)) {
        return false;
    }
    AbVec2 result = world;
    if (target == AB_SPACE_SCREEN) {
        if (!world_to_screen(camera, world, &result)) {
            return false;
        }
    } else if (target == AB_SPACE_PHYSICS) {
        result = (AbVec2){world.x * 0.05f, world.y * 0.05f};
    }
    if (!finite_vector(result)) {
        return false;
    }
    *output = result;
    return true;
}

bool ab_coordinate_distance(const AbCoordinateCamera *camera, AbCoordinateSpace source,
                            AbCoordinateSpace target, AbVec2 delta, AbVec2 *output) {
    AbVec2 origin, endpoint;
    if (!output || !ab_coordinate_point(camera, source, target, (AbVec2){0}, &origin) ||
        !ab_coordinate_point(camera, source, target, delta, &endpoint)) {
        return false;
    }
    AbVec2 result = {endpoint.x - origin.x, endpoint.y - origin.y};
    if (!finite_vector(result)) {
        return false;
    }
    *output = result;
    return true;
}

bool ab_coordinate_screen_scale(const AbCoordinateCamera *camera, float distance,
                                float *output) {
    if (!camera || !output || !isfinite(camera->zoom.x) || camera->zoom.x == 0 ||
        !isfinite(distance)) {
        return false;
    }
    float relative = distance / camera->zoom.x;
    float result = relative * 0.05f;
    if (!isfinite(relative) || !isfinite(result)) {
        return false;
    }
    *output = result;
    return true;
}
