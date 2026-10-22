#ifndef AB_POWERUP_CATALOG_H
#define AB_POWERUP_CATALOG_H

#include "ab/target_data.h"

typedef struct AbPowerupCatalog AbPowerupCatalog;

typedef struct {
    AbSpan name;
    const AbDataTable *definition;
    AbDataValue initial_amount, infinite;
    float reward_probability;
} AbPowerupMetadata;

/* Read the pinned ordinary Android game_conf enabled array and PowerupDefines
 * literal resource; project PowerupManager.init's initial amount, infinite and
 * reward-probability fields in activation-array order. Definitions and all text
 * stay owned through close. Initial overrides are the actual raw global table
 * or nil/false; truthy leaf overrides pass through without numeric coercion.
 * Selected override text/tables remain borrowed and must outlive this owner.
 *
 * This is the immutable registration metadata, not an initialized PowerupManager:
 * it resolves no function names, runs no effect constructor, creates no active/
 * passive instances, mutates no inventory and supplies no purchase/SDK behavior.
 * The caller establishes the ordinary non-China profile before this selection;
 * China's separate enabled array remains outside it. No VM or original code
 * execution. The retained owner/data/row allocations share storage_bytes;
 * packed-resource codecs retain their separate workspace bounds. Output starts
 * NULL; partial construction is freed on failure. */
bool ab_powerup_catalog_open(const AbZip *archive, AbSpan key,
                             AbDataValue initial_overrides, size_t storage_bytes,
                             AbPowerupCatalog **output, AbError *error);
void ab_powerup_catalog_close(AbPowerupCatalog *catalog);
size_t ab_powerup_catalog_count(const AbPowerupCatalog *catalog);
const AbPowerupMetadata *ab_powerup_catalog_at(const AbPowerupCatalog *catalog,
                                               size_t index);

#endif
