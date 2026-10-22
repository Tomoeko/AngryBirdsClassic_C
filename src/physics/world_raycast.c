#include "ab/world_raycast.h"

typedef struct {
    const AbPhysicsWorld *world;
    AbWorldRaycastCallback callback;
    void *context;
} WorldRaycast;

typedef struct {
    AbWorldRaycastHit hit;
    bool found;
} NearestRaycast;

static bool fixture_raycast(void *context, int32_t proxy, uintptr_t payload, AbRay ray,
                            float *fraction, AbError *error) {
    WorldRaycast *query = context;
    const AbPhysicsWorld *world = query->world;
    if (!payload || !ab_world_has_fixture(world, payload - 1)) {
        return ab_fail(error, "World ray reached an invalid fixture");
    }
    size_t index = payload - 1;
    const AbWorldFixture *fixture = world->storage.fixtures + index;
    if (!fixture->proxy.active || fixture->proxy.proxy != proxy ||
        !ab_world_has_body(world, fixture->body)) {
        return ab_fail(error, "World ray fixture/proxy binding is invalid");
    }
    AbRigidTransform transform =
        world->storage.bodies[fixture->body].motion.mass.transform;
    AbRayHit result = {0};
    bool hit = false;
    if (fixture->shape.kind == AB_SHAPE_CIRCLE) {
        hit = ab_circle_raycast(fixture->shape.data.circle, transform, ray, &result);
    } else if (fixture->shape.kind == AB_SHAPE_POLYGON) {
        hit = ab_polygon_raycast(&fixture->shape.data.polygon, transform, ray, &result);
    } else {
        return ab_fail(error, "Unsupported world ray fixture shape");
    }
    if (!hit) {
        *fraction = ray.max_fraction;
        return true;
    }
    if (!query->callback) {
        return ab_fail(error, "Missing world ray hit callback");
    }
    float remaining = 1.0f - result.fraction;
    AbWorldRaycastHit event = {
        .fixture = index,
        .body = fixture->body,
        .point = {result.fraction * ray.second.x + remaining * ray.first.x,
                  result.fraction * ray.second.y + remaining * ray.first.y},
        .normal = result.normal,
        .fraction = result.fraction};
    return query->callback(query->context, &event, fraction, error);
}

bool ab_world_raycast(const AbPhysicsWorld *world, AbRay ray,
                      AbWorldRaycastCallback callback, void *context,
                      float *final_fraction, AbError *error) {
    if (!world || world->failed || !world->broadphase.tree.nodes) {
        return ab_fail(error, "Missing healthy world for raycast");
    }
    WorldRaycast query = {world, callback, context};
    return ab_tree_raycast(&world->broadphase.tree, ray, fixture_raycast, &query,
                           final_fraction, error);
}

static bool nearest(void *context, const AbWorldRaycastHit *hit, float *fraction,
                    AbError *error) {
    (void)error;
    NearestRaycast *query = context;
    query->hit = *hit;
    query->found = true;
    *fraction = hit->fraction;
    return true;
}

bool ab_world_raycast_nearest(const AbPhysicsWorld *world, AbRay ray,
                              AbWorldRaycastHit *output, bool *found, AbError *error) {
    if (!output || !found) {
        return ab_fail(error, "Missing nearest world ray outputs");
    }
    NearestRaycast query = {0};
    if (!ab_world_raycast(world, ray, nearest, &query, NULL, error)) {
        return false;
    }
    if (query.found) {
        *output = query.hit;
    }
    *found = query.found;
    return true;
}
