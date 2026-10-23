#include "ab/classic_object_hooks.h"

#include "ab/string_map.h"

#include <stdlib.h>

struct AbClassicObjectHooks {
    AbClassicLevel *level;
    AbClassicController *controller;
    AbStringMap *registered;
    AbGameTailHook *rows;
    AbGameTailHooks source;
    AbSpan previous;
    size_t capacity, cursor, bytes;
    bool has_previous;
};

static bool begin(void *context, AbError *error) {
    (void)error;
    AbClassicObjectHooks *owner = context;
    owner->cursor = 0;
    owner->has_previous = false;
    return true;
}

static bool next(void *context, AbGameTailHook **output, AbError *error) {
    AbClassicObjectHooks *owner = context;
    if (owner->has_previous && !ab_string_map_resume(owner->registered, owner->previous,
                                                     &owner->cursor, error))
        return false;
    AbStringMapEntry entry;
    if (!ab_string_map_next(owner->registered, &owner->cursor, &entry, error))
        return false;
    owner->has_previous = entry.present;
    if (entry.present)
        owner->previous = entry.key;
    *output = entry.present ? (AbGameTailHook *)entry.value : NULL;
    return true;
}

static bool prune(void *context, AbError *error) {
    AbClassicObjectHooks *owner = context;
    if (!owner->has_previous)
        return ab_fail(error, "Missing current object-update registry key");
    if (!ab_string_map_set(owner->registered, owner->previous, 0, false, error))
        return false;
    AbStringMapLayout layout;
    if (!ab_string_map_layout(owner->registered, &layout))
        return ab_fail(error, "Missing object-update registry layout");
    owner->source.count = layout.count;
    return true;
}

bool ab_classic_object_hooks_open(AbClassicLevel *level, size_t storage_bytes,
                                  AbClassicObjectHooks **output, AbError *error) {
    if (!output)
        return ab_fail(error, "Missing object-update registry output");
    *output = NULL;
    AbClassicLevelView view = ab_classic_level_view(level);
    if (!view.properties || storage_bytes < sizeof(AbClassicObjectHooks))
        return ab_fail(error, "Missing object-update level/storage");
    size_t capacity = view.properties->capacity;
    size_t remaining = storage_bytes - sizeof(AbClassicObjectHooks);
    if (capacity > remaining / sizeof(AbGameTailHook))
        return ab_fail(error, "Object-update registry exceeds storage budget");
    size_t nodes = 1;
    while (nodes < capacity) {
        if (nodes > SIZE_MAX / 2)
            return ab_fail(error, "Object-update registry capacity overflow");
        nodes *= 2;
    }
    AbClassicObjectHooks *owner = calloc(1, sizeof(*owner));
    if (!owner)
        return ab_fail(error, "Cannot allocate object-update registry");
    owner->level = level;
    owner->capacity = capacity;
    owner->bytes = sizeof(*owner) + capacity * sizeof(*owner->rows);
    if (capacity)
        owner->rows = calloc(capacity, sizeof(*owner->rows));
    if ((capacity && !owner->rows) ||
        !ab_string_map_open(0, nodes, storage_bytes - owner->bytes, &owner->registered,
                            error)) {
        ab_classic_object_hooks_close(owner);
        return error && error->message[0]
                   ? false
                   : ab_fail(error, "Cannot allocate stable object-update rows");
    }
    owner->bytes += ab_string_map_bytes(owner->registered);
    owner->source = (AbGameTailHooks){
        .context = owner, .begin = begin, .next = next, .prune = prune};
    *output = owner;
    return true;
}

void ab_classic_object_hooks_close(AbClassicObjectHooks *owner) {
    if (!owner)
        return;
    ab_string_map_close(owner->registered);
    free(owner->rows);
    free(owner);
}

size_t ab_classic_object_hooks_bytes(const AbClassicObjectHooks *owner) {
    return owner ? owner->bytes : 0;
}

bool ab_classic_object_hooks_register(AbClassicObjectHooks *owner, size_t object,
                                      AbObjectUpdates *updates, AbError *error) {
    AbClassicLevelView level = ab_classic_level_view(owner ? owner->level : NULL);
    if (!owner || !updates || !level.bodies || object >= level.bodies->count ||
        object >= owner->capacity || level.removed_objects[object])
        return ab_fail(error, "Invalid registered object-update identity");
    AbGameTailObject *binding =
        owner->controller ? ab_classic_controller_object(owner->controller, object)
                          : NULL;
    if (owner->controller && !binding)
        return ab_fail(error, "Object-update lacks its controller registration");
    AbGameTailHook *row = owner->rows + object;
    *row =
        (AbGameTailHook){binding, updates, &level.properties->objects[object].physical};
    if (!ab_string_map_set(owner->registered, level.bodies->objects[object].name,
                           (uintptr_t)row, true, error))
        return false;
    AbStringMapLayout layout;
    if (!ab_string_map_layout(owner->registered, &layout))
        return ab_fail(error, "Missing registered object-update layout");
    owner->source.count = layout.count;
    return true;
}

bool ab_classic_object_hooks_bind(AbClassicObjectHooks *owner,
                                  AbClassicController *controller,
                                  AbGameTailHooks **output, AbError *error) {
    if (!owner || owner->controller || !controller || !output)
        return ab_fail(error, "Invalid object-update controller binding");
    for (size_t index = 0; index < owner->capacity; ++index) {
        AbGameTailHook *row = owner->rows + index;
        if (!row->updates)
            continue;
        row->object = ab_classic_controller_object(controller, index);
        if (!row->object)
            return ab_fail(error, "Object-update has no stable controller identity");
    }
    owner->controller = controller;
    *output = &owner->source;
    return true;
}
