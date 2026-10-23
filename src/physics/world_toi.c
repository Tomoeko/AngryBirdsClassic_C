#include "world_internal.h"

#include "ab/physics_impact.h"
#include "internal.h"
#include "step_internal.h"

#include <float.h>
#include <math.h>

enum { IMPACT_BODY_LIMIT = 64, IMPACT_CONTACT_LIMIT = 32 };

static AbSweep read_sweep(const AbBodyMotion *body) {
    const AbBodyMassState *mass = &body->mass;
    return (AbSweep){mass->local_center,   mass->previous_center, mass->center,
                     mass->previous_angle, mass->angle,           mass->sweep_alpha};
}

static void write_sweep(AbBodyMotion *body, AbSweep sweep) {
    body->mass.local_center = sweep.local_center;
    body->mass.previous_center = sweep.previous_center;
    body->mass.center = sweep.center;
    body->mass.previous_angle = sweep.previous_angle;
    body->mass.angle = sweep.angle;
    body->mass.sweep_alpha = sweep.alpha;
}

static bool advance_body(AbBodyMotion *body, float alpha) {
    AbSweep sweep = read_sweep(body);
    if (!ab_sweep_advance(&sweep, alpha)) {
        return false;
    }
    sweep.center = sweep.previous_center;
    sweep.angle = sweep.previous_angle;
    write_sweep(body, sweep);
    return ab_step_sync_body(body);
}

static bool moving_body(const AbBodyMotion *body) {
    return body->awake && body->mass.type != AB_BODY_STATIC;
}

static bool continuous_pair(const AbBodyMotion *first, const AbBodyMotion *second) {
    return first->bullet || second->bullet || first->mass.type != AB_BODY_DYNAMIC ||
           second->mass.type != AB_BODY_DYNAMIC;
}

static bool contact_fraction(AbPhysicsWorld *world, size_t index, float *fraction) {
    AbWorldSolveContact *contact = world->storage.contacts + index;
    *fraction = 1;
    if (!(contact->state.flags & AB_CONTACT_ENABLED) || contact->impact_count > 8) {
        return true;
    }
    if (contact->state.flags & 32) {
        *fraction = contact->impact_fraction;
        return true;
    }
    AbBodyMotion *first = &world->storage.bodies[contact->first_body].motion;
    AbBodyMotion *second = &world->storage.bodies[contact->second_body].motion;
    AbFixturePair pair = world->storage.pairs[index];
    const AbWorldFixture *first_fixture = world->storage.fixtures + pair.first;
    const AbWorldFixture *second_fixture = world->storage.fixtures + pair.second;
    if (first_fixture->sensor || second_fixture->sensor ||
        (!moving_body(first) && !moving_body(second)) ||
        !continuous_pair(first, second)) {
        return true;
    }
    AbSweep first_sweep = read_sweep(first), second_sweep = read_sweep(second);
    float alpha = first_sweep.alpha;
    if (first_sweep.alpha < second_sweep.alpha) {
        alpha = second_sweep.alpha;
        if (!ab_sweep_advance(&first_sweep, alpha)) {
            return false;
        }
        write_sweep(first, first_sweep);
    } else if (second_sweep.alpha < first_sweep.alpha) {
        if (!ab_sweep_advance(&second_sweep, alpha)) {
            return false;
        }
        write_sweep(second, second_sweep);
    }
    AbImpactInput input = {ab_world_shape_proxy(&first_fixture->shape),
                           ab_world_shape_proxy(&second_fixture->shape), first_sweep,
                           second_sweep, 1};
    AbImpactResult impact;
    if (!ab_time_of_impact(&input, &impact)) {
        return false;
    }
    if (impact.kind == AB_IMPACT_TOUCHING) {
        *fraction = fminf(alpha + (1.0f - alpha) * impact.fraction, 1.0f);
    }
    contact->impact_fraction = *fraction;
    contact->state.flags |= 32;
    return true;
}

static void select_body(AbPhysicsWorld *world, AbWorldIslandSelection *selection,
                        size_t index) {
    AbWorldSolveBody *body = world->storage.bodies + index;
    body->island = true;
    body->island_index = selection->body_count;
    world->storage.solve->body_order[selection->body_count++] = index;
}

static void select_contact(AbPhysicsWorld *world, AbWorldIslandSelection *selection,
                           size_t index) {
    world->storage.contacts[index].state.flags |= 1;
    world->storage.solve->contact_order[selection->contact_count++] = index;
}

static bool include_neighbors(AbPhysicsWorld *world, AbWorldIslandSelection *selection,
                              size_t seed, float alpha, AbError *error) {
    AbBodyMotion *body = &world->storage.bodies[seed].motion;
    if (body->mass.type != AB_BODY_DYNAMIC) {
        return true;
    }
    for (size_t index = 0; index < world->contact_count; ++index) {
        if (selection->body_count == IMPACT_BODY_LIMIT ||
            selection->contact_count == IMPACT_CONTACT_LIMIT) {
            break;
        }
        AbWorldSolveContact *contact = world->storage.contacts + index;
        if (contact->state.flags & 1) {
            continue;
        }
        size_t other_index;
        if (contact->first_body == seed) {
            other_index = contact->second_body;
        } else if (contact->second_body == seed) {
            other_index = contact->first_body;
        } else {
            continue;
        }
        AbWorldSolveBody *other = world->storage.bodies + other_index;
        AbFixturePair pair = world->storage.pairs[index];
        if (!continuous_pair(body, &other->motion) ||
            world->storage.fixtures[pair.first].sensor ||
            world->storage.fixtures[pair.second].sensor) {
            continue;
        }
        AbSweep backup = read_sweep(&other->motion);
        if ((!other->island && !advance_body(&other->motion, alpha)) ||
            !ab_world_update_contact(world, index, error)) {
            return false;
        }
        if ((contact->state.flags & 6) != 6) {
            write_sweep(&other->motion, backup);
            if (!ab_step_sync_body(&other->motion)) {
                return false;
            }
            continue;
        }
        select_contact(world, selection, index);
        if (!other->island) {
            select_body(world, selection, other_index);
            if (other->motion.mass.type != AB_BODY_STATIC) {
                ab_world_wake_body(&other->motion);
            }
        }
    }
    return true;
}

static bool finish_island(AbPhysicsWorld *world, AbWorldIslandSelection selection,
                          AbError *error) {
    for (size_t selected = 0; selected < selection.body_count; ++selected) {
        size_t index = world->storage.solve->body_order[selected];
        AbWorldSolveBody *body = world->storage.bodies + index;
        body->island = false;
        if (body->motion.mass.type != AB_BODY_DYNAMIC) {
            continue;
        }
        if (!ab_world_synchronize_body(world, index, error)) {
            return false;
        }
        for (size_t contact = 0; contact < world->contact_count; ++contact) {
            AbWorldSolveContact *current = world->storage.contacts + contact;
            if (current->first_body == index || current->second_body == index) {
                current->state.flags &= ~UINT32_C(33);
            }
        }
    }
    return ab_world_find_contacts(world, error);
}

static bool solve_impact(AbPhysicsWorld *world, size_t index, float alpha,
                         const AbIslandStep *step, AbError *error) {
    AbWorldSolveContact *contact = world->storage.contacts + index;
    size_t first_index = contact->first_body, second_index = contact->second_body;
    AbBodyMotion *first = &world->storage.bodies[first_index].motion;
    AbBodyMotion *second = &world->storage.bodies[second_index].motion;
    AbSweep first_backup = read_sweep(first), second_backup = read_sweep(second);
    if (!advance_body(first, alpha) || !advance_body(second, alpha) ||
        !ab_world_update_contact(world, index, error)) {
        return false;
    }
    contact->state.flags &= ~UINT32_C(32);
    ++contact->impact_count;
    if ((contact->state.flags & 6) != 6) {
        contact->state.flags &= ~UINT32_C(4);
        write_sweep(first, first_backup);
        write_sweep(second, second_backup);
        return ab_step_sync_body(first) && ab_step_sync_body(second);
    }
    ab_world_wake_body(first);
    ab_world_wake_body(second);
    AbWorldIslandSelection selection = {
        .continuous = true, .first_impact_body = 0, .second_impact_body = 1};
    select_body(world, &selection, first_index);
    select_body(world, &selection, second_index);
    select_contact(world, &selection, index);
    if (!include_neighbors(world, &selection, first_index, alpha, error) ||
        !include_neighbors(world, &selection, second_index, alpha, error)) {
        return false;
    }
    AbIslandStep substep = *step;
    substep.motion.seconds = (1.0f - alpha) * step->motion.seconds;
    substep.motion.position_iterations = 20;
    substep.time_ratio = 1;
    substep.warm_start = false;
    if (!ab_world_solve_selected(world->storage.bodies, world->storage.contacts, NULL,
                                 selection, &substep, world->storage.solve)) {
        return false;
    }
    return finish_island(world, selection, error);
}

bool ab_world_solve_continuous(AbPhysicsWorld *world, const AbIslandStep *step,
                               AbError *error) {
    for (size_t cursor = 0; cursor < world->body_count; ++cursor) {
        size_t index = world->body_order[cursor];
        world->storage.bodies[index].island = false;
        world->storage.bodies[index].motion.mass.sweep_alpha = 0;
    }
    for (size_t index = 0; index < world->contact_count; ++index) {
        AbWorldSolveContact *contact = world->storage.contacts + index;
        contact->state.flags &= ~UINT32_C(33);
        contact->impact_count = 0;
        contact->impact_fraction = 1;
    }
    for (;;) {
        size_t earliest = SIZE_MAX;
        float alpha = 1;
        for (size_t index = 0; index < world->contact_count; ++index) {
            float fraction;
            if (!contact_fraction(world, index, &fraction)) {
                return ab_fail(error, "Continuous world impact query failed");
            }
            if (fraction < alpha) {
                alpha = fraction;
                earliest = index;
            }
        }
        if (earliest == SIZE_MAX || alpha > 1.0f - 10.0f * FLT_EPSILON) {
            return true;
        }
        if (!solve_impact(world, earliest, alpha, step, error)) {
            if (error && error->message[0])
                return false;
            return ab_fail(error, "Continuous world island solve failed");
        }
    }
}
