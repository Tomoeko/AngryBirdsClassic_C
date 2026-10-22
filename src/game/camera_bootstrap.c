#include "ab/camera_bootstrap.h"

#include <math.h>
#include <string.h>

bool ab_camera_snap_fit(const AbCameraSnap *authored, AbVec2 pixel_size,
                        AbCameraSnap *output) {
    if (!authored || !output) {
        return false;
    }
    AbCameraViewportInput input = {
        authored->position, {1024, 768}, {0, 0}, authored->zoom};
    AbCameraViewport original;
    if (!ab_camera_viewport(&input, &original) || !isfinite(pixel_size.x) ||
        !isfinite(pixel_size.y)) {
        return false;
    }
    float coverage = original.bottom_right.x - original.top_left.x;
    if (!isfinite(coverage) || coverage == 0) {
        return false;
    }
    AbCameraSnap result = *authored;
    result.zoom = pixel_size.x / coverage;
    if (!isfinite(result.zoom) || result.zoom == 0) {
        return false;
    }
    float half_height = pixel_size.y * 0.5f;
    float radius = half_height / result.zoom;
    result.position.y = original.bottom_right.y - radius;
    if (!isfinite(radius) || !isfinite(result.position.y)) {
        return false;
    }
    *output = result;
    return true;
}

static const AbDataValue *field(const AbDataTable *table, const char *name) {
    AbDataValue key = {.kind = AB_DATA_STRING,
                       .string = {(const uint8_t *)name, strlen(name)}};
    return table ? ab_data_get(table, key) : NULL;
}

static const AbDataTable *child(const AbDataTable *table, const char *name) {
    const AbDataValue *value = field(table, name);
    return value && value->kind == AB_DATA_TABLE ? value->table : NULL;
}

static bool numeric(const AbDataTable *table, const char *name, float *output) {
    const AbDataValue *value = field(table, name);
    if (!value || value->kind != AB_DATA_NUMBER) {
        return false;
    }
    memcpy(output, &value->bits, sizeof(*output));
    return isfinite(*output);
}

bool ab_camera_museum_snaps(const AbDataTable *level, AbVec2 pixel_size,
                            AbCameraSnap output[2], AbError *error) {
    if (!level || !output) {
        return ab_fail(error, "Missing museum camera metadata or output");
    }
    const char *names[] = {"birdCameraData", "castleCameraData"};
    AbCameraSnap result[2];
    for (size_t index = 0; index < 2; ++index) {
        const AbDataTable *profile = child(child(level, names[index]), "ipad");
        AbCameraSnap authored;
        if (!numeric(profile, "px", &authored.position.x) ||
            !numeric(profile, "py", &authored.position.y) ||
            !numeric(profile, "sx", &authored.zoom) ||
            !ab_camera_snap_fit(&authored, pixel_size, result + index)) {
            return ab_fail(error, "Unsupported museum camera snap profile");
        }
    }
    memcpy(output, result, sizeof(result));
    return true;
}
