#include "ab/physics_fixture.h"

#include "fixture_internal.h"
#include "tree_internal.h"

#include <math.h>

bool ab_fixture_shape_bounds(const AbPhysicsShape *shape, AbRigidTransform transform,
                             AbAabb *bounds) {
    if (!shape || !isfinite(transform.position.x) || !isfinite(transform.position.y) ||
        !isfinite(transform.sine) || !isfinite(transform.cosine)) {
        return false;
    }
    if (shape->kind == AB_SHAPE_CIRCLE) {
        AbCircle circle = shape->data.circle;
        if (!isfinite(circle.radius) || circle.radius < 0 ||
            !isfinite(circle.center.x) || !isfinite(circle.center.y)) {
            return false;
        }
        *bounds = ab_circle_bounds(circle, transform);
    } else if (shape->kind == AB_SHAPE_POLYGON) {
        const AbPolygon *polygon = &shape->data.polygon;
        if (polygon->count < 3 || polygon->count > AB_POLYGON_VERTICES ||
            !isfinite(polygon->radius) || polygon->radius < 0) {
            return false;
        }
        for (size_t vertex = 0; vertex < polygon->count; ++vertex) {
            if (!isfinite(polygon->vertices[vertex].x) ||
                !isfinite(polygon->vertices[vertex].y)) {
                return false;
            }
        }
        *bounds = ab_polygon_bounds(polygon, transform);
    } else {
        return false;
    }
    return ab_tree_valid_bounds(*bounds);
}

bool ab_fixture_proxy_create(AbFixtureProxy *proxy, const AbPhysicsShape *shape,
                             AbRigidTransform transform, AbBroadPhase *phase,
                             uintptr_t payload, AbError *error) {
    AbAabb bounds;
    if (!proxy || proxy->active ||
        !ab_fixture_shape_bounds(shape, transform, &bounds)) {
        return ab_fail(error, "Invalid fixture proxy creation");
    }
    int32_t id;
    if (!ab_broadphase_create(phase, bounds, payload, &id, error)) {
        return false;
    }
    *proxy = (AbFixtureProxy){bounds, id, true};
    return true;
}

bool ab_fixture_proxy_sync(AbFixtureProxy *proxy, const AbPhysicsShape *shape,
                           AbRigidTransform previous, AbRigidTransform current,
                           AbBroadPhase *phase, AbError *error) {
    if (!proxy) {
        return ab_fail(error, "Missing fixture proxy");
    }
    if (!proxy->active) {
        return true;
    }
    AbAabb previous_bounds, current_bounds;
    if (!ab_fixture_shape_bounds(shape, previous, &previous_bounds) ||
        !ab_fixture_shape_bounds(shape, current, &current_bounds)) {
        return ab_fail(error, "Invalid fixture synchronization bounds");
    }
    AbAabb combined = ab_tree_combine(previous_bounds, current_bounds);
    AbVec2 displacement = {current.position.x - previous.position.x,
                           current.position.y - previous.position.y};
    if (!ab_broadphase_move(phase, proxy->proxy, combined, displacement, error)) {
        return false;
    }
    proxy->bounds = combined;
    return true;
}

bool ab_fixture_proxy_destroy(AbFixtureProxy *proxy, AbBroadPhase *phase) {
    if (!proxy) {
        return false;
    }
    if (!proxy->active) {
        return true;
    }
    if (!ab_broadphase_destroy(phase, proxy->proxy)) {
        return false;
    }
    proxy->proxy = -1;
    proxy->active = false;
    return true;
}
