#ifndef AB_BLOCK_COLLISION_H
#define AB_BLOCK_COLLISION_H

#include "ab/audio_play.h"
#include "ab/damage_sprite.h"
#include "ab/physics_geometry.h"

typedef struct AbBlockCollisionObject AbBlockCollisionObject;
typedef struct AbBlockCollisionNotice AbBlockCollisionNotice;

typedef struct {
    void *context;
    /* Bound gameObjectScript:onBlockCollision method. */
    bool (*collision)(void *context, const AbBlockCollisionNotice *notice,
                      AbError *error);
} AbBlockCollisionScript;

struct AbBlockCollisionObject {
    AbSpan name;
    AbDataValue sensor, damage_sprite, blink_sprite, smile_sprite;
    AbBlockCollisionScript *script;
    void *context;
};

typedef struct {
    AbDataValue on_collision;
    void *context;
} AbBlockCollisionDefinition;

struct AbBlockCollisionNotice {
    AbBlockCollisionObject *first, *second;
    AbSpan other_name;
    float force, damage;
    AbVec2 normal, point;
    bool normal_present, point_present;
};

typedef struct {
    AbSpan first, second;
    float force, damage;
    AbVec2 normal, point;
} AbBlockCollisionInput;

typedef enum {
    AB_BLOCK_COLLISION_DAMAGE_SOUND,
    AB_BLOCK_COLLISION_CONTACT_SOUND,
} AbBlockCollisionSound;

typedef struct {
    float total;
    float *entries;
    size_t count, capacity;
} AbBlockCollisionScores;

typedef struct {
    void *context;
    /* Pure lookup of the current world table entry. Keep cached objects alive
     * through both recursive sensor dispatch and the enclosing call. */
    bool (*object)(void *context, AbSpan name, AbBlockCollisionObject **output,
                   AbError *error);
    bool (*level)(void *context, const AbBlockCollisionNotice *notice,
                  AbDataValue *result, AbError *error);
    bool (*definition)(void *context, AbSpan name, AbBlockCollisionDefinition **output,
                       AbError *error);
    /* Resolve the current named handler in the game environment. An absent
     * binding skips; a present binding receives the notice. */
    bool (*named)(void *context, AbDataValue name, const AbBlockCollisionNotice *notice,
                  AbError *error);
    bool (*scripts)(void *context, const AbBlockCollisionNotice *notice,
                    AbError *error);
    bool (*sprites)(void *context, AbBlockCollisionObject *object,
                    AbDamageSpriteNames *output, AbError *error);
    bool (*sound)(void *context, AbBlockCollisionSound kind,
                  AbBlockCollisionObject *object,
                  AbBlockCollisionDefinition *definition, AbDataValue *output,
                  AbError *error);
    bool (*audio_name)(void *context, AbDataValue sound, AbDataValue *output,
                       AbError *error);
    bool (*audio)(void *context, AbAudioPlay request, AbError *error);
    bool (*particles)(void *context, AbBlockCollisionObject *object, AbError *error);
    bool (*score)(void *context, float damage, float *output, AbError *error);
    AbBlockCollisionScores *scores;
    float sound_force_threshold;
} AbBlockCollisionHost;

/* Resolved CollisionCallbacks.blockCollision orchestration over borrowed typed
 * state. Both sensors skip; a second-only sensor dispatches a swapped recursive
 * call before the outer call continues. Each invocation runs level hook,
 * definition/named hooks, both block-event boundaries, then object scripts.
 * Second definition/block events negate the normal; object-script normals and
 * points retain the incoming values. A second sensor suppresses its object
 * script and final contact sound only.
 *
 * Positive damage refreshes both damage sprite states. Changed states update
 * damage_sprite/blink_sprite/smile_sprite before damage sound and particles. A truthy
 * level hook result suppresses only score addition/append. Force strictly above the
 * threshold requests first and eligible second contact sounds independently.
 * Sound resolution precedes the fixed volume .5, false-loop, channel-two audio
 * request. Required missing hosts fail when reached; no effects are fabricated.
 *
 * Bind named/script/resource hosts to first-party implementations. State and
 * returned definitions/names stay live and stable through the call. Hosts may
 * change later fields, bindings, score state and threshold in native order.
 * No allocation, Lua, body mutation or particle/audio rendering occurs here.
 * Finite used numeric state only; later failures retain all prior effects,
 * including score-total assignment before a bounded append failure. Serialize
 * calls and do not reenter from hosts. Object lookup itself must be pure.
 */
bool ab_block_collision_callback(const AbBlockCollisionInput *input,
                                 const AbBlockCollisionHost *host, AbError *error);

#endif
