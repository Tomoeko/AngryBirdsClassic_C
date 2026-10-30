#include "ab/object_body.h"

#include <math.h>
#include <string.h>

static AbDataValue property(const AbObjectDefinition *definition, const char *name) {
    return ab_object_property(definition,
                              (AbSpan){(const uint8_t *)name, strlen(name)});
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL &&
           (value.kind != AB_DATA_BOOLEAN || value.bits != 0);
}

static bool override_angle(const AbObjectDefinition *definition, float *angle,
                           bool *present) {
    const AbDataValue *value = ab_data_get(
        definition->overrides,
        (AbDataValue){.kind = AB_DATA_STRING, .string = {(const uint8_t *)"angle", 5}});
    *present = value && truthy(*value);
    if (!*present) {
        return true;
    }
    if (value->kind != AB_DATA_NUMBER) {
        return false;
    }
    float input;
    memcpy(&input, &value->bits, sizeof(input));
    return ab_game_angle_wrap(input, angle);
}

static bool create_body(AbPhysicsWorld *world, const AbObjectPhysics *physics,
                        AbVec2 position, uintptr_t fixture_user_data,
                        AbObjectBody *result, AbError *error) {
    if (!world || world->failed || world->locked || !world->broadphase.tree.nodes ||
        !isfinite(position.x) || !isfinite(position.y) ||
        world->body_count >= world->storage.body_capacity ||
        world->fixture_count >= world->storage.fixture_capacity) {
        return ab_fail(error, "Invalid or full classic object world");
    }
    AbBodyDefinition body = {.type = physics->body_type,
                             .position = position,
                             .angular_damping = 1,
                             .gravity_scale = 1,
                             .sleep_allowed = true,
                             .awake = true,
                             .active = true};
    AbWorldFixture fixture = {.shape = physics->shape,
                              .density = physics->density,
                              .friction = physics->friction,
                              .restitution = physics->restitution,
                              .filter = {1, 0xffff, 0},
                              .user_data = fixture_user_data};
    if (!ab_world_create_body(world, &body, &result->body, error)) {
        return false;
    }
    fixture.body = result->body;
    return ab_world_create_fixture(world, &fixture, &result->fixture, error);
}

bool ab_object_body_create(AbPhysicsWorld *world, const AbObjectDefinition *definition,
                           AbVec2 position, AbVec2 sprite_size, float physics_scale,
                           uintptr_t fixture_user_data, AbObjectBody *output,
                           AbError *error) {
    AbObjectPhysics physics;
    float angle = 0;
    bool rotate = false;
    if (!output) {
        return ab_fail(error, "Missing classic object body output");
    }
    if (!ab_object_physics(definition, sprite_size, physics_scale, &physics, error)) {
        return false;
    }
    if (!override_angle(definition, &angle, &rotate)) {
        return ab_fail(error, "Invalid classic object body angle");
    }
    AbDataValue collision = property(definition, "collision");
    AbDataValue gravity = property(definition, "gravityEnabled");
    AbDataValue controllable = property(definition, "controllable");
    AbObjectBody result = {
        .dimensions = physics.dimensions,
        .collider = definition->collider,
        .collisions_enabled = physics.shape.kind == AB_SHAPE_CIRCLE ||
                              collision.kind == AB_DATA_NIL || truthy(collision),
        .gravity_enabled = gravity.kind == AB_DATA_NIL || truthy(gravity),
        .controllable = controllable.kind == AB_DATA_BOOLEAN && controllable.bits != 0,
    };
    if (!create_body(world, &physics, position, fixture_user_data, &result, error) ||
        !ab_world_set_fixture_sensor(world, result.fixture,
                                     truthy(property(definition, "sensor"))) ||
        (rotate &&
         !ab_world_set_transform(world, result.body, position, angle, error))) {
        return false;
    }
    *output = result;
    return true;
}

bool ab_object_body_create_circle(AbPhysicsWorld *world, AbVec2 position, float radius,
                                  float density, float friction, float restitution,
                                  uintptr_t fixture_user_data, AbObjectBody *output,
                                  AbError *error) {
    if (!output || !isfinite(radius) || radius < 0 || !isfinite(density) ||
        density < 0 || !isfinite(friction) || friction < 0 || !isfinite(restitution) ||
        restitution < 0)
        return ab_fail(error, "Invalid direct circle body inputs");
    AbObjectPhysics physics = {
        .shape = {.kind = AB_SHAPE_CIRCLE, .data.circle = {.radius = radius}},
        .density = density,
        .friction = friction,
        .restitution = restitution,
        .body_type = density == 0 ? AB_BODY_STATIC : AB_BODY_DYNAMIC,
        .dimensions = {-1, -1, radius}};
    AbObjectBody result = {.collider = {.kind = AB_DATA_NUMBER},
                           .collisions_enabled = true,
                           .gravity_enabled = true,
                           .dimensions = physics.dimensions};
    if (!create_body(world, &physics, position, fixture_user_data, &result, error))
        return false;
    *output = result;
    return true;
}

bool ab_ground_body_create(AbPhysicsWorld *world, AbVec2 position,
                           uintptr_t fixture_user_data, AbObjectBody *output,
                           AbError *error) {
    if (!output) {
        return ab_fail(error, "Missing ground body output");
    }
    AbObjectPhysics physics = {
        .shape = {.kind = AB_SHAPE_POLYGON,
                  .data.polygon = ab_polygon_box(1000, 5, 0x1.99999ap-4f)},
        .body_type = AB_BODY_STATIC,
        .friction = 0x1.99999ap-1f,
    };
    AbObjectBody result = {.collider = {.kind = AB_DATA_NUMBER},
                           .collisions_enabled = true,
                           .gravity_enabled = true,
                           .dimensions = {2000, 10, -1}};
    if (!create_body(world, &physics, position, fixture_user_data, &result, error)) {
        return false;
    }
    *output = result;
    return true;
}

bool ab_object_body_resize_circle(AbPhysicsWorld *world, AbObjectBody *object,
                                  float radius, AbError *error) {
    if (!world || !object || world->failed || world->locked || !isfinite(radius) ||
        radius < 0 || !ab_world_has_body(world, object->body) ||
        !ab_world_has_fixture(world, object->fixture))
        return ab_fail(error, "Invalid circle fixture recreation input");
    AbWorldFixture fixture = world->storage.fixtures[object->fixture];
    if (fixture.body != object->body || fixture.shape.kind != AB_SHAPE_CIRCLE)
        return ab_fail(error, "Circle recreation requires its actual fixture");
    for (size_t index = 0; index < world->fixture_count; ++index) {
        size_t other = world->fixture_order[index];
        if (other != object->fixture &&
            world->storage.fixtures[other].body == object->body)
            return ab_fail(error, "Circle recreation requires a single fixture");
    }
    /* ARM GameObject.recreate3448b4 destroys the fixture, not the body.
     * createFixture34458c restores default filter/sensor fields. */
    fixture.shape.data.circle = (AbCircle){.radius = radius};
    fixture.filter = (AbCollisionFilter){1, 0xffff, 0};
    fixture.sensor = false;
    object->dimensions.radius = radius;
    return ab_world_destroy_fixture(world, object->fixture, error) &&
           ab_world_create_fixture(world, &fixture, &object->fixture, error);
}

bool ab_object_body_queue(AbPhysicsWorld *world, const AbObjectBody *object,
                          AbError *error) {
    if (!object || !ab_world_has_body(world, object->body)) {
        return ab_fail(error, "Invalid queued object body");
    }
    AbVec2 position =
        world->storage.bodies[object->body].motion.mass.transform.position;
    return ab_world_set_transform(world, object->body, position, 0, error) &&
           ab_world_set_body_type(world, object->body, AB_BODY_STATIC, error);
}
