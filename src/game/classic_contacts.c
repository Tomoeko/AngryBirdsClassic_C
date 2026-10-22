#include "ab/classic_contacts.h"

#include <string.h>

static size_t named_object(const AbClassicContacts *contacts, AbSpan name) {
    for (size_t index = 0; index < contacts->count; ++index) {
        if (contacts->level.removed_objects[index])
            continue;
        AbSpan candidate = contacts->level.bodies->objects[index].name;
        if (candidate.size == name.size &&
            (!name.size || memcmp(candidate.data, name.data, name.size) == 0)) {
            return index;
        }
    }
    return SIZE_MAX;
}

static bool object_lookup(void *context, uintptr_t object,
                          AbGameContactWorldObject *output, AbError *error) {
    AbClassicContacts *contacts = context;
    for (size_t index = 0; index < contacts->count; ++index) {
        const AbLevelObject *entry = contacts->level.bodies->objects + index;
        if ((uintptr_t)entry == object && !contacts->level.removed_objects[index]) {
            *output = (AbGameContactWorldObject){
                .collision = {contacts->level.frame->objects[index].collider,
                              entry->physics.collisions_enabled},
                .script = {entry->name, contacts->objects[index].script_table_present}};
            return true;
        }
    }
    return ab_fail(error, "Contact fixture does not belong to its classic level");
}

static bool velocity_lookup(void *context, AbSpan name, AbDeferredVelocity **output,
                            AbError *error) {
    (void)error;
    AbClassicContacts *contacts = context;
    size_t index = named_object(contacts, name);
    *output = index == SIZE_MAX ? NULL : &contacts->objects[index].velocity;
    return true;
}

static bool body_lookup(void *context, AbSpan name, size_t *output, AbError *error) {
    (void)error;
    const AbClassicContacts *contacts = context;
    size_t index = named_object(contacts, name);
    size_t body = index == SIZE_MAX
                      ? SIZE_MAX
                      : contacts->level.bodies->objects[index].physics.body;
    *output = ab_world_has_body(contacts->level.world, body) ? body : SIZE_MAX;
    return true;
}

static bool collision(void *context, AbGameContactCall *call, AbError *error) {
    AbClassicContacts *contacts = context;
    if (!contacts->config.collision) {
        return ab_fail(error, "Missing classic collision callback");
    }
    return contacts->config.collision(contacts->config.context, call, error);
}

static bool ended(void *context, AbSpan first, AbSpan second, AbError *error) {
    AbClassicContacts *contacts = context;
    if (!contacts->config.end) {
        return ab_fail(error, "Missing classic end-contact callback");
    }
    return contacts->config.end(contacts->config.context, first, second, error);
}

static bool special(void *context, const AbGameContactDispatchCall *call,
                    AbError *error) {
    AbClassicContacts *contacts = context;
    if (!contacts->config.special) {
        return ab_fail(error, "Missing classic special contact handler");
    }
    return contacts->config.special(contacts->config.context, call, error);
}

static bool valid_object(AbClassicLevelView view, size_t index) {
    const AbLevelObject *entry = view.bodies->objects + index;
    return !view.removed_objects[index] && entry->name.size && entry->name.data &&
           ab_world_has_body(view.world, entry->physics.body) &&
           ab_world_has_fixture(view.world, entry->physics.fixture) &&
           view.frame->objects[index].body == entry->physics.body &&
           view.frame->objects[index].collider < AB_GAME_COLLIDER_TYPES &&
           view.world->storage.fixtures[entry->physics.fixture].body ==
               entry->physics.body &&
           view.world->storage.fixtures[entry->physics.fixture].user_data ==
               (uintptr_t)entry;
}

bool ab_classic_contacts_open(AbClassicLevel *level, AbClassicContactObject *objects,
                              size_t count, AbClassicContactsConfig config,
                              AbClassicContacts *output, AbError *error) {
    AbClassicLevelView view = ab_classic_level_view(level);
    size_t capacity = config.object_capacity ? config.object_capacity : count;
    if (!view.world || !view.bodies || !view.frame || view.world->failed ||
        view.world->locked || !output || !config.routes ||
        count != view.bodies->count || count != view.frame->count || capacity < count ||
        capacity > view.bodies->capacity || (capacity && !objects) ||
        view.world->listener.begin || view.world->listener.end ||
        view.world->listener.pre_solve) {
        return ab_fail(error, "Invalid classic contact bindings or occupied listener");
    }
    for (size_t index = 0; index < count; ++index) {
        if (!valid_object(view, index)) {
            return ab_fail(error, "Unsupported classic contact object binding");
        }
    }
    AbGameContactWorldConfig listener = {
        .routes = config.routes,
        .glue = config.glue,
        .object_context = output,
        .object = object_lookup,
        .basic = {output, collision, velocity_lookup, body_lookup},
        .end = ended,
        .special = special,
        .begin_disabled = config.begin_disabled};
    /* The listener stores this address, so initialize directly in caller storage.
     * Its preflight must complete before any of the remaining fields change. */
    if (!ab_game_contact_world_open(view.world, listener, &output->listener, error)) {
        return false;
    }
    output->level = view;
    output->objects = objects;
    output->count = count;
    output->capacity = capacity;
    output->config = config;
    return true;
}

bool ab_classic_contacts_register_object(AbClassicContacts *contacts, size_t object,
                                         AbError *error) {
    if (!contacts || !contacts->listener.world || contacts->listener.failed ||
        contacts->listener.processing || contacts->level.world->failed ||
        contacts->level.world->locked || object != contacts->count ||
        object >= contacts->capacity || object >= contacts->level.bodies->count ||
        object >= contacts->level.frame->count ||
        contacts->level.world->listener.context != &contacts->listener ||
        !valid_object(contacts->level, object))
        return ab_fail(error, "Invalid new classic contact object binding");
    ++contacts->count;
    return true;
}

bool ab_classic_contacts_close(AbClassicContacts *contacts, AbError *error) {
    if (!contacts) {
        return true;
    }
    if (!ab_game_contact_world_close(&contacts->listener, error)) {
        return false;
    }
    *contacts = (AbClassicContacts){0};
    return true;
}

bool ab_classic_contacts_play_mode_end(void *context, AbSpan first, AbSpan second,
                                       AbError *error) {
    (void)context;
    (void)first;
    (void)second;
    (void)error;
    /* PlayMode prototype 1437 contains only RETURN. */
    return true;
}

bool ab_classic_contacts_flush(AbClassicContacts *contacts, size_t object,
                               AbError *error) {
    if (!contacts || !contacts->listener.world || contacts->listener.failed ||
        contacts->listener.processing || contacts->level.world->locked ||
        contacts->level.world->failed || object >= contacts->count ||
        contacts->level.removed_objects[object]) {
        return ab_fail(error, "Invalid classic pending-velocity flush");
    }
    AbDeferredVelocity *pending = &contacts->objects[object].velocity;
    size_t body = contacts->level.bodies->objects[object].physics.body;
    if (pending->pending &&
        (!ab_world_has_body(contacts->level.world, body) ||
         !ab_collision_velocity_flush(
             pending, &contacts->level.world->storage.bodies[body].motion))) {
        return ab_fail(error, "Cannot flush classic pending velocity");
    }
    return true;
}
