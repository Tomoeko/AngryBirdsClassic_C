#include "ab/object_definition.h"

#include <string.h>

static AbDataValue number(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return (AbDataValue){.kind = AB_DATA_NUMBER, .bits = bits};
}

static bool named(AbSpan field, const char *name) {
    size_t length = strlen(name);
    return field.size == length && (!length || memcmp(field.data, name, length) == 0);
}

static AbDataValue lookup(const AbDataTable *table, AbSpan key) {
    if (!table || (key.size && !key.data)) {
        return (AbDataValue){0};
    }
    const AbDataValue *value =
        ab_data_get(table, (AbDataValue){.kind = AB_DATA_STRING, .string = key});
    return value ? *value : (AbDataValue){0};
}

static AbDataValue field(const AbDataTable *table, const char *name) {
    return lookup(table, (AbSpan){(const uint8_t *)name, strlen(name)});
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL &&
           (value.kind != AB_DATA_BOOLEAN || value.bits != 0);
}

static bool numeric_equal(AbDataValue first, AbDataValue second) {
    if (first.kind != AB_DATA_NUMBER || second.kind != AB_DATA_NUMBER) {
        return false;
    }
    float left, right;
    memcpy(&left, &first.bits, sizeof(left));
    memcpy(&right, &second.bits, sizeof(right));
    return left == right;
}

static bool material_property(AbSpan name) {
    /* Legacy damageSound also writes a separate global during creation; it
     * must not become an ordinary material fallback in this immutable view. */
    static const char *const fields[] = {
        "density",
        "friction",
        "restitution",
        "controllable",
        "strength",
        "defence",
        "forceX",
        "forceY",
        "levelGoal",
        "z_order",
        "animation",
        "collisionSound",
        "underwaterCollisionSound",
        "destroyedSound",
        "underwaterDestroyedSound",
        "rollingSound",
        "portalDelay",
        "portalMinSpeedOut",
        "passThrough",
        "passThroughDelay",
        "passThroughParticles",
    };
    for (size_t index = 0; index < sizeof(fields) / sizeof(*fields); ++index) {
        if (named(name, fields[index])) {
            return true;
        }
    }
    return false;
}

AbDataValue ab_object_property(const AbObjectDefinition *definition, AbSpan name) {
    if (!definition || !definition->source || (name.size && !name.data)) {
        return (AbDataValue){0};
    }
    if (named(name, "collider")) {
        return definition->collider;
    }
    if (named(name, "z_order")) {
        return definition->z_order;
    }
    AbDataValue value = lookup(definition->source, name);
    if (named(name, "density")) {
        AbDataValue override = lookup(definition->overrides, name);
        if (truthy(override)) {
            value = override;
        }
    }
    if (value.kind == AB_DATA_NIL && material_property(name)) {
        value = lookup(definition->material, name);
    }
    return value;
}

static AbDataValue draw_order(const AbObjectDefinition *definition,
                              const AbDataTable *colliders) {
    AbDataValue value = field(definition->overrides, "z_order");
    if (!truthy(value)) {
        value = field(definition->source, "z_order");
    }
    if (value.kind == AB_DATA_NIL) {
        value = field(definition->material, "z_order");
    }
    if (value.kind != AB_DATA_NIL) {
        return value;
    }
    if (numeric_equal(definition->collider, field(colliders, "portal"))) {
        return number(6);
    }
    if (numeric_equal(definition->collider, field(colliders, "bird")) ||
        numeric_equal(definition->collider, field(colliders, "pig"))) {
        return number(4);
    }
    value = ab_object_property(definition, (AbSpan){(const uint8_t *)"density", 7});
    return number(numeric_equal(value, number(0)) ? 0 : 2);
}

bool ab_object_definition_open(const AbDataTable *catalog, AbSpan name,
                               const AbDataTable *overrides, AbObjectDefinition *output,
                               AbError *error) {
    if (!catalog || !output || !name.data || !name.size) {
        return ab_fail(error, "Invalid object-definition request");
    }
    AbDataValue blocks = field(catalog, "blocks");
    AbDataValue materials = field(catalog, "materials");
    AbDataValue colliders = field(catalog, "collider_types");
    if (blocks.kind != AB_DATA_TABLE || !blocks.table ||
        materials.kind != AB_DATA_TABLE || !materials.table ||
        colliders.kind != AB_DATA_TABLE || !colliders.table) {
        return ab_fail(error, "Object catalog needs blocks, materials and colliders");
    }
    AbDataValue source = lookup(blocks.table, name);
    if (source.kind != AB_DATA_TABLE || !source.table) {
        return ab_fail(error, "Unknown object definition: %.*s",
                       name.size > 96 ? 96 : (int)name.size, name.data);
    }
    AbDataValue material_name = field(source.table, "materialName");
    const AbDataValue *material = ab_data_get(materials.table, material_name);
    AbObjectDefinition definition = {
        .source = source.table,
        .material =
            material && material->kind == AB_DATA_TABLE ? material->table : NULL,
        .overrides = overrides,
        .collider = field(source.table, "collider"),
    };
    if (definition.collider.kind == AB_DATA_NIL) {
        definition.collider = field(definition.material, "collider");
    }
    if (definition.collider.kind == AB_DATA_NIL) {
        definition.collider = number(0);
    }
    definition.z_order = draw_order(&definition, colliders.table);
    *output = definition;
    return true;
}
