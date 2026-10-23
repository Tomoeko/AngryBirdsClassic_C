#include "ab/extra_draw.h"
#include "ab/string_map.h"
#include "ab/z_order.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

struct AbExtraDraws {
    AbStringMap *registry;
    AbZOrderEntry *sorted;
    size_t count, capacity;
    bool drawing, calling, failed;
};

bool ab_extra_draws_open(size_t nodes, size_t storage_bytes, AbExtraDraws **output,
                         AbError *error) {
    if (!output)
        return ab_fail(error, "Missing extra-draw registry output");
    *output = NULL;
    if ((nodes & (nodes - 1)) || storage_bytes < sizeof(AbExtraDraws) ||
        nodes > (storage_bytes - sizeof(AbExtraDraws)) / sizeof(AbZOrderEntry))
        return ab_fail(error, "Invalid extra-draw registry storage");
    AbExtraDraws *owner = calloc(1, sizeof(*owner));
    if (!owner)
        return ab_fail(error, "Cannot allocate extra-draw registry");
    owner->capacity = nodes;
    if (nodes) {
        owner->sorted = calloc(nodes, sizeof(*owner->sorted));
        if (!owner->sorted) {
            ab_extra_draws_close(owner);
            return ab_fail(error, "Cannot allocate sorted extra-draw references");
        }
    }
    if (!ab_string_map_open(
            0, nodes, storage_bytes - sizeof(*owner) - nodes * sizeof(*owner->sorted),
            &owner->registry, error)) {
        ab_extra_draws_close(owner);
        return false;
    }
    *output = owner;
    return true;
}

void ab_extra_draws_close(AbExtraDraws *owner) {
    if (!owner)
        return;
    ab_string_map_close(owner->registry);
    free(owner->sorted);
    free(owner);
}

static bool valid_function(const AbExtraDrawFunction *function, AbError *error) {
    return (function && function->draw &&
            (!function->z_present || isfinite(function->z))) ||
           ab_fail(error, "Invalid registered extra-draw function/z");
}

bool ab_extra_draws_set(AbExtraDraws *owner, AbSpan name,
                        const AbExtraDrawFunction *function, AbError *error) {
    if (!owner || owner->failed || !valid_function(function, error))
        return error && error->message[0]
                   ? false
                   : ab_fail(error, "Missing or stopped extra-draw registry");
    return ab_string_map_set(owner->registry, name, (uintptr_t)function, true, error);
}

bool ab_extra_draws_remove(AbExtraDraws *owner, AbSpan name, AbError *error) {
    return owner && !owner->failed
               ? ab_string_map_set(owner->registry, name, 0, false, error)
               : ab_fail(error, "Missing or stopped extra-draw removal registry");
}

bool ab_extra_draws_find(const AbExtraDraws *owner, AbSpan name,
                         const AbExtraDrawFunction **output, AbError *error) {
    if (!owner || owner->failed || !output)
        return ab_fail(error, "Invalid extra-draw lookup");
    uintptr_t value;
    bool present;
    if (!ab_string_map_get(owner->registry, name, &value, &present, error))
        return false;
    *output = present ? (const AbExtraDrawFunction *)value : NULL;
    return true;
}

static bool invoke(AbExtraDraws *owner, const AbExtraDrawFunction *function,
                   AbExtraDrawInput input, AbError *error) {
    if (owner->calling || !valid_function(function, error))
        return ab_fail(error, "Invalid or reentrant extra draw");
    owner->calling = true;
    bool okay = function->draw(function->context, function->parameter, input, error);
    owner->calling = false;
    return okay;
}

static bool traverse(AbExtraDraws *owner, AbExtraDrawInput input, bool before,
                     AbError *error) {
    size_t cursor = 0;
    for (;;) {
        AbStringMapEntry entry;
        if (!ab_string_map_next(owner->registry, &cursor, &entry, error))
            return false;
        if (!entry.present)
            return true;
        const AbExtraDrawFunction *function = (const AbExtraDrawFunction *)entry.value;
        if (!valid_function(function, error))
            return false;
        if (before && function->z_present) {
            if (owner->count == owner->capacity)
                return ab_fail(error, "Sorted extra-draw references exceed capacity");
            owner->sorted[owner->count++] = (AbZOrderEntry){function->z, entry.value};
            if (input.publish_has_z)
                input.publish_has_z(input.state_context, true);
        } else if (!function->z_present && function->before_blocks == before &&
                   !invoke(owner, function, input, error)) {
            return false;
        }
        if (!ab_string_map_resume(owner->registry, entry.key, &cursor, error))
            return false;
    }
}

bool ab_extra_draws_begin(AbExtraDraws *owner, AbExtraDrawInput input, bool *has_z,
                          AbError *error) {
    if (!owner || owner->drawing || owner->calling || owner->failed || !has_z ||
        !input.list || !input.width || !input.height)
        return ab_fail(error, "Invalid or reentrant extra-draw preparation");
    owner->drawing = true;
    owner->count = 0;
    *has_z = false;
    if (input.publish_has_z)
        input.publish_has_z(input.state_context, false);
    bool okay = traverse(owner, input, true, error) &&
                ab_z_order_sort(owner->sorted, owner->count, error);
    owner->failed = !okay;
    if (okay)
        *has_z = owner->count != 0;
    return okay;
}

bool ab_extra_draws_layer(AbExtraDraws *owner, float layer, AbExtraDrawInput input,
                          AbError *error) {
    if (!owner || !owner->drawing || owner->calling || owner->failed ||
        !isfinite(layer))
        return ab_fail(error, "Invalid or reentrant extra-draw layer");
    bool okay = true;
    for (size_t index = 0; okay && index < owner->count;) {
        const AbExtraDrawFunction *function =
            (const AbExtraDrawFunction *)owner->sorted[index].identity;
        if (function->z <= layer) {
            okay = invoke(owner, function, input, error);
            if (okay) {
                --owner->count;
                memmove(owner->sorted + index, owner->sorted + index + 1,
                        (owner->count - index) * sizeof(*owner->sorted));
            }
        } else {
            ++index;
        }
    }
    owner->failed = !okay;
    return okay;
}

bool ab_extra_draws_finish(AbExtraDraws *owner, AbExtraDrawInput input,
                           AbError *error) {
    if (!ab_extra_draws_layer(owner, 1e10f, input, error))
        return false;
    bool okay = traverse(owner, input, false, error);
    owner->failed = !okay;
    owner->drawing = false;
    return okay;
}
