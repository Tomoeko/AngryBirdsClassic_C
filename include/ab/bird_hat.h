#ifndef AB_BIRD_HAT_H
#define AB_BIRD_HAT_H

#include "ab/bird_collision_prelude.h"

typedef struct {
    /* First member: existing collision/input registries borrow this view. */
    AbBirdCollisionHat collision;
    AbSpan joint, definition;
    AbVec2 offset;
    AbDataValue joint_destroyed;
    char object_name[64], joint_name[64];
} AbBirdHat;

typedef struct {
    void *context;
    bool (*create_object)(void *context, AbSpan definition, AbSpan name,
                          AbVec2 position, float density, AbError *error);
    bool (*set_static)(void *context, AbSpan name, AbError *error);
    bool (*set_defence)(void *context, AbSpan name, float defence, AbError *error);
    /* createHat's type=2, coordType=2, zero-anchor joint, collideConnected=false. */
    bool (*create_joint)(void *context, AbSpan name, AbSpan bird, AbSpan hat,
                         AbError *error);
    bool (*publish)(void *context, AbSpan bird, AbBirdHat *hat, AbError *error);
    bool (*print_release)(void *context, AbError *error);
    bool (*hat)(void *context, AbSpan bird, AbBirdHat **output, AbError *error);
    bool (*destroy_joint_deferred)(void *context, AbSpan name, AbError *error);
    bool (*set_damping)(void *context, AbSpan name, float damping, AbError *error);
    bool (*set_dynamic)(void *context, AbSpan name, AbError *error);
    bool (*object)(void *context, AbSpan name, const AbDataTable **output,
                   AbError *error);
    bool (*impulse)(void *context, AbSpan name, AbVec2 impulse, AbVec2 point,
                    AbError *error);
} AbBirdHatHost;

/* createHat97169. Borrow the actual blockTable.hats and bird physical table.
 * Initialize nil/false counter before querying the bird's offset; an absent
 * offset succeeds without creating a hat. Names live in the caller's unused,
 * zero-initialized stable record until session disposal. Counter is the shared source
 * scalar, read again after publication; finite numeric counters use the target's text
 * conversion. Numeric-string/coercion/metatable behavior is outside this API.
 * Host owns real object construction, static transition, physical defence,
 * joint creation and registry publication, in source order. No allocation.
 * Failure retains preceding counter/record/host effects; discard the session. */
bool ab_bird_hat_create(const AbDataTable *hats, const AbDataTable *bird, AbSpan name,
                        AbDataValue *counter, AbBirdHat *record, bool *created,
                        AbBirdHatHost host, AbError *error);

/* releaseHat97988. Print before the actual registry lookup. Missing/already
 * released hats return after that lookup. Queue destruction, set damping and
 * dynamic type, then query current physical velocity/position/height for the
 * actual impulse. Mark jointDestroyed only after its successful application.
 * The source record remains registered. Host lookups borrow stable plain
 * physical tables. No immediate joint removal or invented retirement occurs. */
bool ab_bird_hat_release(AbSpan bird, AbBirdHatHost host, AbError *error);

#endif
