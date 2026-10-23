#include "ab/bird_hat.h"

#include "ab/ui_value_text.h"

#include <math.h>
#include <string.h>

static AbSpan span(const char *value) {
    return (AbSpan){(const uint8_t *)value, strlen(value)};
}

static bool valid_table(const AbDataTable *value) {
    return value && value->count <= value->capacity &&
           (!value->count || value->entries);
}

static AbDataValue field(const AbDataTable *table, const char *name) {
    if (!valid_table(table))
        return (AbDataValue){0};
    const AbDataValue *value =
        ab_data_get(table, (AbDataValue){.kind = AB_DATA_STRING, .string = span(name)});
    return value ? *value : (AbDataValue){0};
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL && (value.kind != AB_DATA_BOOLEAN || value.bits);
}

static bool table(AbDataValue value, const AbDataTable **output, AbError *error) {
    if (value.kind != AB_DATA_TABLE || !valid_table(value.table))
        return ab_fail(error, "Hat operation requires a valid source table");
    *output = value.table;
    return true;
}

static bool real(AbDataValue value, float *output, AbError *error) {
    if (value.kind != AB_DATA_NUMBER)
        return ab_fail(error, "Hat operation requires a numeric physical field");
    memcpy(output, &value.bits, sizeof(*output));
    return isfinite(*output) || ab_fail(error, "Nonfinite hat physical field");
}

static AbDataValue number(float value) {
    AbDataValue result = {.kind = AB_DATA_NUMBER};
    memcpy(&result.bits, &value, sizeof(value));
    return result;
}

static bool text(AbDataValue value, AbSpan *output, AbError *error) {
    if (value.kind != AB_DATA_STRING || !value.string.data || !value.string.size)
        return ab_fail(error, "Hat operation requires a source name");
    *output = value.string;
    return true;
}

static bool offset(const AbDataTable *definition, const AbDataTable *bird,
                   const AbDataTable **output, bool *present, AbError *error) {
    const AbDataTable *parameters = NULL;
    if (!table(field(definition, "params"), &parameters, error))
        return false;
    AbDataValue name = field(bird, "definition");
    if (name.kind == AB_DATA_STRING && name.string.size && !name.string.data)
        return ab_fail(error, "Invalid hat bird definition name");
    const AbDataValue *selected = ab_data_get(parameters, name);
    *present = selected && truthy(*selected);
    return !*present || table(*selected, output, error);
}

static bool generated_name(const char *prefix, AbDataValue counter, char storage[64],
                           AbSpan *output, AbError *error) {
    float finite = 0;
    AbSpan suffix = {0};
    uint8_t bytes[32];
    if (!real(counter, &finite, error) ||
        !ab_ui_value_text((AbUiPointerValue){.kind = AB_UI_POINTER_NUMBER,
                                             .number_bits = counter.bits},
                          bytes, sizeof(bytes), &suffix))
        return ab_fail(error, "Cannot format source hat counter");
    size_t length = strlen(prefix);
    if (suffix.size > 63 - length)
        return ab_fail(error, "Generated hat name exceeds bounded storage");
    memcpy(storage, prefix, length);
    memcpy(storage + length, suffix.data, suffix.size);
    storage[length + suffix.size] = 0;
    *output = (AbSpan){(const uint8_t *)storage, length + suffix.size};
    return true;
}

bool ab_bird_hat_create(const AbDataTable *hats, const AbDataTable *bird, AbSpan name,
                        AbDataValue *counter, AbBirdHat *record, bool *created,
                        AbBirdHatHost host, AbError *error) {
    if (!valid_table(hats) || !valid_table(bird) || !counter || !record || !created ||
        record->collision.object_name.size || (name.size && !name.data))
        return ab_fail(error, "Invalid hat creation owner/storage");
    *created = false;
    if (!truthy(*counter))
        *counter = number(1);
    const AbDataValue *selected =
        ab_data_get(hats, (AbDataValue){.kind = AB_DATA_STRING, .string = name});
    const AbDataTable *definition = NULL, *parameters = NULL;
    bool present = false;
    if (!selected || !table(*selected, &definition, error) ||
        !offset(definition, bird, &parameters, &present, error))
        return error && error->message[0]
                   ? false
                   : ab_fail(error, "Missing requested hat definition");
    if (!present)
        return true;
    AbSpan block = {0};
    AbVec2 position = {0}, delta = {0};
    if (!generated_name("generated_hat_", *counter, record->object_name,
                        &record->collision.object_name, error) ||
        !generated_name("generated_hat_joint_", *counter, record->joint_name,
                        &record->joint, error) ||
        !text(field(definition, "block"), &block, error) ||
        !real(field(bird, "x"), &position.x, error) ||
        !real(field(parameters, "x"), &delta.x, error) ||
        !real(field(bird, "y"), &position.y, error) ||
        !real(field(parameters, "y"), &delta.y, error))
        return false;
    position.x += delta.x;
    position.y += delta.y;
    if (!host.create_object || !host.set_static || !host.set_defence ||
        !host.create_joint || !host.publish)
        return ab_fail(error, "Missing actual hat construction services");
    AbSpan object = record->collision.object_name;
    if (!host.create_object(host.context, block, object, position, 0.001f, error) ||
        !host.set_static(host.context, object, error) ||
        !host.set_defence(host.context, object, 1000000, error))
        return false;
    AbSpan bird_name = {0};
    if (!text(field(bird, "name"), &bird_name, error) ||
        !host.create_joint(host.context, record->joint, bird_name, object, error))
        return false;
    /* The source rereads the descriptor and bird after creation callbacks. */
    if (!text(field(bird, "name"), &bird_name, error) ||
        !text(field(definition, "block"), &record->definition, error) ||
        !offset(definition, bird, &parameters, &present, error) || !present ||
        !real(field(parameters, "x"), &record->offset.x, error) ||
        !real(field(parameters, "y"), &record->offset.y, error) ||
        !host.publish(host.context, bird_name, record, error))
        return error && error->message[0]
                   ? false
                   : ab_fail(error, "Hat publication lost its current bird parameters");
    float current = 0;
    if (!real(*counter, &current, error))
        return false;
    *counter = number(current + 1);
    *created = true;
    return true;
}

bool ab_bird_hat_release(AbSpan bird, AbBirdHatHost host, AbError *error) {
    if ((bird.size && !bird.data) || !host.print_release || !host.hat)
        return ab_fail(error, "Missing actual hat release registry/log boundary");
    if (!host.print_release(host.context, error))
        return false;
    AbBirdHat *record = NULL;
    if (!host.hat(host.context, bird, &record, error))
        return false;
    if (!record || truthy(record->joint_destroyed))
        return true;
    if (!host.destroy_joint_deferred || !host.set_damping || !host.set_dynamic ||
        !host.object || !host.impulse)
        return ab_fail(error, "Missing actual hat release physics services");
    AbSpan object = record->collision.object_name;
    if (!host.destroy_joint_deferred(host.context, record->joint, error) ||
        !host.set_damping(host.context, object, 0.4f, error) ||
        !host.set_dynamic(host.context, object, error))
        return false;
    const AbDataTable *bird_object = NULL, *hat_object = NULL;
    float velocity = 0, height = 0;
    AbVec2 point = {0};
    if (!host.object(host.context, bird, &bird_object, error) ||
        !real(field(bird_object, "xVel"), &velocity, error) ||
        !host.object(host.context, object, &hat_object, error) ||
        !real(field(hat_object, "x"), &point.x, error) ||
        !real(field(hat_object, "y"), &point.y, error) ||
        !real(field(hat_object, "height"), &height, error))
        return false;
    point.y -= height;
    if (!host.impulse(host.context, object, (AbVec2){velocity * -1e-5f, -0.0003f},
                      point, error))
        return false;
    record->joint_destroyed = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1};
    return true;
}
