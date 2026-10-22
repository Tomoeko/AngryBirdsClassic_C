#ifndef AB_PROCESS_MANAGER_H
#define AB_PROCESS_MANAGER_H

#include "ab/event_manager.h"

typedef struct AbProcessManager AbProcessManager;

typedef struct {
    AbDataTable *table;
    void *context;
    bool (*action)(void *context, AbDataValue item, float elapsed, float duration,
                   float seconds, AbDataValue meta, AbError *error);
    /* Canonical resolved optional function field; NULL represents nil/false.
     * The callback has no original arguments, and may replace this binding. */
    bool (*on_finish)(void *context, AbError *error);
} AbProcessAction;

typedef enum {
    AB_PROCESS_RESET_LOG,
    AB_PROCESS_RESET_ONE_LOG,
} AbProcessLog;

typedef struct {
    AbEventManager *events;
    const AbDataValue *finished_event;
    AbProcessAction *const *bindings;
    size_t binding_count;
    size_t actions, generations, notifications, storage_bytes;
    void *context;
    bool (*log)(void *context, AbProcessLog kind, AbDataValue name, AbError *error);
} AbProcessManagerConfig;

/* ProcessManager.lua init866/start357/reset442/resetProcess1117/insert1534/
 * update1646. Own fixed state, bounded dense actions-table generations and
 * distinct retained event payloads. Prepared action tables/function bindings,
 * event manager/ID and init-info x/y values remain borrowed until close.
 * Open initializes name, elapsed0, running=false and a fresh actions table.
 * No process is started implicitly. No allocation during later operations.
 *
 * Static prepared action identities and dense numeric pairs traversal only.
 * Callbacks can append, start, reset or reset another process through this API;
 * captured old action tables remain alive after replacement. Do not reenter
 * update, recursively reset/resetProcess, free owners or edit numeric keys.
 * Function fields use the live callback slots above, not a Lua VM. Other
 * truthy noncallable fields and arbitrary replacement action tables reject.
 * Previous effects survive failure; stop this owner afterwards.
 */
bool ab_process_manager_open(AbProcessManagerConfig config, const AbDataTable *info,
                             AbProcessManager **output, AbError *error);
void ab_process_manager_close(AbProcessManager *manager);
size_t ab_process_manager_bytes(const AbProcessManager *manager);
AbDataTable *ab_process_manager_state(AbProcessManager *manager);
bool ab_process_manager_start(AbProcessManager *manager, AbError *error);
bool ab_process_manager_insert(AbProcessManager *manager, AbProcessAction *action,
                               AbError *error);
bool ab_process_manager_reset(AbProcessManager *manager, AbError *error);
bool ab_process_manager_reset_process(AbProcessManager *manager, AbDataValue id,
                                      AbError *error);
/* Only literal Boolean true running enters. Advance elapsed before skipInput
 * and callbacks. Completion is strict elapsed-start > duration and literal
 * finished=false. All-finished is sampled before each action's callbacks, so
 * finishing an initially false action delays automatic reset until next call.
 * Third original time argument is unused and omitted here. */
bool ab_process_manager_update(AbProcessManager *manager, float seconds,
                               AbDataValue *skip_input, AbError *error);

#endif
