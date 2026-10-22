#ifndef AB_THEME_ASSETS_H
#define AB_THEME_ASSETS_H

#include "ab/lua_data.h"

typedef bool (*AbThemeAssetHost)(void *context, AbSpan group, bool loading,
                                 AbError *error);

typedef struct {
    AbDataTable *current;
    AbDataTable wrapped[2];
    AbDataEntry wrapped_entries[2][2];
    AbSpan *loads, *releases;
    size_t capacity;
    bool busy, failed;
} AbThemeAssets;

/* Caller-owned state and 2*capacity disjoint scratch spans. Current must be a
 * dense, string-valued numeric array; empty is valid. Keep this owner at a stable
 * address. External tables/strings remain borrowed through their last use as
 * current. Wrapped string-list views expire on the next successful transition.
 * No allocation, decoding, resource ownership or GPU work. Failure preserves
 * output. Reinitialization requires retiring prior views. */
bool ab_theme_assets_init(AbDataTable *current, AbSpan *scratch, size_t capacity,
                          AbThemeAssets *output, AbError *error);

/* Recovered loadThemeGraphics boundary: graphic_set is a string (wrapped into an
 * owned one-element table) or a borrowed mutable table. show_editor is resolved
 * Lua truthiness. When true, append the episode-red-bird group to that table,
 * preserving duplicates and aliasing with current; external tables need a free
 * entry slot. Compute both differences before callbacks, release old-only members,
 * assign current, then load new-only members. Differences preserve duplicate
 * values and numeric iteration order. Host failure retains the editor append,
 * callback prefix and current assignment, marks failed, and prohibits further
 * transitions. Validation/capacity failures precede visible changes. Callbacks
 * may inspect current but must not mutate owner/tables, retain scratch views or
 * reenter. Serialize transitions. Sparse/hash-keyed arrays and other value kinds
 * reject explicitly; they are outside the recovered target theme-list domain.
 * Theme-name selection and loadAllThemeGraphics traversal remain separate. */
bool ab_theme_assets_transition(AbThemeAssets *assets, AbDataValue graphic_set,
                                bool show_editor, AbThemeAssetHost host, void *context,
                                AbError *error);

#endif
