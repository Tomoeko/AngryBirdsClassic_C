#include "ab/game_contact.h"

#include <math.h>
#include <string.h>

static AbDataValue field(const AbDataTable *table, const char *name) {
    const AbDataValue *value = ab_data_get(
        table, (AbDataValue){.kind = AB_DATA_STRING,
                             .string = {(const uint8_t *)name, strlen(name)}});
    return value ? *value : (AbDataValue){0};
}

static bool finite_number(AbDataValue value, float *output) {
    if (value.kind != AB_DATA_NUMBER) {
        return false;
    }
    memcpy(output, &value.bits, sizeof(*output));
    return isfinite(*output);
}

static bool offer_velocity(AbDataValue response, AbGameContactHost host,
                           AbError *error) {
    if (response.kind != AB_DATA_TABLE) {
        return true;
    }
    const AbDataTable *table = response.table;
    if (!table || table->count > table->capacity || (table->count && !table->entries) ||
        !host.velocity) {
        return ab_fail(error, "Invalid contact velocity response or lookup");
    }
    AbDataValue name = field(table, "birdName");
    AbVec2 velocity;
    if (name.kind != AB_DATA_STRING || (name.string.size && !name.string.data) ||
        !finite_number(field(table, "afterCollisionVelocityX"), &velocity.x) ||
        !finite_number(field(table, "afterCollisionVelocityY"), &velocity.y)) {
        return ab_fail(error, "Contact velocity requires a string name and numbers");
    }
    AbDeferredVelocity *state = NULL;
    if (!host.velocity(host.context, name.string, &state, error)) {
        return false;
    }
    if (!state || !ab_collision_velocity_offer(state, velocity)) {
        return ab_fail(error, "Contact velocity object or pending value is invalid");
    }
    return true;
}

bool ab_game_contact_end(const AbGameContactObject *first,
                         const AbGameContactObject *second, AbGameContactEndHost host,
                         void *context, AbError *error) {
    if (!first || !second || !first->script_table_present ||
        !second->script_table_present) {
        return true;
    }
    if (!host || (first->name.size && !first->name.data) ||
        (second->name.size && !second->name.data)) {
        return ab_fail(error, "Invalid end-contact name or callback");
    }
    return host(context, first->name, second->name, error);
}

bool ab_game_contact_begin(const AbGameContactInput *input, AbGameContactHost host,
                           AbError *error) {
    if (!input) {
        return ab_fail(error, "Missing basic contact input");
    }
    if (input->callbacks_disabled || !input->first || !input->second ||
        !input->first->script_table_present || !input->second->script_table_present) {
        return true;
    }
    const AbWorldManifold *manifold = input->manifold;
    if (!input->contact || !host.collision || !manifold ||
        (input->first->name.size && !input->first->name.data) ||
        (input->second->name.size && !input->second->name.data) ||
        !isfinite(manifold->normal.x) || !isfinite(manifold->normal.y) ||
        !isfinite(manifold->points[0].x) || !isfinite(manifold->points[0].y)) {
        return ab_fail(error, "Invalid basic contact geometry or callback");
    }
    AbGameContactCall call = {.first = input->first->name,
                              .second = input->second->name,
                              .normal = manifold->normal,
                              .point = manifold->points[0]};
    if (!host.collision(host.context, &call, error)) {
        return false;
    }
    if (call.response.should_disable_contact) {
        input->contact->flags &= ~(uint32_t)AB_CONTACT_ENABLED;
    }
    if (!offer_velocity(call.response.new_collision_parameters, host, error)) {
        return false;
    }
    if (call.response.glue_contact_pair.kind == AB_DATA_TABLE &&
        !ab_classic_glue_queue_contact(input->glue, call.response.glue_contact_pair,
                                       host.body, host.context, error)) {
        return false;
    }
    return true;
}
