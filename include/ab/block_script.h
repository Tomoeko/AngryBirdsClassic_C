#ifndef AB_BLOCK_SCRIPT_H
#define AB_BLOCK_SCRIPT_H

#include "ab/block_event.h"
#include "ab/data_order.h"

typedef struct AbBlockScript AbBlockScript;

struct AbBlockScript {
    AbBlockEventScript events;
    bool (*can_apply)(AbBlockScript *script, const AbDataTable *definition,
                      bool *applicable, AbError *error);
    bool (*initialize)(AbBlockScript *script, AbError *error);
    bool (*apply)(AbBlockScript *script, AbBlockEventObject *object,
                  const AbDataTable *definition, AbError *error);
    bool (*dispose)(AbBlockScript *script, AbError *error);
    bool (*update)(AbBlockScript *script, float seconds, float time, AbError *error);
};

typedef struct {
    AbBlockScript **items;
    size_t count, capacity;
} AbBlockScriptList;

typedef struct {
    AbSpan name;
    AbBlockEventObject *object;
} AbTrackedBlockScriptObject;

typedef struct {
    AbBlockScriptList registered;
    AbBlockScriptList initialized;
    AbTrackedBlockScriptObject *tracked;
    size_t count, capacity;
} AbBlockScriptManager;

typedef struct {
    AbDataEntry *entries;
    size_t entry_capacity;
    AbStringOrderWorkspace strings;
    AbBlockEventObject **objects;
    size_t object_capacity;
    size_t count, hash_capacity;
} AbBlockScriptOrder;

typedef struct {
    AbBlockEventObject *const *objects;
    size_t count;
    AbBlockEventHost host;
    AbSpan event;
} AbBlockScriptUpdate;

/* All manager/list/object storage is caller-owned, initially empty, bounded and
 * stable. Script callbacks use events.context as their shared first-party state.
 * Names/string bytes remain borrowed. Register in native insertion order;
 * duplicate registrations are allowed, but initialization is once per script
 * identity until clear. A callable can_apply is required. No Lua or allocation.
 */
bool ab_block_script_register(AbBlockScriptManager *manager, AbBlockScript *script,
                              AbError *error);

/* Resolve the raw definition once, before preparing the object's script list.
 * For each registered script, test applicability; mark first initialization
 * before its optional callback, append its event view, run optional apply, then
 * track the object by name. The cached definition is retained across callbacks.
 * Existing attachments are preserved, including duplicate applications. Object
 * capacity bounds its attachment storage. Stable names/membership only; adding
 * or reinserting tracked keys after pruning rejects until map growth/deletion
 * is recovered. Later failure retains preceding metadata/callback changes.
 */
bool ab_block_scripts_apply(AbBlockScriptManager *manager, AbBlockEventObject *object,
                            AbBlockEventHost host, AbError *error);

/* Original BlockScriptManager.update: visit the dense registered list, calling
 * each currently present update(self, seconds, time), including duplicates and
 * scripts not initialized/applied to any object. Then read the current global
 * view/event and dispatch the same numeric arguments. Callbacks may replace
 * later methods or append bounded registrations; appended entries participate
 * in this pass. List storage/capacity remain stable and no entries are removed
 * during traversal. The global event is normally onUpdateBlock. Numeric bits
 * are forwarded unchanged; no allocation or Lua. A later failure retains
 * preceding callback changes and prevents subsequent phases.
 */
bool ab_block_scripts_update(AbBlockScriptManager *manager, AbBlockScriptUpdate *update,
                             float seconds, float time, AbError *error);

/* Recover initial growing string-map order for event traversal. Retain pruned
 * entries as inactive slots, preserving the original remaining traversal.
 * Scratch may change on failure; output count/hash capacity commit on success.
 */
bool ab_block_script_order(const AbBlockScriptManager *manager,
                           AbBlockScriptOrder *order, AbError *error);

/* Dispose initialized scripts in an explicit native set-order permutation,
 * then drop active/initialized membership. Registration and object attachment
 * lists survive. Pointer-key set-order recovery is separate. A missing required
 * dispose or later callback error retains all membership and earlier effects.
 * Membership must stay stable during disposal. No allocation or implicit order.
 */
bool ab_block_scripts_clear(AbBlockScriptManager *manager, const size_t *dispose_order,
                            size_t count, AbError *error);

#endif
