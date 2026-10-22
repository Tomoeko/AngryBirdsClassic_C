#ifndef AB_ASSET_PROFILE_H
#define AB_ASSET_PROFILE_H

#include "ab/lua_data.h"
#include "ab/physics_geometry.h"

typedef struct {
    const AbDataTable *asset_groups;
    float screen_width;
    bool adventure_mode;
    AbSpan splash_profile;
} AbAssetProfileSource;

/* Android 5.2.0 profile selection, independent of the desktop graphics backend.
 * Caller supplies the original AssetGroup data, resolved adventure-mode result,
 * logical screen width and resolved splash helper result. Membership arrays
 * used by the selected branch must be typed contiguous string arrays. Native
 * splash sizing and bundled profile expansion are separate operations.
 * Return a static directory name, or borrow splash_profile for SPLASHES. Input
 * group/data strings remain borrowed during this call. No allocation or path
 * normalization occurs. Invalid used input preserves output. */
bool ab_android_asset_profile(const AbAssetProfileSource *source, AbSpan group,
                              AbSpan *output, AbError *error);

/* Profiles.getDefaultProfile chooses 480x320 at width <=480, otherwise 1024x768.
 * Finite logical widths are supported; invalid input preserves output. */
bool ab_android_default_profile(float screen_width, AbVec2 *output);

typedef struct {
    AbVec2 resolution;
    AbSpan profile;
} AbAssetSplashProfile;

/* Select the original Android splash size/directory by binary32 width/height
 * aspect ratio. Finite dimensions, including signed zero, are supported; a NaN
 * ratio has no native resolution and cannot produce a profile. The directory is
 * static. No allocation; invalid input/undefined ratio preserves output. */
bool ab_android_splash_profile(float screen_width, float screen_height,
                               AbAssetSplashProfile *output, AbError *error);

typedef struct {
    AbDataTable profiles, bundles[6];
    AbDataEntry entries[6], members[6][3];
} AbAndroidAssetBundles;

/* Own the Android profile module's bundle tables in caller storage, using
 * resolved isFreemiumChina/Sugotoku flags. This first-party constructor models
 * only named bundles; functions, requireFile and other platform state are
 * separate. Strings are static target resource identifiers. No allocation or
 * release. The owner must remain at its initialized address while views exist;
 * initialize again only after all borrowed views/membership names are retired. */
bool ab_android_asset_bundles(bool freemium_china, bool sugotoku,
                              AbAndroidAssetBundles *output);

#endif
