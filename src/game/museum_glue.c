#include "ab/museum_glue.h"

static bool valid_name(AbSpan name) {
    return !name.size || name.data;
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL &&
           (value.kind != AB_DATA_BOOLEAN || value.bits != 0);
}

bool ab_museum_glue_contact(AbMuseumGlueObject *glue, AbMuseumGlueObject *other,
                            AbMuseumGlueHost host, AbMuseumGlueResult *output,
                            AbError *error) {
    if (!glue || !other || !output) {
        return ab_fail(error, "Invalid museum glue objects or output");
    }
    AbMuseumGlueResult result = {0};
    if (truthy(other->grabber)) {
        *output = result;
        return true;
    }
    AbDataValue collider;
    if (!valid_name(other->name) || !host.collider) {
        return ab_fail(error, "Invalid museum glue definition lookup");
    }
    if (!host.collider(host.context, other->name, &collider, error)) {
        return false;
    }
    bool bird =
        collider.kind == AB_DATA_NUMBER && collider.bits == UINT32_C(0x40400000);
    bool egg = collider.kind == AB_DATA_NUMBER && collider.bits == UINT32_C(0x40c00000);
    if (!bird && !egg) {
        *output = result;
        return true;
    }
    AbMuseumGlueObject *plunger = NULL;
    if (!valid_name(glue->plunger_name) || !host.object) {
        return ab_fail(error, "Invalid museum glue plunger lookup");
    }
    if (!host.object(host.context, glue->plunger_name, &plunger, error)) {
        return false;
    }
    if (!plunger) {
        return ab_fail(error, "Missing museum glue plunger");
    }
    if (egg) {
        if (!valid_name(plunger->name) || !valid_name(other->name)) {
            return ab_fail(error, "Invalid museum egg/plunger name");
        }
        other->grabber = (AbDataValue){.kind = AB_DATA_STRING, .string = plunger->name};
        plunger->grabbed_egg =
            (AbDataValue){.kind = AB_DATA_STRING, .string = other->name};
    }
    if (!valid_name(other->name) || !host.velocity) {
        return ab_fail(error, "Invalid museum glue velocity request");
    }
    if (!host.velocity(host.context, other->name, (AbVec2){0, 0}, error)) {
        return false;
    }
    other->glued_to_block = (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = 1};
    if (!valid_name(plunger->name) || !valid_name(other->name)) {
        return ab_fail(error, "Invalid museum glue response names");
    }
    result = (AbMuseumGlueResult){true, true, plunger->name, other->name};
    *output = result;
    return true;
}

bool ab_museum_glue_after_damage(const AbMuseumGlueRoute *route, AbMuseumGlueHost host,
                                 AbMuseumGlueResult *response, AbError *error) {
    if (!route || !response) {
        return ab_fail(error, "Invalid post-damage glue route or response");
    }
    if (truthy(route->dead[0]) || truthy(route->dead[1])) {
        return true;
    }
    for (size_t first = 0; first < 2; ++first) {
        if (route->colliders[first].kind == AB_DATA_NUMBER &&
            route->colliders[first].bits == UINT32_C(0x41200000)) {
            return ab_museum_glue_contact(route->objects[first],
                                          route->objects[1u - first], host, response,
                                          error);
        }
    }
    return true;
}
