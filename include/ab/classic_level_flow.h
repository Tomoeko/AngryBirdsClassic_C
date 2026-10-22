#ifndef AB_CLASSIC_LEVEL_FLOW_H
#define AB_CLASSIC_LEVEL_FLOW_H

#include "ab/classic_root_queries.h"
#include "ab/event_manager.h"

typedef struct AbClassicLevelFlow AbClassicLevelFlow;

typedef struct {
    void *context;
    bool (*on_show)(void *context, AbError *error);
    bool (*is_done)(void *context, AbDataValue *result, AbError *error);
} AbClassicLevelFlowListener;

typedef struct {
    AbSpan name;
    float appearing;
    AbClassicLevelFlowListener *listener;
} AbClassicLevelFlowRequest;

typedef struct {
    AbClassicLevelFlowRequest **items;
    size_t count;
} AbClassicLevelFlowQueue;

typedef struct {
    AbClassicLevelFlowQueue staging, initialization, ingame;
    AbClassicLevelFlowRequest *showing;
    AbDataValue done;
} AbClassicLevelFlowView;

typedef enum {
    AB_LEVEL_FLOW_REQUEST,
    AB_LEVEL_FLOW_CONSUME_INIT,
    AB_LEVEL_FLOW_CONSUME_INGAME,
} AbClassicLevelFlowDiagnostic;

typedef struct {
    AbUiMenuManager *menus;
    const AbUiFrameNode *notifications;
    AbEventManager *events;
    AbDataValue loading_init, loading_done, flow_done;
    size_t requests, queue_generations, notifications_count, storage_bytes;
    size_t depth_limit, work_limit;
    void *context;
    bool (*set_paused)(void *context, bool paused, AbError *error);
    /* Optional observation of source print boundaries. The sink must not
     * mutate flow/menu/request state or reenter; a failure retains the prefix. */
    bool (*diagnostic)(void *context, AbClassicLevelFlowDiagnostic kind, AbSpan name,
                       AbError *error);
} AbClassicLevelFlowConfig;

/* Source LevelStartScreenFlow, dense queues with string names and fixed builtin
 * menu/tree getters. Own bounded request records, queue generations and distinct
 * event payloads once. Borrow listeners, strings, menus/trees and events until
 * close; close before EventManager. Old queues remain retained for captured
 * initialization iterators and nested callback mutations. Limits are explicit;
 * exhaustion stops the owner rather than dropping or reordering requests.
 * Listener methods remain live and may call push/clear/loading-event operations.
 * No foreign queue replacement, sparse tables, getter overrides or raw aliases.
 * Open has no source effects; initialize must run at its actual bootstrap phase. */
bool ab_classic_level_flow_open(AbClassicLevelFlowConfig config,
                                AbClassicLevelFlow **output, AbError *error);
void ab_classic_level_flow_close(AbClassicLevelFlow *flow);
size_t ab_classic_level_flow_bytes(const AbClassicLevelFlow *flow);
AbClassicLevelFlowView ab_classic_level_flow_view(AbClassicLevelFlow *flow);
bool ab_classic_level_flow_initialize(AbClassicLevelFlow *flow, AbError *error);
bool ab_classic_level_flow_push(AbClassicLevelFlow *flow, AbSpan name, float appearing,
                                AbClassicLevelFlowListener *listener, AbError *error);
bool ab_classic_level_flow_update(AbClassicLevelFlow *flow, AbError *error);
bool ab_classic_level_flow_clear_init(AbClassicLevelFlow *flow, AbError *error);

#endif
