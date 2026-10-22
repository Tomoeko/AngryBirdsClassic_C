#ifndef AB_OBJECT_PHYSICS_H
#define AB_OBJECT_PHYSICS_H

#include "ab/object_definition.h"
#include "ab/physics_body.h"
#include "ab/physics_fixture.h"

typedef struct {
    float width;
    float height;
    float radius;
} AbObjectDimensions;

typedef struct {
    AbPhysicsShape shape;
    float density;
    float friction;
    float restitution;
    AbBodyType body_type;
    AbObjectDimensions dimensions;
} AbObjectPhysics;

/* Initial classic circle/box/convex-polygon fixture geometry and material. Boxes with
 * both explicit dimensions use physics units directly. Otherwise both dimensions come
 * from sprite bounds, multiply by physics_scale, then by per-axis factors (default
 * binary32 0.92; unit factors at zero density). Box skin is the native GameObject
 * radius 0.1. Circle radius uses an explicit definition value or
 * ((sprite width * 0.5) * physics_scale) * sizeFactorX (default binary32 0.92).
 * Circle factors still apply at zero density. Zero radius is valid, including
 * an absent sprite's zero width; signed zero follows the constructor arithmetic.
 * Sprite height is unused. This does not apply subsequent queue/sensor/angle
 * changes or construct a game object.
 * Dimensions retain native object metadata: circle width/height are -1; box
 * radius is -1 and width/height retain the full, unhalved constructor inputs.
 * Polygons use normalized vertex coordinates times resolved box dimensions,
 * then subtract half the corresponding full size. Preserve that arithmetic
 * order. Their metadata dimensions remain all -1, independent of fixture bounds.
 * Support three through eight ordered vertices forming a finite strictly
 * convex polygon before native hull welding. Concave/collinear polygons,
 * larger source polygons and fixture decomposition remain open.
 * Inputs remain borrowed; invalid/unsupported types or arithmetic preserve
 * output. Sprite bounds and physics_scale are validated only when used.
 */
bool ab_object_physics(const AbObjectDefinition *definition, AbVec2 sprite_size,
                       float physics_scale, AbObjectPhysics *output, AbError *error);

#endif
