#include "ab/game_box.h"

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

bool ab_game_box_create(AbPhysicsWorld *world, AbGameBoxInput input,
                        uintptr_t fixture_user_data, AbGameBox *output,
                        AbError *error) {
    if (!output || !input.name.data || !input.name.size ||
        input.name.size > UINT32_MAX || !isfinite(input.position.x) ||
        !isfinite(input.position.y) || !isfinite(input.size.x) || input.size.x <= 0 ||
        !isfinite(input.size.y) || input.size.y <= 0 || !isfinite(input.density) ||
        input.density < 0 || !isfinite(input.friction) || input.friction < 0 ||
        !isfinite(input.restitution) || input.restitution < 0 ||
        !isfinite(input.z_order))
        return ab_fail(error, "Invalid native box construction input");
    AbDataEntry entries[] = {
        {text("type"), text("box")},
        {text("width"), number(input.size.x)},
        {text("height"), number(input.size.y)},
        {text("density"), number(input.density)},
        {text("friction"), number(input.friction)},
        {text("restitution"), number(input.restitution)},
        {text("collision"), {.kind = AB_DATA_BOOLEAN, .bits = input.collision}},
    };
    AbDataTable table = {entries, sizeof(entries) / sizeof(*entries),
                         sizeof(entries) / sizeof(*entries), NULL};
    AbObjectDefinition definition = {.source = &table,
                                     .collider = {.kind = AB_DATA_NUMBER}};
    AbObjectBody physics;
    if (!ab_object_body_create(world, &definition, input.position, (AbVec2){0}, 1,
                               fixture_user_data, &physics, error))
        return false;
    AbGameFrameObject frame;
    if (!ab_game_frame_object_reset(world, physics.body, 0, &frame, error))
        return false;
    *output = (AbGameBox){.physics = physics, .frame = frame};
    output->initial_mass = world->storage.bodies[physics.body].motion.mass.mass;
    AbLevelPropertyObject *properties = &output->properties;
    AbDataEntry fields[] = {
        {text("name"), {.kind = AB_DATA_STRING, .string = input.name}},
        {text("sprite"), text("")},
        {text("type"), text("box")},
        {text("x"), number(input.position.x)},
        {text("y"), number(input.position.y)},
        {text("width"), number(input.size.x)},
        {text("height"), number(input.size.y)},
        {text("radius"), number(-1)},
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
