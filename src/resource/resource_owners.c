#include "ab/resource_owners.h"

#include <string.h>

static bool kind_valid(AbSpriteRegistryKind kind) {
    return kind == AB_SPRITE_REGISTRY_SPRITE || kind == AB_SPRITE_REGISTRY_COMPOSITE;
}

static bool same(AbSpan first, AbSpan second) {
    return first.size == second.size &&
           (!first.size || memcmp(first.data, second.data, first.size) == 0);
}

static bool valid_registry(const AbSpriteRegistry *registry) {
    return registry && registry->count <= registry->capacity &&
           registry->capacity <= SIZE_MAX / sizeof(*registry->entries) &&
           (!registry->capacity || registry->entries);
}

static bool valid_owners(const AbResourceOwners *owners) {
    return owners && owners->count <= owners->capacity &&
           owners->capacity <= SIZE_MAX / sizeof(*owners->entries) &&
           (!owners->capacity || owners->entries) && valid_registry(owners->registry);
}

static bool valid_key(AbSpan key) {
    return key.size <= 255 && (!key.size || key.data);
}

bool ab_resource_owners_init(AbResourceOwners *owners, AbResourceOwnerEntry *entries,
                             size_t capacity, AbSpriteRegistry *registry,
                             AbError *error) {
    if (!owners || capacity > SIZE_MAX / sizeof(*entries) || (capacity && !entries) ||
        !valid_registry(registry)) {
        return ab_fail(error, "Invalid resource owner initialization storage");
    }
    if (capacity) {
        memset(entries, 0, capacity * sizeof(*entries));
    }
    *owners = (AbResourceOwners){
        .entries = entries, .capacity = capacity, .registry = registry};
    return true;
}

static size_t find_slot(const AbResourceOwners *owners, AbSpriteRegistryKind kind,
                        AbSpan key) {
    for (size_t index = 0; index < owners->capacity; ++index) {
        const AbResourceOwnerEntry *entry = owners->entries + index;
        if (entry->used && entry->source->kind == kind &&
            same((AbSpan){entry->key, entry->key_size}, key)) {
            return index;
        }
    }
    return SIZE_MAX;
}

const AbResourceSource *ab_resource_owners_find(const AbResourceOwners *owners,
                                                AbSpriteRegistryKind kind, AbSpan key) {
    if (!valid_owners(owners) || !kind_valid(kind) || !valid_key(key)) {
        return NULL;
    }
    size_t index = find_slot(owners, kind, key);
    return index == SIZE_MAX ? NULL : owners->entries[index].source;
}

bool ab_resource_owners_find_member(const AbResourceOwners *owners,
                                    AbSpriteRegistryKind kind, AbSpan name,
                                    AbResourceMemberView *output) {
    if (!valid_owners(owners) || !kind_valid(kind) || !output ||
        (name.size && !name.data)) {
        return false;
    }
    bool found = false;
    AbResourceMemberView result = {0};
    for (size_t slot = 0; slot < owners->capacity; ++slot) {
        const AbResourceOwnerEntry *entry = owners->entries + slot;
        if (!entry->used || entry->source->kind != kind) {
            continue;
        }
        AbSpan key = {entry->key, entry->key_size};
        if (found) {
            size_t common = key.size < result.owner.size ? key.size : result.owner.size;
            int order = common ? memcmp(key.data, result.owner.data, common) : 0;
            if (order > 0 || (!order && key.size >= result.owner.size)) {
                continue;
            }
        }
        for (size_t member = 0; member < entry->source->count; ++member) {
            if (same(entry->source->members[member].name, name)) {
                result = (AbResourceMemberView){key, entry->source,
                                                entry->source->members + member};
                found = true;
                break;
            }
        }
    }
    if (found) {
        *output = result;
    }
    return found;
}

static bool valid_source(const AbResourceSource *source, size_t capacity) {
    if (!source || !kind_valid(source->kind) || !source->retain || !source->release ||
        (source->kind == AB_SPRITE_REGISTRY_SPRITE && !source->unload) ||
        source->count > capacity || (source->count && !source->members)) {
        return false;
    }
    for (size_t index = 0; index < source->count; ++index) {
        const AbResourceMember *member = source->members + index;
        if (!member->resource || (member->name.size && !member->name.data)) {
            return false;
        }
        for (size_t earlier = 0; earlier < index; ++earlier) {
            if (same(source->members[earlier].name, member->name)) {
                return false;
            }
        }
    }
    return true;
}

static bool source_contains(const AbResourceSource *source,
                            const AbSpriteRegistryEntry *entry) {
    if (!source || source->kind != entry->kind) {
        return false;
    }
    for (size_t index = 0; index < source->count; ++index) {
        const AbResourceMember *member = source->members + index;
        if (entry->resource == member->resource && same(entry->name, member->name)) {
            return true;
        }
    }
    return false;
}

static bool unregister_source(AbResourceOwners *owners, const AbResourceSource *source,
                              AbError *error) {
    for (size_t index = 0; index < source->count; ++index) {
        const AbResourceMember *member = source->members + index;
        if (!ab_sprite_registry_remove(owners->registry, member->name, source->kind,
                                       member->resource, error)) {
            return false;
        }
    }
    return true;
}

static bool finish(AbResourceOwners *owners, bool okay) {
    owners->busy = false;
    owners->failed = owners->failed || !okay;
    return okay;
}

bool ab_resource_owners_put(AbResourceOwners *owners, AbSpan key,
                            const AbResourceSource *source, AbError *error) {
    if (!valid_owners(owners) || owners->busy || owners->failed || !valid_key(key) ||
        !valid_source(source, owners->registry->capacity)) {
        return ab_fail(error, "Invalid resource source/key/owner state");
    }
    size_t index = find_slot(owners, source->kind, key);
    const AbResourceSource *old =
        index == SIZE_MAX ? NULL : owners->entries[index].source;
    if (index == SIZE_MAX) {
        for (size_t slot = 0; slot < owners->capacity; ++slot) {
            if (!owners->entries[slot].used) {
                index = slot;
                break;
            }
        }
    }
    if (index == SIZE_MAX) {
        return ab_fail(error, "Resource owner slot capacity exceeded");
    }
    size_t removed = 0;
    for (size_t entry = 0; entry < owners->registry->count; ++entry) {
        removed += source_contains(old, owners->registry->entries + entry);
    }
    if (source->count >
        owners->registry->capacity - (owners->registry->count - removed)) {
        return ab_fail(error, "Resource source exceeds sprite registry capacity");
    }
    AbResourceOwnerEntry *slot = owners->entries + index;
    owners->busy = true;
    if (old && !unregister_source(owners, old, error)) {
        return finish(owners, false);
    }
    if (key.size) {
        memmove(slot->key, key.data, key.size);
    }
    slot->key[key.size] = 0;
    slot->key_size = key.size;
    for (size_t member = 0; member < source->count; ++member) {
        AbSpriteRegistryEntry entry = {.name = source->members[member].name,
                                       .owner = {slot->key, slot->key_size},
                                       .kind = source->kind,
                                       .resource = source->members[member].resource};
        if (!ab_sprite_registry_add(owners->registry, entry, error)) {
            return finish(owners, false);
        }
    }
    source->retain(source);
    slot->source = source;
    slot->used = true;
    if (old) {
        old->release(old);
    } else {
        ++owners->count;
    }
    return finish(owners, true);
}

bool ab_resource_owners_remove(AbResourceOwners *owners, AbSpriteRegistryKind kind,
                               AbSpan key, bool unload_image, AbError *error) {
    if (!valid_owners(owners) || owners->busy || owners->failed || !kind_valid(kind) ||
        !valid_key(key)) {
        return ab_fail(error, "Invalid resource owner removal state/key");
    }
    size_t index = find_slot(owners, kind, key);
    if (index == SIZE_MAX) {
        return true;
    }
    AbResourceOwnerEntry *entry = owners->entries + index;
    const AbResourceSource *source = entry->source;
    owners->busy = true;
    if (!unregister_source(owners, source, error)) {
        return finish(owners, false);
    }
    if (kind == AB_SPRITE_REGISTRY_SPRITE && unload_image) {
        source->unload(source);
    } else {
        entry->used = false;
        entry->source = NULL;
        --owners->count;
        source->release(source);
    }
    return finish(owners, true);
}

bool ab_resource_owners_clear(AbResourceOwners *owners, AbError *error) {
    if (!valid_owners(owners) || owners->busy) {
        return ab_fail(error, "Invalid resource owner teardown state");
    }
    owners->busy = true;
    for (size_t index = 0; index < owners->capacity; ++index) {
        AbResourceOwnerEntry *entry = owners->entries + index;
        if (!entry->used) {
            continue;
        }
        const AbResourceSource *source = entry->source;
        if (!unregister_source(owners, source, error)) {
            return finish(owners, false);
        }
        entry->used = false;
        entry->source = NULL;
        --owners->count;
        source->release(source);
    }
    return finish(owners, true);
}
