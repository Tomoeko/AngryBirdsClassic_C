#include "ab/sprite_set.h"

static bool valid_registry(const AbSpriteRegistry *registry) {
    return registry && registry->count <= registry->capacity &&
           registry->capacity <= SIZE_MAX / sizeof(*registry->entries) &&
           (!registry->capacity || registry->entries);
}

bool ab_sprite_set_register_sheet(const AbSpriteSet *set, size_t index, AbSpan owner,
                                  AbSpriteRegistry *registry, AbError *error) {
    AbSpriteSheetView view;
    if (!valid_registry(registry) || (owner.size && !owner.data) ||
        !ab_sprite_set_sheet(set, index, &view)) {
        return ab_fail(error, "Invalid sprite sheet registration input/storage");
    }
    size_t additions = 0;
    for (size_t region = 0; region < view.sheet->count; ++region) {
        const AbSpriteRegion *source = view.sheet->sprites + region;
        additions += ab_sprite_find(view.sheet, source->name) == source;
    }
    if (additions > registry->capacity - registry->count) {
        return ab_fail(error, "Sprite sheet exceeds registry capacity");
    }
    for (size_t region = 0; region < view.sheet->count; ++region) {
        const AbSpriteRegion *source = view.sheet->sprites + region;
        if (ab_sprite_find(view.sheet, source->name) != source) {
            continue;
        }
        AbSpriteRegistryEntry entry = {.name = source->name,
                                       .owner = owner,
                                       .kind = AB_SPRITE_REGISTRY_SPRITE,
                                       .resource = source};
        if (!ab_sprite_registry_add(registry, entry, error)) {
            return false;
        }
    }
    return true;
}

bool ab_sprite_set_unregister_sheet(const AbSpriteSet *set, size_t index,
                                    AbSpriteRegistry *registry, AbError *error) {
    AbSpriteSheetView view;
    if (!valid_registry(registry) || !ab_sprite_set_sheet(set, index, &view)) {
        return ab_fail(error, "Invalid sprite sheet removal input/storage");
    }
    for (size_t region = 0; region < view.sheet->count; ++region) {
        const AbSpriteRegion *source = view.sheet->sprites + region;
        if (ab_sprite_find(view.sheet, source->name) == source &&
            !ab_sprite_registry_remove(registry, source->name,
                                       AB_SPRITE_REGISTRY_SPRITE, source, error)) {
            return false;
        }
    }
    return true;
}

bool ab_sprite_set_find_registered(const AbSpriteSet *set,
                                   const AbSpriteRegistry *registry, AbSpan name,
                                   AbSpriteBinding *output) {
    AbSpriteRegistryEntry entry;
    if (!set || !output ||
        !ab_sprite_registry_find(registry, name, AB_SPRITE_REGISTRY_SPRITE, &entry)) {
        return false;
    }
    for (size_t index = 0; index < ab_sprite_set_count(set); ++index) {
        AbSpriteSheetView view;
        if (!ab_sprite_set_sheet(set, index, &view)) {
            return false;
        }
        for (size_t region = 0; region < view.sheet->count; ++region) {
            const AbSpriteRegion *source = view.sheet->sprites + region;
            if (source == entry.resource) {
                *output = (AbSpriteBinding){view.metadata_path, view.sheet, source};
                return true;
            }
        }
    }
    return false;
}
