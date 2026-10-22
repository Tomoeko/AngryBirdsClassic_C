#include "ab/asset_groups.h"

#include <string.h>

/* PathName scans dot pairs even within a component. Preserve its backward
 * separator count and skipped following byte; imposing filesystem segment
 * semantics here would change native resource keys. */
static size_t normalize_dots(char *path, size_t size) {
    size_t position = 0;
    while (position + 1 < size) {
        bool parent = path[position] == '.' && path[position + 1] == '.';
        bool current = path[position] == '.' && path[position + 1] == '/';
        if (!parent && !current) {
            ++position;
            continue;
        }
        if (parent && (position + 2 == size || !position)) {
            break;
        }
        size_t cursor = position;
        unsigned separators = parent ? 2u : 1u;
        while (cursor) {
            --cursor;
            if (path[cursor] == '/' && !--separators) {
                ++cursor;
                break;
            }
        }
        size_t source = position + (parent ? 3u : 2u);
        memmove(path + cursor, path + source, size - source + 1);
        size -= source - cursor;
        position = cursor;
    }
    return size;
}

static size_t basename(char *path, size_t size) {
    size = normalize_dots(path, size);
    size_t first = 0, end = size;
    for (size_t index = 0; index < size; ++index) {
        if (path[index] == '/') {
            first = index + 1;
            end = size;
        } else if (path[index] == '.') {
            end = index;
        }
    }
    size_t length = end - first;
    memmove(path, path + first, length);
    path[length] = 0;
    return length;
}

bool ab_asset_sheet_key(AbSpan path, char *output, size_t capacity, size_t *length,
                        AbError *error) {
    if (!output || !length || !capacity || path.size > 255 ||
        (path.size && !path.data)) {
        return ab_fail(error, "Invalid sprite sheet key input/output bounds");
    }
    for (size_t index = 0; index < path.size; ++index) {
        if (!path.data[index] || path.data[index] == '\\' || path.data[index] == ':') {
            return ab_fail(error, "Sprite sheet key requires a forward APK path");
        }
    }
    char work[256];
    if (path.size) {
        memcpy(work, path.data, path.size);
    }
    work[path.size] = 0;
    size_t size = basename(work, path.size);
    size = basename(work, size);
    if (size >= capacity) {
        return ab_fail(error, "Sprite sheet key exceeds output capacity");
    }
    memcpy(output, work, size + 1);
    *length = size;
    return true;
}
