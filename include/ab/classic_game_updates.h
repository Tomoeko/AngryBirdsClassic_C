#ifndef AB_CLASSIC_GAME_UPDATES_H
#define AB_CLASSIC_GAME_UPDATES_H

#include "ab/string_map.h"

typedef struct AbClassicGameUpdates AbClassicGameUpdates;

typedef struct {
    void *context;
    bool (*update)(void *context, float seconds, float time, AbError *error);
} AbClassicGameUpdateFunction;

/* Own the private string-key function registry used by call/add/remove/clear
 * GameUpdateFunctions prototypes120868/122960/123028/123081. Open preserves the
 * original nil upvalue; the actual level-load clear establishes its empty table.
 * nodes is a power of two. Borrow pinned key bytes and stable callback records
 * until close. Numeric/object keys, false-valued entries, metatables and GC are
 * outside this binding. All storage is preallocated and bounded by bytes.
 */
bool ab_classic_game_updates_open(size_t nodes, size_t bytes,
                                  AbClassicGameUpdates **output, AbError *error);
void ab_classic_game_updates_close(AbClassicGameUpdates *updates);
size_t ab_classic_game_updates_bytes(const AbClassicGameUpdates *updates);
/* Read the current table without changing it. Nil/missing returns NULL;
 * callback records remain borrowed. This also permits failed-state inspection. */
bool ab_classic_game_updates_lookup(const AbClassicGameUpdates *updates, AbSpan key,
                                    const AbClassicGameUpdateFunction **output,
                                    AbError *error);
bool ab_classic_game_updates_clear(AbClassicGameUpdates *updates, AbError *error);
bool ab_classic_game_updates_add(AbClassicGameUpdates *updates, AbSpan key,
                                 const AbClassicGameUpdateFunction *function,
                                 AbError *error);
bool ab_classic_game_updates_remove(AbClassicGameUpdates *updates, AbSpan key,
                                    AbError *error);

/* Capture the current table and iterate in native string-map pairs order.
 * Mutations use current upvalue state; clearing during a callback replaces it
 * while the captured table continues. Deleting the current key preserves its
 * iterator position; insertion/rehash resumes after that key's current slot.
 * If mutation removes that retained key during rehash, next rejects as native
 * next would. Reentry rejects. Callback/iteration failure retains effects and
 * stops further use. No implicit time/global publication or frame allocation.
 */
bool ab_classic_game_updates_call(AbClassicGameUpdates *updates, float seconds,
                                  float time, AbError *error);

#endif
