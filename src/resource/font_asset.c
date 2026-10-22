#include "ab/font_asset.h"

#include <stdlib.h>
#include <string.h>

static bool read_entry(const AbZip *archive, const char *path, size_t limit,
                       AbBuffer *output, AbError *error) {
    const AbZipEntry *entry = ab_zip_find(archive, path);
    return entry ? ab_zip_read(archive, (size_t)(entry - archive->entries), limit,
                               output, error)
                 : ab_fail(error, "Missing APK font entry: %s", path);
}

static bool texture_path(const char *metadata_path, AbSpan name, char **output,
                         AbError *error) {
    const char *slash = strrchr(metadata_path, '/');
    size_t prefix = slash ? (size_t)(slash - metadata_path) + 1 : 0;
    if (!name.size || !name.data || memchr(name.data, 0, name.size) ||
        memchr(name.data, '/', name.size) || memchr(name.data, '\\', name.size) ||
        prefix == SIZE_MAX || name.size > SIZE_MAX - prefix - 1)
        return ab_fail(error, "Invalid relative font texture name");
    char *path = malloc(prefix + name.size + 1);
    if (!path)
        return ab_fail(error, "Cannot allocate font texture path");
    memcpy(path, metadata_path, prefix);
    memcpy(path + prefix, name.data, name.size);
    path[prefix + name.size] = 0;
    *output = path;
    return true;
}

static bool decode_texture(AbSpan bytes, AbSpan name, size_t limit, AbFontAsset *asset,
                           AbError *error) {
    if (name.size >= 4 && !memcmp(name.data + name.size - 4, ".pvr", 4))
        return ab_pvr_has_alpha(bytes, &asset->has_alpha, error) &&
               ab_pvr_decode(bytes, limit, &asset->image, error);
    if (name.size >= 4 && !memcmp(name.data + name.size - 4, ".png", 4)) {
        AbPngInfo info;
        if (!ab_png_read_info(bytes, &info, error))
            return false;
        asset->has_alpha =
            info.color_type == 4 || info.color_type == 6 || info.has_transparency;
        return ab_png_decode(bytes, limit, &asset->image, error);
    }
    return ab_fail(error, "Unsupported APK font texture format");
}

bool ab_font_asset_open(const AbZip *archive, const char *metadata_path,
                        size_t allocation_limit, size_t entry_limit,
                        AbFontAsset *output, AbError *error) {
    if (!output)
        return ab_fail(error, "Missing APK font asset output");
    *output = (AbFontAsset){0};
    if (!archive || !metadata_path || !allocation_limit || !entry_limit)
        return ab_fail(error, "Invalid APK font asset input/limits");
    AbFontAsset asset = {0};
    AbBuffer encoded = {0};
    char *path = NULL;
    bool okay = false;
    size_t metadata_limit =
        entry_limit < allocation_limit ? entry_limit : allocation_limit;
    if (!read_entry(archive, metadata_path, metadata_limit, &asset.metadata, error) ||
        !ab_font_read((AbSpan){asset.metadata.data, asset.metadata.size},
                      allocation_limit - asset.metadata.size, &asset.font, error) ||
        !texture_path(metadata_path, asset.font.texture, &path, error) ||
        !read_entry(archive, path, entry_limit, &encoded, error))
        goto release;
    size_t retained =
        asset.metadata.size + asset.font.count * sizeof(*asset.font.glyphs);
    if (!decode_texture((AbSpan){encoded.data, encoded.size}, asset.font.texture,
                        allocation_limit - retained, &asset, error))
        goto release;
    *output = asset;
    asset = (AbFontAsset){0};
    okay = true;

release:
    free(path);
    ab_buffer_free(&encoded);
    ab_font_asset_close(&asset);
    return okay;
}

void ab_font_asset_close(AbFontAsset *asset) {
    if (!asset)
        return;
    ab_image_free(&asset->image);
    ab_font_free(&asset->font);
    ab_buffer_free(&asset->metadata);
    *asset = (AbFontAsset){0};
}
