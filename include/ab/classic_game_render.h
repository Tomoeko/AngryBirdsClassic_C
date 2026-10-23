#ifndef AB_CLASSIC_GAME_RENDER_H
#define AB_CLASSIC_GAME_RENDER_H

#include "ab/classic_game_draw.h"
#include "ab/classic_playfield.h"

typedef struct AbClassicGameRender AbClassicGameRender;

typedef struct {
    AbClassicPlayfield *playfield;
    AbClassicGamePowerups *powerups;
    const AbParticles *particles;
    const AbSpriteProvider *provider;
    const AbDataTable *theme, *settings, *extra_draw_functions;
    AbExtraDraws *extra_draws;
    const AbDataValue *physics_to_world;
    AbClassicGameDrawState *state;
    AbSpan basic_font, *current_font;
    uint32_t white_texture;
    size_t layers_bytes, sling_bytes, storage_bytes, placement_limit;
    void *context;
    bool (*editor)(void *context, AbDrawList *list, AbError *error);
    bool (*popup)(void *context, AbDataValue page, AbDrawList *list, AbError *error);
    bool (*restricted_borders)(void *context, AbDrawList *list, AbError *error);
} AbClassicGameRenderConfig;

/* Own cached theme layers, source render state, ordinary sling draw and drawGame
 * over the actual preloaded resource registry. Borrow all configured resources,
 * canonical fields and game owners until close. White texture identifies the
 * caller's prepared white image; open performs no decoding, uploads or frame draws.
 * Keep the initialized draw method at its original controller boundary and
 * submit commands through the shared Metal/GLES2 renderer afterward. */
bool ab_classic_game_render_open(AbClassicGameRenderConfig config,
                                 AbClassicGameRender **output, AbError *error);
void ab_classic_game_render_close(AbClassicGameRender *render);
AbClassicGameDraw *ab_classic_game_render_draw(AbClassicGameRender *render);
const AbThemeLayerRenderState *
ab_classic_game_render_state(const AbClassicGameRender *render);

#endif
