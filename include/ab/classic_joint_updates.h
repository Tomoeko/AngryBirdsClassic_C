#ifndef AB_CLASSIC_JOINT_UPDATES_H
#define AB_CLASSIC_JOINT_UPDATES_H

#include "ab/classic_joints.h"
#include "ab/event_manager.h"
#include "ab/game_update_tail.h"

typedef struct AbClassicJointUpdates AbClassicJointUpdates;

typedef struct {
    AbClassicJoints *joints;
    const AbLuaData *level_data;
    AbEventManager *events;
    AbDataValue destroyed_event;
    void *context;
    bool (*print_destroy)(void *context, AbSpan name, AbError *error);
    size_t names, requests, storage_bytes;
} AbClassicJointUpdateConfig;

/* Bind the initial native joint metadata insertion order and an owned deferred
 * queue to GameUpdate's existing joint phase. Borrow the registry, level data,
 * event manager and all names until close. Reserve all storage at open; refresh,
 * enqueue and dispatch allocate nothing. Metadata tables retain stable identities
 * even after removal. Notification payloads also remain valid until close;
 * requests bounds both queued names and lifetime notification storage.
 * Subsequent factories must register each new metadata table
 * immediately after native creation. Failures can retain preceding effects. */
bool ab_classic_joint_updates_open(AbClassicJointUpdateConfig config,
                                   AbClassicJointUpdates **output, AbError *error);
void ab_classic_joint_updates_close(AbClassicJointUpdates *updates);
bool ab_classic_joint_updates_register(AbClassicJointUpdates *updates, AbSpan name,
                                       AbError *error);
bool ab_classic_joint_updates_refresh(AbClassicJointUpdates *updates, AbError *error);
AbGameTailJoints *ab_classic_joint_updates_tail(AbClassicJointUpdates *updates);
/* Retained notification packets, including a packet whose listener failed. */
size_t ab_classic_joint_updates_notifications(const AbClassicJointUpdates *updates);
/* Callback signatures for Museum collision break requests and tail services. */
bool ab_classic_joint_updates_defer(void *updates, AbSpan name, AbError *error);
/* Museum explosion's objects.joints traversal: enqueue every named metadata
 * record whose end1 or end2 equals object, in source string-map order. Keep
 * duplicate requests and defer all engine destruction to the normal tail pass.
 * Refresh the live projection before traversing; no allocation occurs. */
bool ab_classic_joint_updates_defer_object(AbClassicJointUpdates *updates,
                                           AbSpan object, AbError *error);
bool ab_classic_joint_updates_call(AbClassicJointUpdates *updates, AbGameTailCall *call,
                                   AbError *error);

#endif
