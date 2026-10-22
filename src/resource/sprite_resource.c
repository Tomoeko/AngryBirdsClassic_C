#include "ab/sprite_resource.h"

#include <stdlib.h>

struct AbSpriteResource {
    AbResourceSource source;
    AbSpriteSet *sprites;
    AbSpriteAtlas *atlas;
    AbResourceMember *members;
    size_t references, bytes;
};

static void retain_resource(const AbResourceSource *source) {
    AbSpriteResource *resource = source->context;
    ++resource->references;
}

static void release_resource(const AbResourceSource *source) {
    AbSpriteResource *resource = source->context;
    if (--resource->references) {
        return;
    }
    ab_sprite_atlas_close(resource->atlas);
    ab_sprite_set_close(resource->sprites);
    free(resource->members);
    free(resource);
}

static void unload_image(const AbResourceSource *source) {
    AbSpriteResource *resource = source->context;
    resource->bytes -= ab_sprite_atlas_bytes(resource->atlas);
    ab_sprite_atlas_close(resource->atlas);
    resource->atlas = NULL;
}

static bool source_members(AbSpriteResource *resource, size_t limit, AbError *error) {
    AbSpriteSheetView view;
    if (!ab_sprite_set_sheet(resource->sprites, 0, &view)) {
        return ab_fail(error, "Cannot read sprite resource source sheet");
    }
    size_t count = 0;
    for (size_t index = 0; index < view.sheet->count; ++index) {
        const AbSpriteRegion *region = view.sheet->sprites + index;
        count += ab_sprite_find(view.sheet, region->name) == region;
    }
    if (count > (limit - resource->bytes) / sizeof(*resource->members)) {
        return ab_fail(error, "Sprite resource members exceed allocation limit");
    }
    if (count) {
        resource->members = calloc(count, sizeof(*resource->members));
        if (!resource->members) {
            return ab_fail(error, "Cannot allocate sprite resource member views");
        }
    }
    resource->source.members = resource->members;
    for (size_t index = 0; index < view.sheet->count; ++index) {
        const AbSpriteRegion *region = view.sheet->sprites + index;
        if (ab_sprite_find(view.sheet, region->name) == region) {
            resource->members[resource->source.count++] =
                (AbResourceMember){region->name, region};
        }
    }
    resource->bytes += count * sizeof(*resource->members);
    return true;
}

bool ab_sprite_resource_open(const AbZip *archive, const char *metadata_path,
                             size_t allocation_limit, size_t entry_limit,
                             AbSpriteResource **output, AbError *error) {
    if (!output) {
        return ab_fail(error, "Missing sprite resource output");
    }
    *output = NULL;
    if (!archive || !archive->input.data || !metadata_path ||
        allocation_limit < sizeof(AbSpriteResource)) {
        return ab_fail(error, "Invalid sprite resource input/storage bounds");
    }
    const AbZipEntry *entry = ab_zip_find(archive, metadata_path);
    if (!entry || entry->size > entry_limit) {
        return ab_fail(error, "Missing/oversized sprite resource metadata payload");
    }
    AbSpriteResource *resource = calloc(1, sizeof(*resource));
    if (!resource) {
        return ab_fail(error, "Cannot allocate sprite resource owner");
    }
    resource->references = 1;
    resource->bytes = sizeof(*resource);
    resource->source = (AbResourceSource){.kind = AB_SPRITE_REGISTRY_SPRITE,
                                          .context = resource,
                                          .retain = retain_resource,
                                          .release = release_resource,
                                          .unload = unload_image};
    if (!ab_sprite_set_open(archive, &metadata_path, 1,
                            allocation_limit - resource->bytes, &resource->sprites,
                            error)) {
        goto release_source;
    }
    resource->bytes += ab_sprite_set_bytes(resource->sprites);
    if (!source_members(resource, allocation_limit, error) ||
        !ab_sprite_atlas_open(archive, resource->sprites,
                              allocation_limit - resource->bytes, entry_limit,
                              &resource->atlas, error)) {
        goto release_source;
    }
    resource->bytes += ab_sprite_atlas_bytes(resource->atlas);
    *output = resource;
    return true;

release_source:
    ab_sprite_resource_close(resource);
    return false;
}

void ab_sprite_resource_close(AbSpriteResource *resource) {
    if (resource) {
        release_resource(&resource->source);
    }
}

const AbResourceSource *ab_sprite_resource_source(const AbSpriteResource *resource) {
    return resource ? &resource->source : NULL;
}

const AbSpriteResource *ab_sprite_resource_from_source(const AbResourceSource *source) {
    if (!source || source->retain != retain_resource ||
        source->release != release_resource || source->unload != unload_image ||
        !source->context) {
        return NULL;
    }
    const AbSpriteResource *resource = source->context;
    return source == &resource->source ? resource : NULL;
}

bool ab_sprite_resource_member(const AbSpriteResource *resource,
                               const AbResourceMember *member,
                               AbSpriteBinding *output) {
    if (!resource || !member || !output) {
        return false;
    }
    for (size_t index = 0; index < resource->source.count; ++index) {
        if (member != resource->members + index) {
            continue;
        }
        AbSpriteSheetView view;
        if (!ab_sprite_set_sheet(resource->sprites, 0, &view)) {
            return false;
        }
        *output = (AbSpriteBinding){view.metadata_path, view.sheet, member->resource};
        return true;
    }
    return false;
}

const AbSpriteAtlas *ab_sprite_resource_atlas(const AbSpriteResource *resource) {
    return resource ? resource->atlas : NULL;
}

AbSpan ab_sprite_resource_path(const AbSpriteResource *resource) {
    AbSpriteSheetView view;
    return resource && ab_sprite_set_sheet(resource->sprites, 0, &view)
               ? view.metadata_path
               : (AbSpan){0};
}

size_t ab_sprite_resource_bytes(const AbSpriteResource *resource) {
    return resource ? resource->bytes : 0;
}

bool ab_sprite_resource_find(const AbSpriteResource *resource,
                             const AbSpriteRegistry *registry, AbSpan name,
                             AbSpriteAtlasBinding *output) {
    return resource &&
           ab_sprite_atlas_find_registered(resource->atlas, registry, name, output);
}
