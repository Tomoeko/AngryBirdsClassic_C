#ifndef AB_UI_GAME_HUD_SERVICES_H
#define AB_UI_GAME_HUD_SERVICES_H

#include "ab/ui_eagle_services.h"
#include "ab/ui_game_hud_controls.h"
#include "ab/settings_statistics.h"

typedef struct {
    AbUiEagleServices *eagle;
    AbSettingsStatistics *local_settings;
    void *context;
    /* Reached ShopOffer.shouldShowOfferAdOnLevelStart requires its actual
     * initialized owner. No absent-offer, network or SDK response is supplied. */
    bool (*offer_on_start)(void *context, AbDataValue *output, AbError *error);
} AbUiGameHudServices;

/* Ordinary GameHud predicates over the same canonical globals, annotated
 * episode/page request, settings and purchases used by Eagle. Preserve raw
 * false/nil/zero values and source short circuits. The current level name must
 * match the retained resolved request when getLevelById/getLevelType is reached.
 * Classification covers that request's real pagelist record; separate bonus
 * registrations and overridden script methods are outside this domain.
 * No allocation, generated unlocks, persistence, VM or gameplay effects. */
bool ab_ui_game_hud_services_query(void *context, AbUiGameHudControls *controls,
                                   AbUiGameHudQuery query, AbUiPointerValue *output,
                                   AbError *error);

/* SettingsWrapper.isAudioEnabled30590: nil-only initialization in the actual
 * owned localSettings root, then source truthiness. Direct PausePage callback;
 * no persistence or audio device output. Eagle must borrow that same root. */
bool ab_ui_game_hud_services_audio(void *context, bool *output, AbError *error);

#endif
