#ifndef AB_CLASSIC_GAME_DRAW_H
#define AB_CLASSIC_GAME_DRAW_H

#include "ab/classic_game_powerups.h"
#include "ab/classic_sling_draw.h"
#include "ab/classic_render_scene.h"
#include "ab/particle_draw.h"
#include "ab/theme_draw.h"

typedef struct {
    AbDataValue background_overlay, level_particles_enabled, current_theme;
    AbDataValue has_extra_z_ordered_draws, zoom_level, old_zoom_level;
    AbDataValue eagle_darkness, popup_page;
    AbDataValue current_game_mode, editor_mode;
    AbDataValue use_restricted_aspect_ratio, updated_width, updated_height;
} AbClassicGameDrawState;

typedef struct {
    AbClassicLevel *level;
    AbClassicVisuals *visuals;
    AbClassicBehaviors *behaviors;
    AbClassicRenderScene *scene;
    AbClassicCamera *camera;
    AbClassicSlingDraw *sling;
    AbClassicGamePowerups *powerups;
    const AbParticles *particles;
    const AbSpriteProvider *provider;
    AbThemeDraw *theme;
    const AbDataTable *theme_definition, *local_settings, *extra_draw_functions;
    AbClassicGameDrawState *state;
    AbSpan basic_font;
    AbSpan *current_font;
    void *context;
    /* Actual selected services, required only when their source gates reach.
     * Editor/popup/restricted-viewport content is not replaced by empty work. */
    bool (*editor)(void *context, AbDrawList *list, AbError *error);
    bool (*popup)(void *context, AbDataValue page, AbDrawList *list, AbError *error);
    bool (*restricted_borders)(void *context, AbDrawList *list, AbError *error);
} AbClassicGameDrawConfig;

typedef struct {
    AbClassicGameDrawConfig config;
    bool initialized, drawing, failed;
} AbClassicGameDraw;

/* Actual ordinary drawGame141561 assembly, using real theme, sling, scene,
 * particles, powerup and behavior owners. Borrow all objects/typed globals and
 * cached resources until the last call. Initialize has no drawing/state effects.
 * No frame allocation, clock tick, particle update or GPU submission.
 *
 * Fixed initial world identities and the explicitly resolved empty extra-draw
 * registry only. Nonempty extra/z draws reject at their reached source phase;
 * dynamic insertion, powered class construction, editor, popup and restricted
 * viewport rendering require their own producers. This is not root update,
 * MenuManager/GameHud drawing or a complete native frame/session comparison.
 * Earlier commands and global effects survive failure; stop the owner.
 */
bool ab_classic_game_draw_initialize(AbClassicGameDrawConfig config,
                                     AbClassicGameDraw *output, AbError *error);
/* Append to the caller's list; do not reset it. Read current camera and raw
 * quality/particle/theme gates at their original boundaries. time supplies theme
 * animation only, from the caller's already published script clock. */
bool ab_classic_game_draw(AbClassicGameDraw *draw, uint32_t width, uint32_t height,
                          float time, AbDrawList *list, AbError *error);

#endif
