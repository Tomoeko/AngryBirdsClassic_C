#include "ab/game_circle.h"

#include <math.h>
#include <string.h>

static AbDataValue text(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)value, strlen(value)}};
}

static AbDataValue number(float value) {
    AbDataValue result = {.kind = AB_DATA_NUMBER};
    memcpy(&result.bits, &value, sizeof(value));
    return result;
}

bool ab_game_circle_create(AbPhysicsWorld *world, AbGameCircleInput input,
                           uintptr_t fixture_user_data, AbGameCircle *output,
                           AbError *error) {
    if (!output || !input.name.data || !input.name.size ||
        input.name.size > UINT32_MAX || !isfinite(input.position.x) ||
        !isfinite(input.position.y) || !isfinite(input.radius) || input.radius < 0 ||
        !isfinite(input.density) || input.density < 0 || !isfinite(input.friction) ||
        input.friction < 0 || !isfinite(input.restitution) || input.restitution < 0 ||
        !isfinite(input.z_order))
        return ab_fail(error, "Invalid native circle construction input");

    AbObjectBody physics;
    if (!ab_object_body_create_circle(world, input.position, input.radius,
                                      input.density, input.friction, input.restitution,
                                      fixture_user_data, &physics, error))
        return false;
    AbGameFrameObject frame;
    if (!ab_game_frame_object_reset(world, physics.body, 0, &frame, error))
        return false;

    *output = (AbGameCircle){.physics = physics, .frame = frame};
    output->physics.controllable = input.controllable;
    output->initial_mass = world->storage.bodies[physics.body].motion.mass.mass;
    AbLevelPropertyObject *properties = &output->properties;
    AbDataEntry fields[] = {
        {text("name"), {.kind = AB_DATA_STRING, .string = input.name}},
        {text("sprite"), text("")},
        {text("type"), text("circle")},
        {text("x"), number(input.position.x)},
        {text("y"), number(input.position.y)},
        {text("width"), number(-1)},
        {text("height"), number(-1)},
        {text("radius"), number(input.radius)},
        {text("angle"), number(0)},
        {text("density"), number(input.density)},
        {text("friction"), number(input.friction)},
        {text("restitution"), number(input.restitution)},
        {text("mass"), number(output->initial_mass)},
        {text("xVel"), number(0)},
        {text("yVel"), number(0)},
        {text("z_order"), number(input.z_order)},
    };
    memcpy(properties->physical_entries, fields, sizeof(fields));
    properties->physical =
        (AbDataTable){properties->physical_entries, sizeof(fields) / sizeof(*fields),
                      AB_LEVEL_PHYSICAL_PROPERTY_CAPACITY, NULL};
    return true;
}
