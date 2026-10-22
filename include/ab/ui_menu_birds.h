#ifndef AB_UI_MENU_BIRDS_H
#define AB_UI_MENU_BIRDS_H

#include "ab/episode_statistics.h"
#include "ab/settings_statistics.h"
#include "ab/random.h"
#include "ab/ui_widget.h"
#include "ab/sprite_provider.h"

#define AB_UI_MENU_BIRD_CAPACITY 128

typedef struct {
    AbSpan sprite;
    float scale;
    unsigned reward;
} AbUiMenuBirdCandidate;

typedef struct {
    AbSpan sprite;
    uint8_t yell[160];
    size_t yell_size;
    AbVec2 position, velocity;
    float angle, angle_speed, scale, sprite_height;
    unsigned layer, reward;
} AbUiMenuBirdActor;

typedef struct {
    AbUiMenuBirdCandidate candidates[AB_UI_MENU_BIRD_CAPACITY];
    AbUiMenuBirdActor actors[AB_UI_MENU_BIRD_CAPACITY];
    AbDataEntry tutorials[AB_UI_MENU_BIRD_CAPACITY];
    AbRandom48 *random;
    size_t candidate_count, actor_count;
    bool initialized;
} AbUiMenuBirds;

/* Source MainMenuRoot's shared bird/reward lists and animate/draw methods.
 * All storage belongs to this stable owner. Build reads actual settings pairs
 * in native dictionary order, the real catalog/highscores and additional feather
 * episode IDs; it creates tutorials only for nil. Rebuild candidates at entry
 * without clearing live actors. Device/retina inputs are resolved by the caller.
 * No invented unlocks, reward counts, settings persistence or source execution.
 *
 * Initialize fresh zeroed storage without RNG effects. Animation uses the shared script
 * stream in recovered call order. Capacity is an explicit implementation bound,
 * not a recovered game limit. Used values are finite; integer RNG arguments must
 * fit signed32. Failures retain earlier settings/RNG/actor/render effects and
 * require the caller to stop. Views expire on rebuild/update; sprite names borrow
 * their original settings/catalog owners. No allocation or resource I/O. */
bool ab_ui_menu_birds_init(AbUiMenuBirds *owner, AbRandom48 *random, AbError *error);
bool ab_ui_menu_birds_build(AbUiMenuBirds *owner, AbSettingsStatistics *settings,
                            const AbEpisodeStatistics *statistics,
                            const AbDataTable *additional_feather_episodes,
                            AbVec2 screen, AbSpan device_model, bool retina_ipad,
                            AbError *error);
bool ab_ui_menu_birds_update(AbUiMenuBirds *owner, AbUiWidgetContext *context,
                             AbVec2 screen, float seconds, AbError *error);
bool ab_ui_menu_birds_draw(AbUiMenuBirds *owner, AbUiWidgetContext *context,
                           const AbSpriteProvider *sprites, AbVec2 screen,
                           AbError *error);

#endif
