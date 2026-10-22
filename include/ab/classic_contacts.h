#ifndef AB_CLASSIC_CONTACTS_H
#define AB_CLASSIC_CONTACTS_H

#include "ab/classic_level.h"
#include "ab/game_contact_world.h"

typedef struct {
    AbDeferredVelocity velocity;
    bool script_table_present;
} AbClassicContactObject;

typedef struct {
    const AbGameContactRoutes *routes;
    AbClassicGlueQueue *glue;
    void *context;
    bool (*collision)(void *context, AbGameContactCall *call, AbError *error);
    AbGameContactEndHost end;
    AbGameContactDispatchHost special;
    bool begin_disabled;
    /* Borrowed contact-object array capacity; zero retains initial count. */
    size_t object_capacity;
} AbClassicContactsConfig;

typedef struct {
    AbGameContactWorld listener;
    AbClassicLevelView level;
    AbClassicContactObject *objects;
    size_t count, capacity;
    AbClassicContactsConfig config;
} AbClassicContacts;

/* Bind actual stable level fixture userdata, names and collider metadata to the
 * recovered world listener. Borrow one contact object per created level object,
 * in creation order. The gameplay owner supplies live script-table membership
 * explicitly, including ground; open neither invents membership nor resets
 * pending velocities. Names, level, routes, queue and callback data remain live
 * until close. Reserve capacity for later canonical object identities explicitly;
 * retired object slots remain distinct when engine body slots are reused.
 *
 * Storage is caller-owned and must not move while open. No allocation or world
 * simulation occurs. Invalid open preserves output and the prior listener.
 * Close before the level and before releasing any borrowed storage. Basic and
 * special hosts retain config.context; missing required hosts fail when reached.
 */
bool ab_classic_contacts_open(AbClassicLevel *level, AbClassicContactObject *objects,
                              size_t count, AbClassicContactsConfig config,
                              AbClassicContacts *output, AbError *error);
bool ab_classic_contacts_close(AbClassicContacts *contacts, AbError *error);

/* Append the next canonical level identity between world/listener passes.
 * The caller must already supply that reserved contact row's actual physical
 * Lua-table membership and pending velocity state. This does not infer the
 * separate GameObjectScript field or reset any existing/new row. Stable fixture
 * userdata, healthy body/fixture links and supported collider metadata must
 * match the level. No allocation, callback or listener reinstall. */
bool ab_classic_contacts_register_object(AbClassicContacts *contacts, size_t object,
                                         AbError *error);

/* Recovered empty PlayMode.onEndContact, inherited unchanged by MuseumMode.
 * Select only that resolved class method; custom mode overrides remain separate.
 * The original ignores receiver and arguments and has no effects. */
bool ab_classic_contacts_play_mode_end(void *context, AbSpan first, AbSpan second,
                                       AbError *error);

/* Flush one object's retained proposal using the measured body setter. The
 * enclosing controller chooses surviving objects and native iteration order,
 * after removeBlocks and before pose history. This does not run removals or
 * choose scheduling. An absent proposal needs no live body; a pending proposal
 * requires its original body. Locked/failed worlds and callback reentry reject.
 */
bool ab_classic_contacts_flush(AbClassicContacts *contacts, size_t object,
                               AbError *error);

#endif
