#include "ab/sprite_set.h"

#include <stdlib.h>

typedef struct {
    AbSpan path;
    AbBuffer bytes;
    AbSpriteSheet sheet;
} SpriteEntry;

struct AbSpriteSet {
    SpriteEntry *entries;
    size_t count, bytes;
};

bool ab_sprite_set_open(const AbZip *archive, const char *const *paths, size_t count,
                        size_t allocation_limit, AbSpriteSet **output, AbError *error) {
    if (!output) {
        return ab_fail(error, "Missing sprite set output");
    }
    *output = NULL;
    if (!archive || (count && !paths) || allocation_limit < sizeof(AbSpriteSet) ||
        count > (allocation_limit - sizeof(AbSpriteSet)) / sizeof(SpriteEntry)) {
        return ab_fail(error, "Invalid sprite set input or allocation limit");
    }
    AbSpriteSet *set = calloc(1, sizeof(*set));
    if (!set) {
        return ab_fail(error, "Cannot allocate sprite set owner");
    }
    if (count) {
        set->entries = calloc(count, sizeof(*set->entries));
        if (!set->entries) {
            ab_fail(error, "Cannot allocate sprite set entries");
            goto release_set;
        }
    }
    set->count = count;
    size_t used = sizeof(*set) + count * sizeof(*set->entries);
    for (size_t index = 0; index < count; ++index) {
        const AbZipEntry *entry =
            paths[index] ? ab_zip_find(archive, paths[index]) : NULL;
        if (!entry) {
            ab_fail(error, "Missing sprite set metadata entry");
            goto release_set;
        }
        SpriteEntry *item = set->entries + index;
        item->path = entry->name;
        if (!ab_zip_read(archive, (size_t)(entry - archive->entries),
                         allocation_limit - used, &item->bytes, error)) {
            goto release_set;
        }
        used += item->bytes.size;
        if (!ab_sprite_sheet_read((AbSpan){item->bytes.data, item->bytes.size},
                                  allocation_limit - used, &item->sheet, error)) {
            goto release_set;
        }
        used += item->sheet.count * sizeof(*item->sheet.sprites);
    }
    set->bytes = used;
    *output = set;
    return true;

release_set:
    ab_sprite_set_close(set);
    return false;
}

void ab_sprite_set_close(AbSpriteSet *set) {
    if (set) {
        for (size_t index = 0; index < set->count; ++index) {
            ab_sprite_sheet_free(&set->entries[index].sheet);
            ab_buffer_free(&set->entries[index].bytes);
        }
        free(set->entries);
        free(set);
    }
}

size_t ab_sprite_set_count(const AbSpriteSet *set) {
    return set ? set->count : 0;
}

size_t ab_sprite_set_bytes(const AbSpriteSet *set) {
    return set ? set->bytes : 0;
}

bool ab_sprite_set_sheet(const AbSpriteSet *set, size_t index,
                         AbSpriteSheetView *output) {
    if (!set || !output || index >= set->count) {
        return false;
    }
    const SpriteEntry *entry = set->entries + index;
    *output = (AbSpriteSheetView){entry->path, &entry->sheet};
    return true;
}

bool ab_sprite_set_find(const AbSpriteSet *set, AbSpan name, AbSpriteBinding *output) {
    if (!set || !output || (name.size && !name.data)) {
        return false;
    }
    for (size_t remaining = set->count; remaining; --remaining) {
        const SpriteEntry *entry = set->entries + remaining - 1;
        const AbSpriteRegion *region = ab_sprite_find(&entry->sheet, name);
        if (region) {
            *output = (AbSpriteBinding){entry->path, &entry->sheet, region};
            return true;
        }
    }
    return false;
}
