#ifndef AB_CLASSIC_LEVEL_REQUEST_H
#define AB_CLASSIC_LEVEL_REQUEST_H

#include "ab/lua_data.h"

typedef struct {
    const AbDataTable *episode, *page, *level;
    AbDataValue episode_id, page_number, level_index, world_number, name;
    float current_level_number, current_level_number_in_theme;
    char resource_path[1024];
} AbClassicLevelRequest;

/* Resolve canonical level-button metadata against the annotated episode graph.
 * Recover prepareLevel28689's selected fields/path and getLevelNumber25101's
 * page-local result. The source page-offset loop repeatedly reads the selected
 * page's length; retain that behavior even when preceding page lengths differ.
 *
 * The path is the source file candidate; callers must dispatch the selected mode
 * and verify existence. Five special soundboard entries have no such level file.
 * No loading, attempt counters, events, editing/physics flags or bootstrap run.
 * Borrow catalog records and strings until the result's last use. Own only the
 * fixed path buffer; success commits the whole output, failure preserves it.
 * This domain requires real dense catalog pages, positive integral indexes and
 * a matching canonical name. Duplicate names with differing page-local indexes
 * need the native episode iteration order and reject rather than choosing one.
 * Separately registered bonus levels require their own launch metadata binding.
 * No allocation, VM, path probing or original executable code. */
bool ab_classic_level_request_resolve(const AbDataTable *episodes,
                                      const AbDataTable *metadata,
                                      AbClassicLevelRequest *output, AbError *error);

#endif
