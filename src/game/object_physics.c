#include "ab/object_physics.h"

#include <math.h>
#include <string.h>

static AbDataValue property(const AbObjectDefinition *definition, const char *name) {
    return ab_object_property(definition,
                              (AbSpan){(const uint8_t *)name, strlen(name)});
}

static bool finite_number(AbDataValue value, float *number) {
    if (value.kind != AB_DATA_NUMBER) {
        return false;
    }
    memcpy(number, &value.bits, sizeof(*number));
    return isfinite(*number);
}

static bool truthy(AbDataValue value) {
    return value.kind != AB_DATA_NIL &&
           (value.kind != AB_DATA_BOOLEAN || value.bits != 0);
}

static bool named(AbDataValue value, const char *name) {
    size_t size = strlen(name);
    return value.kind == AB_DATA_STRING && value.string.size == size &&
           memcmp(value.string.data, name, size) == 0;
}

static bool box_size(const AbObjectDefinition *definition, float density, AbVec2 sprite,
                     float scale, AbVec2 *size) {
    AbDataValue width = property(definition, "width");
    AbDataValue height = property(definition, "height");
    if (width.kind != AB_DATA_NIL && height.kind != AB_DATA_NIL) {
        return finite_number(width, &size->x) && finite_number(height, &size->y) &&
               size->x > 0 && size->y > 0;
    }
    if (!isfinite(scale) || scale <= 0 || !isfinite(sprite.x) || sprite.x <= 0 ||
        !isfinite(sprite.y) || sprite.y <= 0) {
        return false;
    }
    float horizontal = 0x1.d70a3ep-1f, vertical = horizontal;
    if (density == 0) {
        horizontal = 1;
        vertical = 1;
    } else {
        AbDataValue first = property(definition, "sizeFactorX");
        AbDataValue second = property(definition, "sizeFactorY");
        if ((truthy(first) && !finite_number(first, &horizontal)) ||
            (truthy(second) && !finite_number(second, &vertical))) {
            return false;
        }
    }
    size->x = (sprite.x * scale) * horizontal;
    size->y = (sprite.y * scale) * vertical;
    return isfinite(size->x) && isfinite(size->y) && size->x > 0 && size->y > 0;
}

static bool strictly_convex(const AbVec2 *points, size_t count) {
    bool positive = false, initialized = false;
    for (size_t edge = 0; edge < count; ++edge) {
        size_t next = (edge + 1) % count;
        AbVec2 delta = {points[next].x - points[edge].x,
                        points[next].y - points[edge].y};
        for (size_t vertex = 0; vertex < count; ++vertex) {
            if (vertex == edge || vertex == next) {
                continue;
            }
            AbVec2 offset = {points[vertex].x - points[edge].x,
                             points[vertex].y - points[edge].y};
            float side = delta.x * offset.y - delta.y * offset.x;
            if (!isfinite(side) || side == 0 ||
                (initialized && (side > 0) != positive)) {
                return false;
            }
            positive = side > 0;
            initialized = true;
        }
    }
    return initialized;
}

static bool convex_polygon_shape(const AbObjectDefinition *definition, AbVec2 size,
                                 AbPolygon *output) {
    AbDataValue vertices = property(definition, "vertices");
    if (vertices.kind != AB_DATA_TABLE || !vertices.table ||
        vertices.table->count < 3 || vertices.table->count > AB_POLYGON_VERTICES) {
        return false;
    }
    size_t count = vertices.table->count;
    AbVec2 points[AB_POLYGON_VERTICES];
    for (size_t index = 0; index < count; ++index) {
        float key = (float)(index + 1);
        AbDataValue position = {.kind = AB_DATA_NUMBER};
        memcpy(&position.bits, &key, sizeof(key));
        const AbDataValue *point = ab_data_get(vertices.table, position);
        if (!point || point->kind != AB_DATA_TABLE || !point->table) {
            return false;
        }
        AbObjectDefinition coordinates = {.source = point->table};
        float x, y;
        if (!finite_number(property(&coordinates, "x"), &x) ||
            !finite_number(property(&coordinates, "y"), &y)) {
            return false;
        }
        points[index].x = x * size.x - size.x * 0.5f;
        points[index].y = y * size.y - size.y * 0.5f;
    }
    return strictly_convex(points, count) &&
           ab_polygon_build(points, count, 0x1.99999ap-4f, output);
}

static bool circle_radius(const AbObjectDefinition *definition, AbVec2 sprite,
                          float scale, float *radius) {
    AbDataValue value = property(definition, "radius");
    if (value.kind != AB_DATA_NIL) {
        return finite_number(value, radius) && *radius >= 0;
    }
    if (!isfinite(sprite.x) || sprite.x < 0 || !isfinite(scale) || scale <= 0) {
        return false;
    }
    float factor = 0x1.d70a3ep-1f;
    AbDataValue size_factor = property(definition, "sizeFactorX");
    if (truthy(size_factor) && !finite_number(size_factor, &factor)) {
        return false;
    }
    *radius = ((sprite.x * 0.5f) * scale) * factor;
    return isfinite(*radius) && *radius >= 0;
}

bool ab_object_physics(const AbObjectDefinition *definition, AbVec2 sprite_size,
                       float physics_scale, AbObjectPhysics *output, AbError *error) {
    AbObjectPhysics result = {0};
    if (!definition || !definition->source || !output ||
        !finite_number(property(definition, "density"), &result.density) ||
        result.density < 0 ||
        !finite_number(property(definition, "friction"), &result.friction) ||
        result.friction < 0 ||
        !finite_number(property(definition, "restitution"), &result.restitution)) {
        return ab_fail(error, "Invalid classic object physics material");
    }
    AbDataValue type = property(definition, "type");
    if (named(type, "circle")) {
        float radius;
        if (!circle_radius(definition, sprite_size, physics_scale, &radius)) {
            return ab_fail(error, "Invalid classic object circle radius");
        }
        result.shape.kind = AB_SHAPE_CIRCLE;
        result.shape.data.circle = (AbCircle){radius, {0, 0}};
        result.dimensions = (AbObjectDimensions){-1, -1, radius};
    } else if (named(type, "box") || named(type, "polygon")) {
        AbVec2 size;
        if (!box_size(definition, result.density, sprite_size, physics_scale, &size)) {
            return ab_fail(error, "Invalid classic object box dimensions");
        }
        float half_width = size.x * 0.5f, half_height = size.y * 0.5f;
        if (half_width <= 0 || half_height <= 0) {
            return ab_fail(error, "Unsupported classic object box range");
        }
        result.shape.kind = AB_SHAPE_POLYGON;
        if (named(type, "polygon")) {
            if (!convex_polygon_shape(definition, size, &result.shape.data.polygon)) {
                return ab_fail(error, "Unsupported classic polygon geometry");
            }
            result.dimensions = (AbObjectDimensions){-1, -1, -1};
        } else {
            result.shape.data.polygon =
                ab_polygon_box(half_width, half_height, 0x1.99999ap-4f);
            result.dimensions = (AbObjectDimensions){size.x, size.y, -1};
        }
    } else {
        return ab_fail(error, "Unsupported classic object shape type");
    }
    result.body_type = result.density == 0 ? AB_BODY_STATIC : AB_BODY_DYNAMIC;
    *output = result;
    return true;
}
