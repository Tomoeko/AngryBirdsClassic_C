#ifndef AB_SETTINGS_INVENTORY_H
#define AB_SETTINGS_INVENTORY_H

#include "ab/settings_statistics.h"

/* SettingsWrapper35270..36064 over the real owned settings graph. Despite its
 * name, installationSpecificInventory is a direct settings table; there is no
 * extra installation-ID dictionary. getItem creates only nil entries, with
 * totalCount=0 then usedCount=0 in a constructor hinted for two string fields.
 * Existing non-table item values return raw and fail only when later indexed.
 *
 * Bind fresh/restored actual storage; item names remain borrowed immutable keys
 * until its close. Numeric operations use finite binary32 fields, without
 * clamping balances or implicit initial rewards. setItemAmount stores its two
 * raw scalar arguments in source order. String coercion/foreign methods and
 * non-scalar item counts are unsupported. No allocation, saving or VM; earlier
 * lazy creation and assignments survive reached failures. */
bool ab_settings_inventory_item(AbSettingsStatistics *settings, AbSpan name,
                                AbDataValue *output, AbError *error);
bool ab_settings_inventory_amount(AbSettingsStatistics *settings, AbSpan name,
                                  float *output, AbError *error);
bool ab_settings_inventory_set(AbSettingsStatistics *settings, AbSpan name,
                               AbDataValue total, AbDataValue used, AbError *error);
bool ab_settings_inventory_consume(AbSettingsStatistics *settings, AbSpan name,
                                   AbError *error);
bool ab_settings_inventory_add(AbSettingsStatistics *settings, AbSpan name,
                               AbDataValue amount, AbError *error);

#endif
