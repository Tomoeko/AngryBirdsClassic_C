#ifndef AB_POWERUP_USES_H
#define AB_POWERUP_USES_H

#include "ab/base.h"

typedef struct AbPowerupUses AbPowerupUses;

/* Own PowerupManager's dense m_powerupsUsed string list. Start empty, as assigned
 * by init; mark appends the lowercase name, including duplicates. Query lowers
 * its argument and tests membership; count includes duplicates. ASCII names only
 * (the pinned powerup identifiers), without locale-dependent conversion, Lua,
 * sparse lists, numeric coercion or raw table aliases. max_name_bytes excludes
 * the terminator. Allocate the bounded list/string storage once; no later
 * allocation. Capacity errors preserve state. Output starts NULL.
 *
 * This is the shared use-list storage, not purchase/activation, effects or the
 * full PowerupManager initializer/reset. clear belongs after the original reset
 * callback/property phases; it does not execute them. Serialize access. */
bool ab_powerup_uses_open(size_t capacity, size_t max_name_bytes, size_t storage_bytes,
                          AbPowerupUses **output, AbError *error);
void ab_powerup_uses_close(AbPowerupUses *uses);
size_t ab_powerup_uses_bytes(const AbPowerupUses *uses);
size_t ab_powerup_uses_count(const AbPowerupUses *uses);
bool ab_powerup_uses_mark(AbPowerupUses *uses, const char *name, AbError *error);
bool ab_powerup_uses_contains(const AbPowerupUses *uses, const char *name, bool *output,
                              AbError *error);
bool ab_powerup_uses_clear(AbPowerupUses *uses);

#endif
