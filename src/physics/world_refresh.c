#include "world_internal.h"

#include "ab/physics_distance.h"

typedef struct {
    AbPhysicsWorld *world;
    size_t contact;
    bool failed;
} ContactContext;

AbDistanceProxy ab_world_shape_proxy(const AbPhysicsShape *shape) {
    if (shape->kind == AB_SHAPE_CIRCLE) {
        return (AbDistanceProxy){&shape->data.circle.center, 1,
                                 shape->data.circle.radius};
    }
    return (AbDistanceProxy){shape->data.polygon.vertices, shape->data.polygon.count,
                             shape->data.polygon.radius};
}

static bool evaluate_overlap(void *context) {
    ContactContext *contact = context;
    AbPhysicsWorld *world = contact->world;
    AbFixturePair pair = world->storage.pairs[contact->contact];
    const AbWorldFixture *first = world->storage.fixtures + pair.first;
    const AbWorldFixture *second = world->storage.fixtures + pair.second;
    bool overlap = false;
    contact->failed = !ab_shape_overlap(
        ab_world_shape_proxy(&first->shape),
        world->storage.bodies[first->body].motion.mass.transform,
        ab_world_shape_proxy(&second->shape),
        world->storage.bodies[second->body].motion.mass.transform, &overlap);
    return overlap;
}

static void evaluate_manifold(void *context, AbManifold *output) {
    ContactContext *contact = context;
    AbPhysicsWorld *world = contact->world;
    AbFixturePair pair = world->storage.pairs[contact->contact];
    const AbWorldFixture *first = world->storage.fixtures + pair.first;
    const AbWorldFixture *second = world->storage.fixtures + pair.second;
    AbRigidTransform first_transform =
        world->storage.bodies[first->body].motion.mass.transform;
    AbRigidTransform second_transform =
        world->storage.bodies[second->body].motion.mass.transform;
    if (first->shape.kind == AB_SHAPE_CIRCLE && second->shape.kind == AB_SHAPE_CIRCLE) {
        ab_collide_circles(output, first->shape.data.circle, first_transform,
                           second->shape.data.circle, second_transform);
    } else if (first->shape.kind == AB_SHAPE_POLYGON &&
               second->shape.kind == AB_SHAPE_CIRCLE) {
        ab_collide_polygon_circle(output, &first->shape.data.polygon, first_transform,
                                  second->shape.data.circle, second_transform);
    } else {
        ab_collide_polygons(output, &first->shape.data.polygon, first_transform,
                            &second->shape.data.polygon, second_transform);
    }
}

static void begin_contact(void *context, AbContactState *state) {
    (void)state;
    ContactContext *contact = context;
    AbPhysicsWorld *world = contact->world;
    if (world->listener.begin) {
        world->listener.begin(world->listener.context, world, contact->contact);
    }
}

static void end_contact(void *context, AbContactState *state) {
    (void)state;
    ContactContext *contact = context;
    AbPhysicsWorld *world = contact->world;
    if (world->listener.end) {
        world->listener.end(world->listener.context, world, contact->contact);
    }
}

static void pre_solve_contact(void *context, AbContactState *state,
                              const AbManifold *previous) {
    (void)state;
    ContactContext *contact = context;
    AbPhysicsWorld *world = contact->world;
    if (world->listener.pre_solve) {
        world->listener.pre_solve(world->listener.context, world, contact->contact,
                                  previous);
    }
}

static bool bounds_overlap(AbAabb first, AbAabb second) {
    return second.lower.x - first.upper.x <= 0 && second.lower.y - first.upper.y <= 0 &&
           first.lower.x - second.upper.x <= 0 && first.lower.y - second.upper.y <= 0;
}

bool ab_world_update_contact(AbPhysicsWorld *world, size_t index, AbError *error) {
    AbWorldSolveContact *contact = world->storage.contacts + index;
    AbFixturePair pair = world->storage.pairs[index];
    contact->sensor = world->storage.fixtures[pair.first].sensor ||
                      world->storage.fixtures[pair.second].sensor;
    ContactContext context = {world, index, false};
    AbContactEvaluator evaluator = {&context, evaluate_manifold, evaluate_overlap};
    AbContactListener listener = {&context, begin_contact, end_contact,
                                  pre_solve_contact};
    if (!ab_contact_state_update(&contact->state,
                                 &world->storage.bodies[contact->first_body].motion,
                                 &world->storage.bodies[contact->second_body].motion,
                                 contact->sensor, &evaluator, &listener) ||
        context.failed || world->failed) {
        world->failed = true;
        return ab_fail(error, "World contact evaluation failed");
    }
    return true;
}

bool ab_world_refresh_contacts(AbPhysicsWorld *world, AbError *error) {
    size_t index = 0;
    while (index < world->contact_count) {
        if (world->failed) {
            return ab_fail(error, "World contact callback failed");
        }
        AbWorldSolveContact *contact = world->storage.contacts + index;
        AbFixturePair pair = world->storage.pairs[index];
        if (contact->state.flags & 8) {
            if (!ab_world_pair_allowed(world, pair.first, pair.second)) {
                ab_world_remove_contact(world, index);
                continue;
            }
            contact->state.flags &= ~UINT32_C(8);
        }
        AbBodyMotion *first = &world->storage.bodies[contact->first_body].motion;
        AbBodyMotion *second = &world->storage.bodies[contact->second_body].motion;
        if (!(first->awake && first->mass.type != AB_BODY_STATIC) &&
            !(second->awake && second->mass.type != AB_BODY_STATIC)) {
            ++index;
            continue;
        }
        const AbWorldFixture *first_fixture = world->storage.fixtures + pair.first;
        const AbWorldFixture *second_fixture = world->storage.fixtures + pair.second;
        AbAabb first_bounds =
            world->broadphase.tree.nodes[first_fixture->proxy.proxy].bounds;
        AbAabb second_bounds =
            world->broadphase.tree.nodes[second_fixture->proxy.proxy].bounds;
        if (!bounds_overlap(first_bounds, second_bounds)) {
            ab_world_remove_contact(world, index);
            continue;
        }
        if (!ab_world_update_contact(world, index, error)) {
            return false;
        }
        ++index;
    }
    return !world->failed || ab_fail(error, "World contact callback failed");
}
