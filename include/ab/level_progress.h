#ifndef AB_LEVEL_PROGRESS_H
#define AB_LEVEL_PROGRESS_H

#include "ab/lua_data.h"

typedef struct {
    const AbDataTable *episodes, *highscores;
    bool release_build, freemium_china;
    void *context;
    bool (*purchased_levels)(void *context, AbDataValue episode,
                             const AbDataTable **output, AbError *error);
} AbLevelProgress;

/* settingsWrapper.isLevelOpen: previous level in the page, or the preceding
 * page's final level. Read completed in the regular profile and cleared in the
 * China profile. Missing predecessors/highscores return false. Tables and
 * annotated catalog metadata remain borrowed; dense arrays are required.
 * No unlocks, purchases, save mutations, script calls or allocations occur. */
bool ab_level_progress_previous(const AbLevelProgress *progress,
                                const AbDataTable *level, bool *output, AbError *error);
/* LSGridLayout.isLevelOpen adds source first-level/unlocked and release gates,
 * then reaches the actual purchased-level service when the China branch needs
 * it. Never supply a successful purchase result when that boundary is absent.
 * This query is for the grid layout; special layouts own their distinct gates.
 * Failure preserves output and retains any reached host effects. */
bool ab_level_progress_grid_open(const AbLevelProgress *progress,
                                 const AbDataTable *level, bool *output,
                                 AbError *error);

#endif
