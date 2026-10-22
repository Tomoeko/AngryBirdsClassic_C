#ifndef AB_MUSEUM_BIRD_BEHAVIOR_H
#define AB_MUSEUM_BIRD_BEHAVIOR_H

#include "ab/lua_data.h"
#include "ab/blues_trajectory_behavior.h"

typedef enum {
    AB_MUSEUM_BIRD_TRAJECTORY,
    AB_MUSEUM_BIRD_CAMERA,
    AB_MUSEUM_BIRD_BLUES,
    AB_MUSEUM_BIRD_BLUES_CAMERA,
} AbMuseumBirdBehaviorKind;

typedef struct {
    AbMuseumBirdBehaviorKind kind;
    AbDataTable fields;
    AbDataEntry entries[8];
} AbMuseumBirdBehavior;

typedef enum {
    AB_MUSEUM_TRAJECTORY_ADD,
    AB_MUSEUM_TRAJECTORY_REMOVE,
    AB_MUSEUM_TRAJECTORY_KEEP,
    AB_MUSEUM_CAMERA_SEQUENCE,
    AB_MUSEUM_CAMERA_CANCEL,
} AbMuseumBirdOperation;

typedef struct {
    AbMuseumBirdOperation operation;
    AbDataValue object;
    /* KEEP represents the original fresh one-slot keep list. A nil object
     * yields an empty list; any other value is its sole element. Table
     * allocation/identity is not exposed here. */
    const char *sequence;
} AbMuseumBirdCall;

typedef struct {
    void *context;
    AbDataValue flying_bird;
    bool (*call)(void *context, const AbMuseumBirdCall *call, AbError *error);
    bool (*get_property)(void *context, AbDataValue game_object, AbSpan key,
                         AbDataValue *output, AbError *error);
    AbBluesTrajectoryHost *blues;
} AbMuseumBirdHost;

/* Initial BehaviorBase.new projection: name, gameObject and freshly read
 * gameObject.physicsObject. Legacy trajectory/camera classes have empty
 * parameters. Blues additionally prepares its authored isCluster parameter,
 * defaulting falsy values to false. Its init phase must run separately below.
 * Accept absent, false or supported plain authored overrides; other parameters
 * reject explicitly.
 * The receiver must be a plain script table. No VM, class loading or allocation.
 * Own four legacy or eight Blues entry slots; borrowed names/tables stay alive until
 * the last view is released. Output must remain at its original address and must not
 * alias input tables/entries. Failure preserves output. */
bool ab_museum_bird_behavior_initialize(AbSpan name, AbDataValue game_object,
                                        AbDataValue overrides,
                                        AbMuseumBirdBehavior *output, AbError *error);

/* Complete the original init phase. Legacy classes have no hook; Blues children
 * add their actual physical object to follow before setting flying=true. Blues
 * camera initialization cancels, then freshly calls the shot sequence service.
 * Its inherited event methods are empty. */
bool ab_museum_bird_behavior_init(AbMuseumBirdBehavior *behavior,
                                  AbMuseumBirdHost *host, AbError *error);

/* Actual legacy methods; trajectory onLaunch writes flying=true after ADD.
 * Collision's other parameter is the original second physical-object argument;
 * its controllable/name fields are raw table reads. Destroy does not clear the
 * flying field. Specialty keeps only the current non-RedBird trajectory; camera
 * specialty reads specialty before a fresh launchVelocity and starts the exact
 * boomerang resource only for negative X. Launch arguments are unused by both
 * recovered methods. KEEP passes one physical object, as described above.
 *
 * Hosts/fields are read at their native phases. Callbacks may change later host
 * bindings and ordinary fields, retaining prior effects on later failure. Keep
 * all borrowed objects alive, do not move/reinitialize this instance, and do not
 * reenter the same event. Ordinary plain data only; metatables/coercion and the
 * actual trajectory renderer, sequence loader and registry remain external.
 */
bool ab_museum_bird_behavior_launch(AbMuseumBirdBehavior *behavior,
                                    AbMuseumBirdHost *host, AbError *error);
bool ab_museum_bird_behavior_collision(AbMuseumBirdBehavior *behavior,
                                       AbDataValue other, AbMuseumBirdHost *host,
                                       AbError *error);
bool ab_museum_bird_behavior_destroy(AbMuseumBirdBehavior *behavior,
                                     AbMuseumBirdHost *host, AbError *error);
bool ab_museum_bird_behavior_specialty(AbMuseumBirdBehavior *behavior,
                                       AbMuseumBirdHost *host, AbError *error);

#endif
