#include "ab/game_contact_world.h"

typedef struct {
    AbGameContactWorld *owner;
    AbGameContactWorldObject objects[2];
} ContactEvent;

static void fail_listener(AbGameContactWorld *owner, const AbError *error) {
    if (!owner->failed) {
        owner->error = *error;
        if (!owner->error.message[0]) {
            ab_fail(&owner->error, "Game contact listener failed");
        }
        owner->failed = true;
    }
    owner->world->failed = true;
}

static bool enter_callback(AbGameContactWorld *owner, const AbPhysicsWorld *world) {
    if (owner->failed || world->failed) {
        return false;
    }
    if (owner->world != world || owner->processing) {
        AbError error = {0};
        ab_fail(&error, "Game contact listener world mismatch or reentry");
        fail_listener(owner, &error);
        return false;
    }
    owner->processing = true;
    return true;
}

static bool basic_begin(ContactEvent *event, size_t index, AbError *error) {
    AbGameContactWorld *owner = event->owner;
    AbPhysicsWorld *world = owner->world;
    AbWorldSolveContact *contact = world->storage.contacts + index;
    AbWorldManifold manifold = {0};
    const AbGameContactObject *first = &event->objects[0].script;
    const AbGameContactObject *second = &event->objects[1].script;
    if (first->script_table_present && second->script_table_present &&
        !owner->config.begin_disabled) {
        if (!contact->state.manifold.point_count ||
            !ab_world_has_body(world, contact->first_body) ||
            !ab_world_has_body(world, contact->second_body)) {
            return ab_fail(error, "Unsupported basic contact geometry or body");
        }
        ab_world_manifold(
            &manifold, &contact->state.manifold,
            world->storage.bodies[contact->first_body].motion.mass.transform,
            contact->first_radius,
            world->storage.bodies[contact->second_body].motion.mass.transform,
            contact->second_radius);
    }
    const AbGameContactInput input = {.first = first,
                                      .second = second,
                                      .manifold = &manifold,
                                      .contact = &contact->state,
                                      .glue = owner->config.glue,
                                      .callbacks_disabled =
                                          owner->config.begin_disabled};
    return ab_game_contact_begin(&input, owner->config.basic, error);
}

static bool selected_handler(void *context, const AbGameContactDispatchCall *call,
                             AbError *error) {
    ContactEvent *event = context;
    AbGameContactWorld *owner = event->owner;
    if (call->stage == AB_GAME_CONTACT_BEGIN &&
        call->handler == AB_GAME_CONTACT_BASIC_BEGIN) {
        return basic_begin(event, call->contact, error);
    }
    if (call->stage == AB_GAME_CONTACT_END &&
        call->handler == AB_GAME_CONTACT_BASIC_END) {
        return ab_game_contact_end(&event->objects[0].script, &event->objects[1].script,
                                   owner->config.end, owner->config.basic.context,
                                   error);
    }
    if (!owner->config.special) {
        return ab_fail(error, "Missing specialized game contact handler");
    }
    return owner->config.special(owner->config.basic.context, call, error);
}

static bool dispatch_event(AbGameContactWorld *owner, AbGameContactStage stage,
                           size_t index, const AbManifold *previous, AbError *error) {
    if (stage == AB_GAME_CONTACT_BEGIN && owner->config.begin_disabled) {
        return true;
    }
    AbPhysicsWorld *world = owner->world;
    if (index >= world->contact_count || !world->storage.contacts ||
        !world->storage.pairs) {
        return ab_fail(error, "Invalid game contact slot");
    }
    AbFixturePair pair = world->storage.pairs[index];
    if (!ab_world_has_fixture(world, pair.first) ||
        !ab_world_has_fixture(world, pair.second)) {
        return ab_fail(error, "Invalid game contact fixtures");
    }
    uintptr_t first = world->storage.fixtures[pair.first].user_data;
    uintptr_t second = world->storage.fixtures[pair.second].user_data;
    if (!first || !second) {
        return true;
    }
    ContactEvent event = {.owner = owner};
    if (!owner->config.object(owner->config.object_context, first, event.objects,
                              error) ||
        !owner->config.object(owner->config.object_context, second, event.objects + 1,
                              error)) {
        return false;
    }
    const AbGameContactDispatchInput input = {.stage = stage,
                                              .first = &event.objects[0].collision,
                                              .second = &event.objects[1].collision,
                                              .contact = index,
                                              .previous = previous,
                                              .begin_disabled =
                                                  owner->config.begin_disabled};
    return ab_game_contact_dispatch(owner->config.routes, &input, selected_handler,
                                    &event, error);
}

static void event_callback(void *context, AbPhysicsWorld *world, size_t index,
                           AbGameContactStage stage, const AbManifold *previous) {
    AbGameContactWorld *owner = context;
    if (!enter_callback(owner, world)) {
        return;
    }
    AbError error = {0};
    bool okay = dispatch_event(owner, stage, index, previous, &error);
    owner->processing = false;
    if (!okay) {
        fail_listener(owner, &error);
    }
}

static void begin_contact(void *context, AbPhysicsWorld *world, size_t index) {
    event_callback(context, world, index, AB_GAME_CONTACT_BEGIN, NULL);
}

static void end_contact(void *context, AbPhysicsWorld *world, size_t index) {
    event_callback(context, world, index, AB_GAME_CONTACT_END, NULL);
}

static void pre_solve(void *context, AbPhysicsWorld *world, size_t index,
                      const AbManifold *previous) {
    event_callback(context, world, index, AB_GAME_CONTACT_PRE_SOLVE, previous);
}

static bool filter(void *context, const AbPhysicsWorld *world, size_t first,
                   size_t second) {
    AbGameContactWorld *owner = context;
    if (!enter_callback(owner, world)) {
        return false;
    }
    bool allowed =
        owner->previous.filter(owner->previous.context, world, first, second);
    owner->processing = false;
    return allowed;
}

static void post_solve(void *context, AbPhysicsWorld *world, size_t index,
                       const AbContactImpulse *impulse) {
    AbGameContactWorld *owner = context;
    if (!enter_callback(owner, world)) {
        return;
    }
    owner->previous.post_solve(owner->previous.context, world, index, impulse);
    owner->processing = false;
}

static void destroy_fixture(void *context, AbPhysicsWorld *world, size_t fixture) {
    AbGameContactWorld *owner = context;
    if (!enter_callback(owner, world)) {
        return;
    }
    owner->previous.destroy_fixture(owner->previous.context, world, fixture);
    owner->processing = false;
}

static void destroy_joint(void *context, AbPhysicsWorld *world, size_t joint) {
    AbGameContactWorld *owner = context;
    if (!enter_callback(owner, world)) {
        return;
    }
    owner->previous.destroy_joint(owner->previous.context, world, joint);
    owner->processing = false;
}

bool ab_game_contact_world_open(AbPhysicsWorld *world, AbGameContactWorldConfig config,
                                AbGameContactWorld *output, AbError *error) {
    if (!world || world->failed || world->locked || !world->broadphase.tree.nodes ||
        !output || !config.routes || !config.object || world->listener.begin ||
        world->listener.end || world->listener.pre_solve) {
        return ab_fail(error, "Invalid or occupied game contact world");
    }
    *output = (AbGameContactWorld){
        .world = world, .config = config, .previous = world->listener};
    world->listener = (AbWorldListener){
        .context = output,
        .filter = output->previous.filter ? filter : NULL,
        .begin = begin_contact,
        .end = end_contact,
        .pre_solve = pre_solve,
        .post_solve = output->previous.post_solve ? post_solve : NULL,
        .destroy_fixture = output->previous.destroy_fixture ? destroy_fixture : NULL,
        .destroy_joint = output->previous.destroy_joint ? destroy_joint : NULL};
    return true;
}

bool ab_game_contact_world_close(AbGameContactWorld *owner, AbError *error) {
    if (!owner || !owner->world) {
        return true;
    }
    if (owner->processing || owner->world->locked ||
        owner->world->listener.context != owner) {
        return ab_fail(error, "Cannot close an active or replaced contact listener");
    }
    owner->world->listener = owner->previous;
    *owner = (AbGameContactWorld){0};
    return true;
}
