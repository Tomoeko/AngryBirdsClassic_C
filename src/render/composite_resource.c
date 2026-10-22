#include "ab/composite_resource.h"
#include "ab/sprite_resource.h"

#include <stdlib.h>

struct AbCompositeResource {
    AbResourceSource source;
    AbCompositeResources *composites;
    AbResolvedComposite *views;
    AbResourceMember *members;
    size_t references, bytes;
};

static void retain_resource(const AbResourceSource *source) {
    AbCompositeResource *resource = source->context;
    ++resource->references;
}

static void release_resource(const AbResourceSource *source) {
    AbCompositeResource *resource = source->context;
    if (--resource->references) {
        return;
    }
    ab_composite_resources_close(resource->composites);
    free(resource->members);
    free(resource->views);
    free(resource);
}

static bool resolve_sprite(void *context, AbSpan name, AbSpriteBinding *output,
                           AbError *error) {
    const AbResourceOwners *const *sheets = context;
    AbResourceMemberView selected;
    if (!ab_resource_owners_find_member(*sheets, AB_SPRITE_REGISTRY_SPRITE, name,
                                        &selected)) {
        return ab_fail(error, "Missing ordinary sprite for composite source");
    }
    const AbSpriteResource *resource = ab_sprite_resource_from_source(selected.source);
    return ab_sprite_resource_member(resource, selected.member, output) ||
           ab_fail(error, "Composite source requires a decoded SPRT provider");
}

static bool source_members(AbCompositeResource *resource, size_t limit,
                           AbError *error) {
    size_t count = ab_composite_resources_count(resource->composites);
    size_t stride = sizeof(*resource->views) + sizeof(*resource->members);
    if (count > (limit - resource->bytes) / stride) {
        return ab_fail(error, "Composite source members exceed allocation limit");
    }
    if (count) {
        resource->views = calloc(count, sizeof(*resource->views));
        resource->members = calloc(count, sizeof(*resource->members));
        if (!resource->views || !resource->members) {
            return ab_fail(error, "Cannot allocate composite source member views");
        }
    }
    resource->source.members = resource->members;
    for (size_t index = 0; index < count; ++index) {
        AbResolvedComposite candidate, selected;
        if (!ab_composite_resources_item(resource->composites, index, &candidate) ||
            !ab_composite_resources_find(resource->composites, candidate.source->name,
                                         &selected)) {
            return ab_fail(error, "Cannot query decoded composite source member");
        }
        if (candidate.source == selected.source) {
            size_t member = resource->source.count++;
            resource->views[member] = selected;
            resource->members[member] =
                (AbResourceMember){selected.source->name, resource->views + member};
        }
    }
    resource->bytes += count * stride;
    return true;
}

bool ab_composite_resource_open(const AbZip *archive, const char *metadata_path,
                                const AbResourceOwners *sheets, size_t allocation_limit,
                                size_t entry_limit, AbCompositeResource **output,
                                AbError *error) {
    if (!output) {
        return ab_fail(error, "Missing composite source output");
    }
    *output = NULL;
    if (!archive || !archive->input.data || !metadata_path || !sheets ||
        allocation_limit < sizeof(AbCompositeResource)) {
        return ab_fail(error, "Invalid composite source input/storage bounds");
    }
    AbCompositeResource *resource = calloc(1, sizeof(*resource));
    if (!resource) {
        return ab_fail(error, "Cannot allocate composite source owner");
    }
    resource->references = 1;
    resource->bytes = sizeof(*resource);
    resource->source = (AbResourceSource){.kind = AB_SPRITE_REGISTRY_COMPOSITE,
                                          .context = resource,
                                          .retain = retain_resource,
                                          .release = release_resource};
    if (!ab_composite_resources_open_resolved(
            archive, &metadata_path, 1, allocation_limit - resource->bytes, entry_limit,
            resolve_sprite, &sheets, &resource->composites, error)) {
        goto release_source;
    }
    resource->bytes += ab_composite_resources_bytes(resource->composites);
    if (!source_members(resource, allocation_limit, error)) {
        goto release_source;
    }
    *output = resource;
    return true;

release_source:
    ab_composite_resource_close(resource);
    return false;
}

void ab_composite_resource_close(AbCompositeResource *resource) {
    if (resource) {
        release_resource(&resource->source);
    }
}

const AbResourceSource *
ab_composite_resource_source(const AbCompositeResource *resource) {
    return resource ? &resource->source : NULL;
}

const AbCompositeResource *
ab_composite_resource_from_source(const AbResourceSource *source) {
    if (!source || source->retain != retain_resource ||
        source->release != release_resource || source->unload || !source->context) {
        return NULL;
    }
    const AbCompositeResource *resource = source->context;
    return source == &resource->source ? resource : NULL;
}

size_t ab_composite_resource_bytes(const AbCompositeResource *resource) {
    return resource ? resource->bytes : 0;
}

bool ab_composite_resource_member(const AbCompositeResource *resource,
                                  const AbResourceMember *member,
                                  AbResolvedComposite *output) {
    if (!resource || !member || !output) {
        return false;
    }
    for (size_t index = 0; index < resource->source.count; ++index) {
        if (member == resource->members + index) {
            *output = resource->views[index];
            return true;
        }
    }
    return false;
}

bool ab_composite_resource_find(const AbCompositeResource *resource,
                                const AbSpriteRegistry *registry, AbSpan name,
                                AbResolvedComposite *output) {
    AbSpriteRegistryEntry selected;
    if (!resource || !output ||
        !ab_sprite_registry_find(registry, name, AB_SPRITE_REGISTRY_COMPOSITE,
                                 &selected)) {
        return false;
    }
    for (size_t index = 0; index < resource->source.count; ++index) {
        if (selected.resource == resource->views + index) {
            *output = resource->views[index];
            return true;
        }
    }
    return false;
}
