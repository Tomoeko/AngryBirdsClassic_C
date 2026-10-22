#include "ab/game_object_remove.h"

#include <string.h>

static bool same_name(AbSpan first, AbSpan second) {
    return first.size == second.size &&
           (!first.size || !memcmp(first.data, second.data, first.size));
}

static bool remove_bird(AbGameObjectCoreRegistry *registry, AbSpan key,
                        AbError *error) {
    if (registry->bird_count > registry->bird_capacity ||
        (registry->bird_count && !registry->birds)) {
        return ab_fail(error, "Invalid native object-removal bird list");
    }
    for (size_t index = 0; index < registry->bird_count; ++index) {
        const AbGameObjectCore *bird = registry->birds[index];
        if (!bird || (bird->name.size && !bird->name.data)) {
            return ab_fail(error, "Missing native object-removal bird metadata");
        }
        if (!same_name(bird->name, key)) {
            continue;
        }
        --registry->bird_count;
        memmove(registry->birds + index, registry->birds + index + 1,
                (registry->bird_count - index) * sizeof(*registry->birds));
        break;
    }
    return true;
}

static bool destroy_body(AbGameObjectCoreRegistry *registry, AbGameObjectCore *object,
                         const AbGameObjectCoreHost *host, AbError *error) {
    if (!registry->world || registry->world->failed) {
        return ab_fail(error, "Missing healthy native object-removal world");
    }
    if (!registry->world->locked &&
        !ab_world_destroy_body(registry->world, object->body, error)) {
        return false;
    }
    if (!object->camera_animation) {
        return true;
    }
    if (!host || !host->close_camera_animation) {
        return ab_fail(error, "Missing removed-object camera animation host");
    }
    return host->close_camera_animation(host->context, object->name, error);
}

static bool detach(AbGameObjectCoreRegistry *registry, AbSpan key,
                   AbGameObjectCore *object, const AbGameObjectCoreHost *host,
                   AbError *error) {
    uintptr_t retained = object->renderable;
    if (retained && (!host || !host->retain_renderable || !host->remove_renderable ||
                     !host->release_renderable)) {
        return ab_fail(error, "Missing removed-object renderable services");
    }
    if (retained) {
        host->retain_renderable(host->context, retained);
        if (!host->remove_renderable(host->context, retained, error)) {
            host->release_renderable(host->context, retained);
            return false;
        }
    }
    uintptr_t current = object->renderable;
    object->renderable = 0;
    if (current) {
        host->release_renderable(host->context, current);
    }
    bool okay = ab_string_map_set(registry->objects, key, 0, false, error);
    if (okay && host && host->destroy_object) {
        host->destroy_object(host->context, object);
    }
    if (retained) {
        host->release_renderable(host->context, retained);
    }
    return okay;
}

bool ab_game_object_remove_core(AbGameObjectCoreRegistry *registry, AbSpan key,
                                const AbGameObjectCoreHost *host, bool *removed,
                                AbError *error) {
    if (!registry || !registry->objects || !removed || (key.size && !key.data)) {
        return ab_fail(error, "Invalid native object-removal input");
    }
    uintptr_t value = 0;
    bool present = false;
    if (!ab_string_map_get(registry->objects, key, &value, &present, error)) {
        return false;
    }
    if (!present) {
        *removed = false;
        return true;
    }
    AbGameObjectCore *object = (AbGameObjectCore *)value;
    if (!object || (object->name.size && !object->name.data) ||
        (object->rolling_sound.size && !object->rolling_sound.data)) {
        return ab_fail(error, "Missing native object-removal metadata");
    }
    if ((object->collider == AB_COLLIDER_BIRD ||
         object->collider == AB_COLLIDER_EAGLE) &&
        !remove_bird(registry, key, error)) {
        return false;
    }
    if (object->rolling_sound.size &&
        (!registry->rolling_levels ||
         !ab_string_map_set(registry->rolling_levels, object->rolling_sound, 0, true,
                            error))) {
        return ab_fail(error, "Cannot reset removed-object rolling level");
    }
    if (!destroy_body(registry, object, host, error) ||
        !detach(registry, key, object, host, error)) {
        return false;
    }
    *removed = true;
    return true;
}
