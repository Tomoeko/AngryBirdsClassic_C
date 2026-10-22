#ifndef AB_PHYSICS_WORLD_H
#define AB_PHYSICS_WORLD_H

#include "ab/physics_filter.h"
#include "ab/physics_fixture.h"
#include "ab/physics_joint_parameters.h"
#include "ab/physics_world_solve.h"

typedef struct {
    AbPhysicsShape shape;
    AbFixtureProxy proxy;
    size_t body;
    float density;
    float friction;
    float restitution;
    AbCollisionFilter filter;
    bool sensor;
    uintptr_t user_data;
} AbWorldFixture;

typedef struct {
    size_t first;
    size_t second;
} AbFixturePair;

typedef struct AbPhysicsWorld AbPhysicsWorld;

typedef struct {
    void *context;
    bool (*filter)(void *context, const AbPhysicsWorld *world, size_t first_fixture,
                   size_t second_fixture);
    void (*begin)(void *context, AbPhysicsWorld *world, size_t contact);
    void (*end)(void *context, AbPhysicsWorld *world, size_t contact);
    void (*pre_solve)(void *context, AbPhysicsWorld *world, size_t contact,
                      const AbManifold *previous);
    void (*post_solve)(void *context, AbPhysicsWorld *world, size_t contact,
                       const AbContactImpulse *impulse);
    void (*destroy_fixture)(void *context, AbPhysicsWorld *world, size_t fixture);
    void (*destroy_joint)(void *context, AbPhysicsWorld *world, size_t joint);
} AbWorldListener;

typedef struct {
    AbWorldSolveBody *bodies;
    size_t body_count;
    AbWorldFixture *fixtures;
    size_t fixture_count;
    AbWorldSolveContact *contacts;
    AbFixturePair *pairs;
    size_t contact_capacity;
    AbFixtureMass *mass_scratch;
    size_t mass_capacity;
    AbWorldSolveWorkspace *solve;
    size_t body_capacity;
    size_t fixture_capacity;
    AbWorldSolveJoint *joints;
    size_t joint_count;
    size_t joint_capacity;
} AbWorldStorage;

typedef struct {
    AbVec2 gravity;
    AbBroadLimits broadphase_limits;
    float maximum_translation;
    float maximum_translation_squared;
    bool allow_sleep;
    bool warm_start;
    bool clear_forces;
    bool continuous;
} AbWorldDefinition;

struct AbPhysicsWorld {
    AbWorldStorage storage;
    AbBroadPhase broadphase;
    AbWorldDefinition definition;
    AbWorldListener listener;
    size_t *body_order;
    size_t *fixture_order;
    bool *body_alive;
    bool *fixture_alive;
    size_t body_count;
    size_t fixture_count;
    size_t body_slot_count;
    size_t fixture_slot_count;
    size_t contact_count;
    float inverse_previous_step;
    bool new_fixtures;
    bool locked;
    bool failed;
    size_t *joint_order;
    bool *joint_alive;
    size_t joint_count;
    size_t joint_slot_count;
};

/* Circle/polygon world over supplied body/fixture storage.
 * Bodies are initialized without fixtures and ordered newest first. Fixtures
 * are ordered by creation, oldest first, and refer to body array indices.
 * Storage is borrowed and disjoint. The world owns its broadphase and slot/list
 * metadata. Keep all arrays alive until close. Initialization
 * attaches fixtures in order, resets mass after each positive-density addition,
 * and creates active proxies. It can fail after changing body/fixture state;
 * close releases its owned storage on every path. Open starts with empty output.
 * Optional capacities default to the initial counts. Reserve larger capacities
 * for creation after open; solve/mass scratch must cover those capacities.
 * body_order is newest first, fixture_order oldest first. Their live lengths
 * are world.body_count and world.fixture_count; slot counts include removed
 * holes. Initial storage counts retain their supplied values. Slots are stable
 * until removal and may be reused afterward. Joints are initially newest first;
 * joint_order and joint_count track that live order separately from stable
 * slots. Initial joint solver state/island flags are reset to constructor state.
 * Prismatic definitions supply axes prepared by ab_prismatic_axes_prepare.
 * Substepping is not implemented yet.
 */
bool ab_world_open(AbPhysicsWorld *world, AbWorldDefinition definition,
                   AbWorldStorage storage, AbError *error);
void ab_world_close(AbPhysicsWorld *world);
bool ab_world_has_body(const AbPhysicsWorld *world, size_t body);
bool ab_world_has_fixture(const AbPhysicsWorld *world, size_t fixture);
bool ab_world_has_joint(const AbPhysicsWorld *world, size_t joint);
/* Structural changes require an unlocked, healthy world. Definitions are
 * copied; fixture.proxy is ignored. Creation returns a slot only on success.
 * Body removal destroys its joints, contacts, then fixtures. Joint/fixture
 * destruction visits each body's lists newest first.
 * destroy_fixture is called only for implicit destruction with a body, before
 * removing each fixture's proxies. Explicit fixture removal resets body mass.
 * Capacity validation rejects before mutation; later failures are sticky.
 */
bool ab_world_create_body(AbPhysicsWorld *world, const AbBodyDefinition *definition,
                          size_t *body, AbError *error);
bool ab_world_destroy_body(AbPhysicsWorld *world, size_t body, AbError *error);
bool ab_world_create_fixture(AbPhysicsWorld *world, const AbWorldFixture *definition,
                             size_t *fixture, AbError *error);
bool ab_world_destroy_fixture(AbPhysicsWorld *world, size_t fixture, AbError *error);
/* Joint creation copies endpoints, definition and metadata, clears retained
 * solver state, and inserts newest first without waking bodies. Removal wakes
 * sleeping endpoints and preserves sleep time on endpoints already awake.
 * Disabling connected collisions flags existing pair contacts for filtering;
 * creation/removal do not touch proxies to discover absent contacts.
 * destroy_joint runs only for implicit body removal, before unlinking the joint.
 * Destruction callbacks keep all structural lists stable until returning.
 */
bool ab_world_create_joint(AbPhysicsWorld *world, const AbWorldSolveJoint *definition,
                           size_t *joint, AbError *error);
bool ab_world_destroy_joint(AbPhysicsWorld *world, size_t joint, AbError *error);
/* Revolute/prismatic motor speed assignment is permitted during a locked step.
 * Wake sleeping endpoints even for an unchanged speed; preserve sleep time on
 * endpoints already awake. Retained impulses, limit state and flags stay intact.
 * Invalid/nonfinite arguments reject before mutation.
 */
bool ab_world_set_joint_motor_speed(AbPhysicsWorld *world, size_t joint, float speed,
                                    AbError *error);

/* Assign selected parameters in native game order: motor enable, speed, maximum
 * torque/force, limit enable, lower bound, upper bound. Motor assignments always
 * wake endpoints, even for identical values. Changed limit enable/bounds wake
 * endpoints and clear only the retained limit impulse; equal bounds (including
 * opposite zero signs) keep their existing bits. Distance frequency/damping
 * assignments neither wake endpoints nor clear solver state. Locked worlds are
 * supported. Only the kind's supported mask is accepted; selected numbers must
 * be finite and motor maximum nonnegative. Invalid inputs reject before writes.
 */
bool ab_world_set_joint_parameters(AbPhysicsWorld *world, size_t joint,
                                   const AbJointParameters *parameters, AbError *error);
/* Listener changes contact/material/force state while keeping array structure
 * and body type/active/transform invariants intact. Contact indices are temporary
 * and can change when contacts are discovered or removed. A custom filter
 * replaces the default fixture filter; body eligibility is always enforced.
 * Contact-capacity or numeric failure can follow partial work and makes the
 * world failed until close/reinitialization. Each successful positive step
 * discovers, refreshes, solves, synchronizes, and rediscovers contacts in target
 * order. Zero steps still refresh contacts and honor force clearing.
 */
bool ab_world_step(AbPhysicsWorld *world, float seconds, size_t velocity_iterations,
                   size_t position_iterations, AbError *error);
/* Clear live bodies' accumulated force and torque to positive zero without
 * waking them. This is permitted during a locked step. Removed slots and all
 * other body state are preserved; invalid storage rejects before clearing. */
bool ab_world_clear_forces(AbPhysicsWorld *world, AbError *error);
bool ab_world_apply_force(AbPhysicsWorld *world, size_t body, AbVec2 force,
                          float torque);

/* Body changes require an unlocked, healthy world. Repositioning synchronizes
 * each fixture at the new transform; activation creates/removes proxies and
 * deactivation removes contacts in body-list order. Type changes reset mass,
 * clear forces, remove contacts, and touch remaining proxies. Rotation changes
 * reset angular velocity and fixture mass. Capacity/numeric failures can follow
 * partial work and make the world failed. Caller indices remain stable.
 */
bool ab_world_set_transform(AbPhysicsWorld *world, size_t body, AbVec2 position,
                            float angle, AbError *error);
bool ab_world_set_active(AbPhysicsWorld *world, size_t body, bool active,
                         AbError *error);
bool ab_world_set_body_type(AbPhysicsWorld *world, size_t body, AbBodyType type,
                            AbError *error);
bool ab_world_set_fixed_rotation(AbPhysicsWorld *world, size_t body, bool fixed,
                                 AbError *error);
/* Filter changes flag existing contacts and touch active proxies. Sensor
 * changes wake the owning body without replacing contact material/state.
 * These two operations may run inside contact callbacks.
 */
bool ab_world_set_fixture_filter(AbPhysicsWorld *world, size_t fixture,
                                 AbCollisionFilter filter, AbError *error);
bool ab_world_set_fixture_sensor(AbPhysicsWorld *world, size_t fixture, bool sensor);

#endif
