#ifndef AB_EXTRA_DRAW_H
#define AB_EXTRA_DRAW_H

#include "ab/coordinates.h"
#include "ab/draw.h"

typedef struct AbExtraDraws AbExtraDraws;

typedef struct {
    AbCoordinateCamera camera;
    uint32_t width, height;
    AbDrawList *list;
    /* Optional source-global write, before callbacks can observe the flag. */
    void *state_context;
    void (*publish_has_z)(void *context, bool present);
} AbExtraDrawInput;

typedef struct {
    void *context, *parameter;
    bool (*draw)(void *context, void *parameter, AbExtraDrawInput input,
                 AbError *error);
    float z;
    bool z_present, before_blocks;
} AbExtraDrawFunction;

/* Own the recovered g_extra_draw_functions string-key registry and a bounded
 * per-frame sorted-reference array. nodes is a power of two or zero. Keys and
 * callback records remain borrowed and stable until close, including records
 * removed after frame preparation. Open has the actual empty-table state.
 * Numeric z=0 remains present. No allocation in registration or frame phases.
 * Finite numeric z, callable records and string keys only. */
bool ab_extra_draws_open(size_t nodes, size_t storage_bytes, AbExtraDraws **output,
                         AbError *error);
void ab_extra_draws_close(AbExtraDraws *draws);
bool ab_extra_draws_set(AbExtraDraws *draws, AbSpan name,
                        const AbExtraDrawFunction *function, AbError *error);
bool ab_extra_draws_remove(AbExtraDraws *draws, AbSpan name, AbError *error);
/* Borrow a registered record; a missing/nil entry returns NULL. */
bool ab_extra_draws_find(const AbExtraDraws *draws, AbSpan name,
                         const AbExtraDrawFunction **output, AbError *error);

/* Source drawGame preparation: pairs in the native string-map order, capture
 * z records, immediately call non-z beforeBlocks records, then native numeric
 * sort. A callback may add/remove entries; resume after the retained key in the
 * current table, including native invalid-next failure. No registry replacement
 * during a frame. Success begins a frame that finish must close. */
bool ab_extra_draws_begin(AbExtraDraws *draws, AbExtraDrawInput input, bool *has_z,
                          AbError *error);
/* Camera.drawGame calls this before each actual scene layer. Scan the captured
 * list and remove each z<=layer after invoking it; records are live references.
 * Calls may remove registrations without invalidating those captured records. */
bool ab_extra_draws_layer(AbExtraDraws *draws, float layer, AbExtraDrawInput input,
                          AbError *error);
/* Source late phase: flush z<=1e10, then pairs over the current registry and call
 * each non-z, non-beforeBlocks record. Earlier callback effects survive failure;
 * a failed frame stops this owner. No implicit render-state changes. */
bool ab_extra_draws_finish(AbExtraDraws *draws, AbExtraDrawInput input, AbError *error);

#endif
