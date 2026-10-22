#include "ab/asset_profile.h"

#include <math.h>
#include <string.h>

static AbSpan text(const char *value) {
    return (AbSpan){(const uint8_t *)value, strlen(value)};
}

bool ab_android_splash_profile(float screen_width, float screen_height,
                               AbAssetSplashProfile *output, AbError *error) {
    if (!output || !isfinite(screen_width) || !isfinite(screen_height)) {
        return ab_fail(error, "Invalid Android splash dimensions/output");
    }
    float aspect = screen_width / screen_height;
    AbAssetSplashProfile result;
    if (isnan(aspect)) {
        return ab_fail(error, "Android splash aspect ratio has no resolution");
    }
    if (aspect <= 1.33f) {
        result = (AbAssetSplashProfile){{1024, 768}, text("1024x768_splash")};
    } else if (aspect <= 1.5f) {
        result = (AbAssetSplashProfile){{1440, 960}, text("1440x960_splash")};
    } else if (aspect <= 1.8f) {
        result = (AbAssetSplashProfile){{1024, 600}, text("1024x600_splash")};
    } else {
        result = (AbAssetSplashProfile){{1024, 550}, text("1024x550_splash")};
    }
    *output = result;
    return true;
}

static bool equal(AbSpan first, AbSpan second) {
    return first.size == second.size &&
           (!first.size || memcmp(first.data, second.data, first.size) == 0);
}

static bool member(const AbDataTable *groups, const char *name, AbSpan group,
                   bool *found, AbError *error) {
    if (!groups || groups->count > groups->capacity ||
        (groups->count && !groups->entries)) {
        return ab_fail(error, "Invalid asset-profile group catalog");
    }
    const AbDataValue *value = ab_data_get(
        groups, (AbDataValue){.kind = AB_DATA_STRING, .string = text(name)});
    if (!value || value->kind != AB_DATA_TABLE || !value->table ||
        value->table->count > value->table->capacity ||
        (value->table->count && !value->table->entries) ||
        value->table->count >= UINT32_C(16777216)) {
        return ab_fail(error, "Invalid asset-profile membership array");
    }
    *found = false;
    for (size_t index = 1; index <= value->table->count + 1; ++index) {
        float numeric_key = (float)index;
        uint32_t bits;
        memcpy(&bits, &numeric_key, sizeof(bits));
        const AbDataValue *entry = ab_data_get(
            value->table, (AbDataValue){.kind = AB_DATA_NUMBER, .bits = bits});
        if (!entry || entry->kind == AB_DATA_NIL) {
            return true;
        }
        if (entry->kind != AB_DATA_STRING ||
            (entry->string.size && !entry->string.data)) {
            return ab_fail(error, "Asset-profile membership is not a string");
        }
        if (equal(entry->string, group)) {
            *found = true;
            return true;
        }
    }
    return ab_fail(error, "Asset-profile membership array exceeds its entry count");
}

bool ab_android_asset_profile(const AbAssetProfileSource *source, AbSpan group,
                              AbSpan *output, AbError *error) {
    if (!source || !output || (group.size && !group.data) ||
        !isfinite(source->screen_width)) {
        return ab_fail(error, "Invalid Android asset-profile source");
    }
    if (source->adventure_mode) {
        const char *const lists[] = {"adventure_ingame", "adventure_1024x768",
                                     "adventure_themes"};
        const char *const profiles[] = {"adventure_ingame", "adventure_1024x768",
                                        "adventure_themes_android"};
        for (size_t index = 0; index < 3; ++index) {
            bool found;
            if (!member(source->asset_groups, lists[index], group, &found, error)) {
                return false;
            }
            if (found) {
                *output = text(profiles[index]);
                return true;
            }
        }
    }
    if (equal(group, text("SPLASHES"))) {
        if (source->splash_profile.size && !source->splash_profile.data) {
            return ab_fail(error, "Invalid resolved Android splash profile");
        }
        *output = source->splash_profile;
        return true;
    }
    if (equal(group, text("CUTSCENES")) || equal(group, text("CUTSCENE_BLACKBIRD")) ||
        equal(group, text("EGGDEFENDER_CUTSCENES"))) {
        *output = text(source->screen_width <= 1024 ? "480x320" : "1024x768");
        return true;
    }
    if (group.size >= 5 && memcmp(group.data, "THEME", 5) == 0) {
        *output = text("themes_android");
        return true;
    }
    bool base;
    if (!member(source->asset_groups, "base", group, &base, error)) {
        return false;
    }
    if (base) {
        *output = text("base");
        return true;
    }
    bool china =
        group.size >= 6 && memcmp(group.data + group.size - 6, "_CHINA", 6) == 0;
    *output =
        text(source->screen_width <= 480 ? (china ? "480x320_china" : "480x320")
                                         : (china ? "1024x768_china" : "1024x768"));
    return true;
}

bool ab_android_default_profile(float screen_width, AbVec2 *output) {
    if (!output || !isfinite(screen_width)) {
        return false;
    }
    *output = screen_width <= 480 ? (AbVec2){480, 320} : (AbVec2){1024, 768};
    return true;
}
