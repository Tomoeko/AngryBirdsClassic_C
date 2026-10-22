#ifndef AB_DYNAMIC_ASSET_REQUIREMENTS_H
#define AB_DYNAMIC_ASSET_REQUIREMENTS_H

#include "ab/target_data.h"

typedef struct {
    AbTargetData source;
    const AbDataTable *requirements;
} AbDynamicAssetRequirements;

/* Read the literal requirement map in scripts/dynamic/default.lua. Validate its
 * terminal registration shape without executing that call. Own decoded bytes,
 * parsed metadata and tables until close. allocation_limit bounds retained data;
 * archive/key are borrowed during open only. Output starts empty on failure.
 * This provides data only: dynamic handler lifetimes, reference counts and scene
 * transitions are separate runtime responsibilities. */
bool ab_dynamic_asset_requirements_open(const AbZip *archive, AbSpan key,
                                        size_t allocation_limit,
                                        AbDynamicAssetRequirements *output,
                                        AbError *error);
/* Borrow the dense ordered group-name table until close; NULL if not present. */
const AbDataTable *
ab_dynamic_asset_requirements_get(const AbDynamicAssetRequirements *requirements,
                                  AbSpan name);
void ab_dynamic_asset_requirements_close(AbDynamicAssetRequirements *requirements);

#endif
