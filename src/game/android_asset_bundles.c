#include "ab/asset_profile.h"

#include <string.h>

static AbDataValue text(const char *value) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)value, strlen(value)}};
}

static void add_bundle(AbAndroidAssetBundles *owner, const char *name,
                       const char *first, const char *second, const char *third) {
    size_t index = owner->profiles.count++;
    AbDataTable *bundle = owner->bundles + index;
    *bundle = (AbDataTable){owner->members[index], 0, 3, NULL};
    const char *members[] = {first, second, third};
    for (size_t member = 0; member < 3 && members[member]; ++member) {
        float number = (float)(member + 1);
        AbDataValue key = {.kind = AB_DATA_NUMBER};
        memcpy(&key.bits, &number, sizeof(key.bits));
        bundle->entries[bundle->count++] = (AbDataEntry){key, text(members[member])};
    }
    owner->entries[index] =
        (AbDataEntry){text(name), {.kind = AB_DATA_TABLE, .table = bundle}};
}

bool ab_android_asset_bundles(bool freemium_china, bool sugotoku,
                              AbAndroidAssetBundles *output) {
    if (!output) {
        return false;
    }
    *output = (AbAndroidAssetBundles){0};
    output->profiles = (AbDataTable){output->entries, 0, 6, NULL};
    /* Profiles.lua applies Sugotoku's text override after the China branch. */
    const char *texts = sugotoku         ? "TEXTS_SUGOTOKU_CUSTOMISATION"
                        : freemium_china ? "TEXTS_CHINA_CUSTOMISATION"
                                         : "TEXTS_APPRATER";
    add_bundle(output, "TEXT_GROUP", "TEXTS_BASIC", texts, NULL);
    if (freemium_china) {
        add_bundle(output, "NEW_SHOP", "NEW_SHOP", "NEW_SHOP_CHINA",
                   "NEW_SHOP_SALE_BANNERS");
        add_bundle(output, "BUTTONS", "BUTTONS", "BUTTONS_CHINA", NULL);
        add_bundle(output, "SHOP_TUTORIALS", "SHOP_TUTORIALS", NULL, NULL);
        add_bundle(output, "INGAME", "INGAME", NULL, NULL);
        add_bundle(output, "SPLASHES", "SPLASHES", "SPLASHES_CHINA", NULL);
    } else {
        add_bundle(output, "NEW_SHOP", "NEW_SHOP", "NEW_SHOP_SALE_BANNERS", NULL);
    }
    return true;
}
