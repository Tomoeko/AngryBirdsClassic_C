#ifndef AB_PHYSICS_FIXTURE_H
#define AB_PHYSICS_FIXTURE_H

#include "ab/physics_broadphase.h"

typedef enum {
    AB_SHAPE_CIRCLE = 0,
    AB_SHAPE_POLYGON = 2,
} AbShapeKind;

typedef struct {
    AbShapeKind kind;
    union {
        AbCircle circle;
        AbPolygon polygon;
    } data;
} AbPhysicsShape;

typedef struct {
    AbAabb bounds;
    int32_t proxy;
    bool active;
} AbFixtureProxy;

/* Circle and convex-polygon fixtures each have one child in the target.
 * Initialize the proxy to zero before first creation. The caller owns the shape
 * and phase and keeps them alive during operations. Shape/transform numeric
 * contracts apply; invalid/nonfinite bounds reject before insertion. Neither
 * output nor shape may overlap phase-owned storage. On failure the proxy is
 * unchanged; the phase may have grown auxiliary buffers.
 */
bool ab_fixture_proxy_create(AbFixtureProxy *proxy, const AbPhysicsShape *shape,
                             AbRigidTransform transform, AbBroadPhase *phase,
                             uintptr_t payload, AbError *error);
/* Combine both endpoint bounds; move prediction uses transform-origin delta,
 * independently of the body's local mass center. Inactive proxies are skipped.
 * Updating a contained proxy still updates its combined fixture bounds.
 */
bool ab_fixture_proxy_sync(AbFixtureProxy *proxy, const AbPhysicsShape *shape,
                           AbRigidTransform previous, AbRigidTransform current,
                           AbBroadPhase *phase, AbError *error);
bool ab_fixture_proxy_destroy(AbFixtureProxy *proxy, AbBroadPhase *phase);

#endif
