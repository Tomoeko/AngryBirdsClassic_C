#include "ab/composite_resources.h"
#include "ab/composite_part.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    AbResolvedComposite view;
    AbSpriteBinding *parts;
} CompositeRecord;

typedef struct {
    AbSpan path;
    AbBuffer bytes;
    AbCompositeSet set;
    CompositeRecord *records;
} CompositeFile;

struct AbCompositeResources {
    CompositeFile *files;
    size_t file_count, count, bytes;
};

static bool account(size_t count, size_t element_size, size_t limit, size_t *used,
                    AbError *error) {
    if (*used > limit || count > (limit - *used) / element_size) {
        return ab_fail(error, "Composite resource owner exceeds allocation limit");
    }
    *used += count * element_size;
    return true;
}

static bool read_file(const AbZip *archive, const char *path, size_t limit,
                      size_t entry_limit, size_t *used, CompositeFile *file,
                      AbError *error) {
    const AbZipEntry *entry = path ? ab_zip_find(archive, path) : NULL;
    if (!entry) {
        return ab_fail(error, "Missing composite resource metadata");
    }
    file->path = entry->name;
    size_t available = limit - *used;
    if (available > entry_limit) {
        available = entry_limit;
    }
    if (!ab_zip_read(archive, (size_t)(entry - archive->entries), available,
                     &file->bytes, error)) {
        return false;
    }
    *used += file->bytes.size;
    if (!ab_composite_read((AbSpan){file->bytes.data, file->bytes.size}, limit - *used,
                           &file->set, error)) {
        return false;
    }
    if (!account(file->set.count, sizeof(*file->set.composites), limit, used, error)) {
        return false;
    }
    for (size_t index = 0; index < file->set.count; ++index) {
        const AbComposite *source = file->set.composites + index;
        if (!account(source->part_count, sizeof(*source->parts), limit, used, error) ||
            !account(source->point_count, sizeof(*source->points), limit, used,
                     error)) {
            return false;
        }
    }
    return true;
}

static bool bind_record(AbCompositeSpriteResolver resolve, void *context,
                        CompositeRecord *record, const AbComposite *source, AbSpan path,
                        bool extended, AbCompositeBoundsPart *workspace, size_t limit,
                        size_t *used, AbError *error) {
    if (!account(source->part_count, sizeof(*record->parts), limit, used, error)) {
        return false;
    }
    if (source->part_count) {
        record->parts = calloc(source->part_count, sizeof(*record->parts));
        if (!record->parts) {
            return ab_fail(error, "Cannot allocate resolved composite parts");
        }
    }
    record->view = (AbResolvedComposite){.metadata_path = path,
                                         .source = source,
                                         .parts = record->parts,
                                         .extended = extended};
    for (size_t index = 0; index < source->part_count; ++index) {
        const AbCompositePart *part = source->parts + index;
        if (!resolve(context, part->sprite, record->parts + index, error) ||
            !ab_composite_part_state(part, extended, record->parts[index].region,
                                     workspace + index, error)) {
            return false;
        }
    }
    return ab_composite_bounds(workspace, source->part_count, &record->view.bounds,
                               error);
}

static bool bind_file(AbCompositeSpriteResolver resolve, void *context,
                      CompositeFile *file, size_t limit, size_t *used, AbError *error) {
    if (!account(file->set.count, sizeof(*file->records), limit, used, error)) {
        return false;
    }
    if (file->set.count) {
        file->records = calloc(file->set.count, sizeof(*file->records));
        if (!file->records) {
            return ab_fail(error, "Cannot allocate composite resource records");
        }
    }
    size_t maximum_parts = 0;
    for (size_t index = 0; index < file->set.count; ++index) {
        if (file->set.composites[index].part_count > maximum_parts) {
            maximum_parts = file->set.composites[index].part_count;
        }
    }
    if (maximum_parts > limit / sizeof(AbCompositeBoundsPart)) {
        return ab_fail(error, "Composite bounds workspace exceeds allocation limit");
    }
    AbCompositeBoundsPart *workspace =
        maximum_parts ? calloc(maximum_parts, sizeof(*workspace)) : NULL;
    if (maximum_parts && !workspace) {
        return ab_fail(error, "Cannot allocate composite bounds workspace");
    }
    bool okay = true;
    for (size_t index = 0; okay && index < file->set.count; ++index) {
        okay = bind_record(resolve, context, file->records + index,
                           file->set.composites + index, file->path, file->set.extended,
                           workspace, limit, used, error);
    }
    free(workspace);
    return okay;
}

bool ab_composite_resources_open_resolved(const AbZip *archive,
                                          const char *const *paths, size_t count,
                                          size_t allocation_limit, size_t entry_limit,
                                          AbCompositeSpriteResolver resolve,
                                          void *context, AbCompositeResources **output,
                                          AbError *error) {
    if (!output) {
        return ab_fail(error, "Missing composite resource output");
    }
    *output = NULL;
    size_t used = sizeof(AbCompositeResources);
    if (!archive || !resolve || (count && !paths) ||
        !account(count, sizeof(CompositeFile), allocation_limit, &used, error)) {
        return ab_fail(error, "Invalid composite resource input or allocation limit");
    }
    AbCompositeResources *resources = calloc(1, sizeof(*resources));
    if (!resources) {
        return ab_fail(error, "Cannot allocate composite resource owner");
    }
    resources->file_count = count;
    if (count) {
        resources->files = calloc(count, sizeof(*resources->files));
        if (!resources->files) {
            ab_fail(error, "Cannot allocate composite resource file array");
            goto release_resources;
        }
    }
    for (size_t index = 0; index < count; ++index) {
        CompositeFile *file = resources->files + index;
        if (!read_file(archive, paths[index], allocation_limit, entry_limit, &used,
                       file, error) ||
            !bind_file(resolve, context, file, allocation_limit, &used, error)) {
            goto release_resources;
        }
        if (file->set.count > SIZE_MAX - resources->count) {
            ab_fail(error, "Composite resource count exceeds bounds");
            goto release_resources;
        }
        resources->count += file->set.count;
    }
    resources->bytes = used;
    *output = resources;
    return true;

release_resources:
    ab_composite_resources_close(resources);
    return false;
}

static bool resolve_explicit(void *context, AbSpan name, AbSpriteBinding *output,
                             AbError *error) {
    const AbSpriteSet *const *sprites = context;
    return ab_sprite_set_find(*sprites, name, output) ||
           ab_fail(error, "Missing ordinary sprite for composite part");
}

bool ab_composite_resources_open(const AbZip *archive, const AbSpriteSet *sprites,
                                 const char *const *paths, size_t count,
                                 size_t allocation_limit, size_t entry_limit,
                                 AbCompositeResources **output, AbError *error) {
    if (!sprites) {
        if (output) {
            *output = NULL;
        }
        return ab_fail(error, "Missing composite resource sprite set");
    }
    return ab_composite_resources_open_resolved(archive, paths, count, allocation_limit,
                                                entry_limit, resolve_explicit, &sprites,
                                                output, error);
}

void ab_composite_resources_close(AbCompositeResources *resources) {
    if (resources) {
        for (size_t index = 0; resources->files && index < resources->file_count;
             ++index) {
            CompositeFile *file = resources->files + index;
            for (size_t record = 0; file->records && record < file->set.count;
                 ++record) {
                free(file->records[record].parts);
            }
            free(file->records);
            ab_composite_free(&file->set);
            ab_buffer_free(&file->bytes);
        }
        free(resources->files);
        free(resources);
    }
}

size_t ab_composite_resources_count(const AbCompositeResources *resources) {
    return resources ? resources->count : 0;
}

size_t ab_composite_resources_bytes(const AbCompositeResources *resources) {
    return resources ? resources->bytes : 0;
}

bool ab_composite_resources_item(const AbCompositeResources *resources, size_t index,
                                 AbResolvedComposite *output) {
    if (!resources || !output || index >= resources->count) {
        return false;
    }
    for (size_t file = 0; file < resources->file_count; ++file) {
        const CompositeFile *source = resources->files + file;
        if (index < source->set.count) {
            *output = source->records[index].view;
            return true;
        }
        index -= source->set.count;
    }
    return false;
}

bool ab_composite_resources_find(const AbCompositeResources *resources, AbSpan name,
                                 AbResolvedComposite *output) {
    if (!resources || !output || (name.size && !name.data)) {
        return false;
    }
    for (size_t file = resources->file_count; file; --file) {
        const CompositeFile *source = resources->files + file - 1;
        for (size_t record = source->set.count; record; --record) {
            const AbResolvedComposite *candidate = &source->records[record - 1].view;
            if (candidate->source->name.size == name.size &&
                (!name.size ||
                 memcmp(candidate->source->name.data, name.data, name.size) == 0)) {
                *output = *candidate;
                return true;
            }
        }
    }
    return false;
}
