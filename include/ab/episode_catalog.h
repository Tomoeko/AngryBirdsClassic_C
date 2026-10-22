#ifndef AB_EPISODE_CATALOG_H
#define AB_EPISODE_CATALOG_H

#include "ab/lua_data.h"
#include "ab/target_resources.h"

typedef struct AbEpisodeCatalog AbEpisodeCatalog;

typedef struct {
    AbSpan device_model;
    bool disable_external_links, apply_china_restrictions, freemium_china;
} AbEpisodeCatalogConfig;

/* Own the pinned episodes.lua constructor graph and its source feature edits.
 * Decode literal fragments as data, then compose the Golden Egg link and the
 * explicit Android/China/device branches in first-party C. No original function,
 * script call, branch or loop executes. Flags are resolved source Booleans, not
 * inferred defaults. This is the catalog immediately after episodes.lua; later
 * level-index annotation, unlock/settings mutations and localization are separate.
 * Bound all retained bytes, parsed metadata and tables together. Archive/key and
 * config text borrow only during open; output starts NULL on every failure. */
bool ab_episode_catalog_open(const AbZip *archive, AbSpan key,
                             AbEpisodeCatalogConfig config, size_t allocation_limit,
                             AbEpisodeCatalog **output, AbError *error);
/* Apply the recovered startup level annotations to this owned graph: page and
 * episode/index links, progression indexes, decimal display numbers and ordered
 * regular/bonus name lists. Preserve every existing table identity. The initial
 * raw constructor graph remains available until this explicit call. Repeated
 * successful calls retain the completed graph and allocate nothing. Retained
 * strings/lists share the original catalog budget. A failed call may retain a
 * mutation prefix; discard that catalog. No VM or frame-time resource work. */
bool ab_episode_catalog_annotate_levels(AbEpisodeCatalog *catalog, AbError *error);
bool ab_episode_catalog_levels_annotated(const AbEpisodeCatalog *catalog);
void ab_episode_catalog_close(AbEpisodeCatalog *catalog);
const AbDataTable *ab_episode_catalog_episodes(const AbEpisodeCatalog *catalog);
const AbDataTable *ab_episode_catalog_ids(const AbEpisodeCatalog *catalog);
const AbDataTable *ab_episode_catalog_feather_ids(const AbEpisodeCatalog *catalog);
size_t ab_episode_catalog_bytes(const AbEpisodeCatalog *catalog);

#endif
