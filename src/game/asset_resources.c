#include "ab/asset_resources.h"

#include <stdlib.h>

struct AbAssetResources {
    const AbZip *archive;
    AbAssetResourceLimits limits;
    AbResourceOwners owners;
    AbSpriteRegistry registry;
    size_t base_bytes;
    bool busy, failed;
};

static bool account(size_t count, size_t stride, size_t limit, size_t *used,
                    AbError *error) {
    if (*used > limit || count > (limit - *used) / stride) {
        return ab_fail(error, "Asset resource storage exceeds allocation limit");
    }
    *used += count * stride;
    return true;
}

bool ab_asset_resources_open(const AbZip *archive, AbAssetResourceLimits limits,
                             AbAssetResources **output, AbError *error) {
    if (!output) {
        return ab_fail(error, "Missing asset resource host output");
    }
    *output = NULL;
    size_t used = sizeof(AbAssetResources);
    if (!archive || !archive->input.data ||
        !account(limits.sources, sizeof(AbResourceOwnerEntry), limits.storage_bytes,
                 &used, error) ||
        !account(limits.bindings, sizeof(AbSpriteRegistryEntry), limits.storage_bytes,
                 &used, error)) {
        return ab_fail(error, "Invalid asset resource archive/storage bounds");
    }
    AbAssetResources *resources = calloc(1, sizeof(*resources));
    if (!resources) {
        return ab_fail(error, "Cannot allocate asset resource host");
    }
    resources->archive = archive;
    resources->limits = limits;
    resources->base_bytes = used;
    if (limits.bindings) {
        resources->registry.entries =
            calloc(limits.bindings, sizeof(*resources->registry.entries));
        if (!resources->registry.entries) {
            ab_fail(error, "Cannot allocate asset resource registry");
            goto release_resources;
        }
    }
    resources->registry.capacity = limits.bindings;
    AbResourceOwnerEntry *entries =
        limits.sources ? calloc(limits.sources, sizeof(*entries)) : NULL;
    if (limits.sources && !entries) {
        ab_fail(error, "Cannot allocate asset resource source slots");
        goto release_resources;
    }
    if (!ab_resource_owners_init(&resources->owners, entries, limits.sources,
                                 &resources->registry, error)) {
        free(entries);
        goto release_resources;
    }
    *output = resources;
    return true;

release_resources:
    ab_asset_resources_close(resources);
    return false;
}

void ab_asset_resources_close(AbAssetResources *resources) {
    if (resources) {
        ab_resource_owners_clear(&resources->owners, NULL);
        free(resources->owners.entries);
        free(resources->registry.entries);
        free(resources);
    }
}

size_t ab_asset_resources_bytes(const AbAssetResources *resources) {
    if (!resources) {
        return 0;
    }
    size_t bytes = resources->base_bytes;
    for (size_t index = 0; index < resources->owners.capacity; ++index) {
        const AbResourceOwnerEntry *entry = resources->owners.entries + index;
        if (!entry->used) {
            continue;
        }
        bytes +=
            ab_sprite_resource_bytes(ab_sprite_resource_from_source(entry->source));
        bytes += ab_composite_resource_bytes(
            ab_composite_resource_from_source(entry->source));
    }
    return bytes;
}

bool ab_asset_resources_failed(const AbAssetResources *resources) {
    return resources && resources->failed;
}

const AbResourceOwners *ab_asset_resources_owners(const AbAssetResources *resources) {
    return resources ? &resources->owners : NULL;
}

const AbSpriteRegistry *ab_asset_resources_registry(const AbAssetResources *resources) {
    return resources ? &resources->registry : NULL;
}

static bool create_source(AbAssetResources *resources, AbSpriteRegistryKind kind,
                          const char *path, AbSpan key, AbError *error) {
    if (ab_resource_owners_find(&resources->owners, kind, key)) {
        return true;
    }
    size_t used = ab_asset_resources_bytes(resources);
    size_t available = resources->limits.storage_bytes - used;
    AbSpriteResource *sprite = NULL;
    AbCompositeResource *composite = NULL;
    const AbResourceSource *source;
    if (kind == AB_SPRITE_REGISTRY_SPRITE) {
        if (!ab_sprite_resource_open(resources->archive, path, available,
                                     resources->limits.entry_bytes, &sprite, error)) {
            return false;
        }
        source = ab_sprite_resource_source(sprite);
    } else {
        if (!ab_composite_resource_open(resources->archive, path, &resources->owners,
                                        available, resources->limits.entry_bytes,
                                        &composite, error)) {
            return false;
        }
        source = ab_composite_resource_source(composite);
    }
    bool okay = ab_resource_owners_put(&resources->owners, key, source, error);
    ab_composite_resource_close(composite);
    ab_sprite_resource_close(sprite);
    return okay;
}

bool ab_asset_resources_host(void *context, AbAssetCall *call, AbError *error) {
    AbAssetResources *resources = context;
    if (!resources || !call || resources->busy || resources->failed) {
        return ab_fail(error, "Invalid/failed/reentrant asset resource host");
    }
    if (call->operation < AB_ASSET_CREATE_SHEET ||
        call->operation > AB_ASSET_RELEASE_COMPOSITES) {
        return ab_fail(error, "Unsupported asset resource host operation");
    }
    char path[256], key[256];
    size_t path_size, key_size;
    resources->busy = true;
    bool okay = ab_asset_resource_path(call, path, sizeof(path), &path_size, error) &&
                ab_asset_sheet_key((AbSpan){(const uint8_t *)path, path_size}, key,
                                   sizeof(key), &key_size, error);
    if (okay) {
        AbSpriteRegistryKind kind = call->operation <= AB_ASSET_RELEASE_SHEET
                                        ? AB_SPRITE_REGISTRY_SPRITE
                                        : AB_SPRITE_REGISTRY_COMPOSITE;
        AbSpan owner_key = {(const uint8_t *)key, key_size};
        bool loading = call->operation == AB_ASSET_CREATE_SHEET ||
                       call->operation == AB_ASSET_CREATE_COMPOSITES;
        okay = loading ? create_source(resources, kind, path, owner_key, error)
                       : ab_resource_owners_remove(&resources->owners, kind, owner_key,
                                                   false, error);
    }
    resources->busy = false;
    resources->failed = !okay;
    return okay;
}

bool ab_asset_resources_find_sprite(const AbAssetResources *resources, AbSpan name,
                                    AbSpriteAtlasBinding *output) {
    if (!resources || !output) {
        return false;
    }
    for (size_t index = 0; index < resources->owners.capacity; ++index) {
        const AbResourceOwnerEntry *entry = resources->owners.entries + index;
        if (entry->used &&
            ab_sprite_resource_find(ab_sprite_resource_from_source(entry->source),
                                    &resources->registry, name, output)) {
            return true;
        }
    }
    return false;
}

bool ab_asset_resources_find_composite(const AbAssetResources *resources, AbSpan name,
                                       AbResolvedComposite *output) {
    if (!resources || !output) {
        return false;
    }
    for (size_t index = 0; index < resources->owners.capacity; ++index) {
        const AbResourceOwnerEntry *entry = resources->owners.entries + index;
        if (entry->used &&
            ab_composite_resource_find(ab_composite_resource_from_source(entry->source),
                                       &resources->registry, name, output)) {
            return true;
        }
    }
    return false;
}

bool ab_asset_resources_resolve_sprite(const AbAssetResources *resources,
                                       AbSpriteBinding sprite,
                                       AbSpriteAtlasBinding *output) {
    if (!resources || resources->failed || !output)
        return false;
    for (size_t index = 0; index < resources->owners.capacity; ++index) {
        const AbResourceOwnerEntry *entry = resources->owners.entries + index;
        if (!entry->used)
            continue;
        const AbSpriteResource *sheet = ab_sprite_resource_from_source(entry->source);
        if (sheet &&
            ab_sprite_atlas_resolve(ab_sprite_resource_atlas(sheet), sprite, output))
            return true;
    }
    return false;
}
