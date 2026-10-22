#include "ab/sprite_registry.h"

#include <string.h>

static bool valid_registry(const AbSpriteRegistry *registry) {
    return registry && registry->count <= registry->capacity &&
           registry->capacity <= SIZE_MAX / sizeof(*registry->entries) &&
           (!registry->capacity || registry->entries);
}

static bool valid_span(AbSpan span) {
    return !span.size || span.data;
}

static bool same_name(AbSpan first, AbSpan second) {
    return first.size == second.size &&
           (!first.size || memcmp(first.data, second.data, first.size) == 0);
}

static bool resource_kind(AbSpriteRegistryKind kind) {
    return kind == AB_SPRITE_REGISTRY_SPRITE || kind == AB_SPRITE_REGISTRY_COMPOSITE;
}

bool ab_sprite_registry_add(AbSpriteRegistry *registry, AbSpriteRegistryEntry entry,
                            AbError *error) {
    if (!valid_registry(registry) || !resource_kind(entry.kind) || !entry.resource ||
        !valid_span(entry.name) || !valid_span(entry.owner)) {
        return ab_fail(error, "Invalid sprite registry binding/storage");
    }
    if (registry->count == registry->capacity) {
        return ab_fail(error, "Sprite registry capacity exceeded");
    }
    registry->entries[registry->count++] = entry;
    return true;
}

bool ab_sprite_registry_remove(AbSpriteRegistry *registry, AbSpan name,
                               AbSpriteRegistryKind kind, const void *resource,
                               AbError *error) {
    if (!valid_registry(registry) || !valid_span(name) || !resource_kind(kind) ||
        !resource) {
        return ab_fail(error, "Invalid sprite registry removal/storage");
    }
    size_t retained = 0;
    for (size_t index = 0; index < registry->count; ++index) {
        AbSpriteRegistryEntry entry = registry->entries[index];
        if (entry.kind == kind && entry.resource == resource &&
            same_name(entry.name, name)) {
            continue;
        }
        registry->entries[retained++] = entry;
    }
    registry->count = retained;
    return true;
}

bool ab_sprite_registry_find(const AbSpriteRegistry *registry, AbSpan name,
                             AbSpriteRegistryKind kind, AbSpriteRegistryEntry *output) {
    if (!valid_registry(registry) || !output || !valid_span(name) ||
        (kind != AB_SPRITE_REGISTRY_ANY && !resource_kind(kind))) {
        return false;
    }
    for (size_t remaining = registry->count; remaining; --remaining) {
        const AbSpriteRegistryEntry *entry = registry->entries + remaining - 1;
        if (!same_name(entry->name, name)) {
            continue;
        }
        if (kind != AB_SPRITE_REGISTRY_ANY && kind != entry->kind) {
            return false;
        }
        *output = *entry;
        return true;
    }
    return false;
}
