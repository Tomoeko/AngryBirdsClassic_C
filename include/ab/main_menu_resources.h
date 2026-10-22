#ifndef AB_MAIN_MENU_RESOURCES_H
#define AB_MAIN_MENU_RESOURCES_H

#include "ab/lua_data.h"
#include "ab/main_menu_settings.h"
#include "ab/main_menu_theme.h"
#include "ab/sprite_set.h"
#include "ab/sprite_provider.h"

typedef struct {
    const AbDataTable *themes;
    const AbSpriteSet *sprites;
    AbMainMenuSettings *settings;
    float screen_height, scroll;
    void *context;
    bool (*call)(void *context, AbMainMenuThemeCall operation, AbSpan theme,
                 const float *numbers, size_t count, AbError *error);
} AbMainMenuResources;

/* Compose verified settings/theme layout with borrowed static APK theme tables
 * and explicitly selected SPRT metadata. themes has the selected APK chunk's
 * settings and named records; an absent defaultTheme uses the source fallback.
 * Only plain stable string-keyed resource tables are supported. Non-text saved
 * keys have no match.
 * The caller opens resources once and keeps all owners/settings/text alive.
 * The no-argument zoom setter clears the saved zoom before reading its default.
 * Rendering/camera/effect application remains the explicit call boundary.
 * No allocation, image decoding, group selection, script VM or GPU calls. */
bool ab_main_menu_resources_layout(AbMainMenuThemeState *state,
                                   const AbMainMenuResources *resources,
                                   float *camera_top, AbError *error);
/* Live active resource-registry variant. resources.sprites is unused; provider
 * supplies ordinary/composite bounds, including native zero for absent images. */
bool ab_main_menu_resources_layout_provider(AbMainMenuThemeState *state,
                                            const AbMainMenuResources *resources,
                                            const AbSpriteProvider *provider,
                                            float *camera_top, AbError *error);

#endif
