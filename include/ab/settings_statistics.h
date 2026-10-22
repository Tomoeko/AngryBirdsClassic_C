#ifndef AB_SETTINGS_STATISTICS_H
#define AB_SETTINGS_STATISTICS_H

#include "ab/lua_data.h"

typedef struct AbSettingsStatistics AbSettingsStatistics;

typedef struct {
    AbSpan device_id, installation_id;
    size_t tables, entries_per_table, storage_bytes;
} AbSettingsStatisticsConfig;

/* Own a bounded, initially empty statistics graph and copies of the two IDs.
 * Form the pinned SettingsWrapper keys device_ID/installationId_ID once. Other
 * key bytes passed to writes remain borrowed and immutable until close. Tables
 * start with the original empty-constructor layout; string-key growth/iteration
 * uses the target dictionary rules. entries_per_table is a nonzero power of two.
 * Allocate all table/entry/hash storage at open; later calls do not allocate.
 *
 * This is storage for the selected counters, not a complete settings file or a
 * save loader. The caller must explicitly restore prior values and their write
 * order before gameplay. Constructor hints, GC, mixed keys, coercion, cycles,
 * arbitrary table-valued leaves and external mutation are outside this domain.
 * Output starts NULL; close releases every allocation. Serialize all access. */
bool ab_settings_statistics_open(AbSettingsStatisticsConfig config,
                                 AbSettingsStatistics **output, AbError *error);
/* Reserve different maximum capacities for a prefix of table slots in their
 * creation order. Remaining slots use entries_per_table. Each capacity is a
 * nonzero power of two. Borrow the array only during open; all entry/hash memory
 * is still owned and allocated up front. Capacity changes no native constructor
 * hint, growth order or table identity. This supports large campaign dictionaries
 * alongside small per-level records without reserving the largest size for all.
 */
bool ab_settings_statistics_open_capacities(AbSettingsStatisticsConfig config,
                                            const size_t *capacities, size_t count,
                                            AbSettingsStatistics **output,
                                            AbError *error);
void ab_settings_statistics_close(AbSettingsStatistics *statistics);
size_t ab_settings_statistics_bytes(const AbSettingsStatistics *statistics);
const AbDataTable *ab_settings_statistics_root(const AbSettingsStatistics *statistics);

/* Explicit storage/import write at one to three string keys. Create missing
 * intermediate tables only for nil, preserving malformed false/number values
 * until indexed. Leaves may be nil, Boolean, finite number or string. Keys and
 * string values remain borrowed. Capacity failure retains earlier table creation;
 * the failing leaf write itself is atomic. Removed tables stay alive until close.
 * No implicit persistence, conversion or installation/device selection. */
bool ab_settings_statistics_write(AbSettingsStatistics *statistics, const AbSpan *path,
                                  size_t count, AbDataValue value, AbError *error);

/* Create a fresh owned table at this path, retaining replaced table identities
 * until close. Parent creation still uses nil-only empty tables. hash_hint is
 * the original constructor's explicit string-field hint; reject excess capacity.
 * This also supports settings achievement-status records. No external table
 * adoption, ownership transfer or frame allocation. Earlier parent creations
 * survive a failed final publication. */
bool ab_settings_statistics_create_table(AbSettingsStatistics *statistics,
                                         const AbSpan *path, size_t count,
                                         size_t hash_hint, AbError *error);

/* Append a non-nil scalar to a dense array leaf. Missing parents/leaf create
 * empty owned tables only for nil. A pristine leaf becomes an array; populated
 * string dictionaries and malformed scalar leaves reject. Array leaves reject
 * subsequent string writes, so the owner never interprets numeric key storage
 * as strings. Used by the source visited-page settings lists. */
bool ab_settings_statistics_append(AbSettingsStatistics *statistics, const AbSpan *path,
                                   size_t count, AbDataValue value, AbError *error);

/* Snapshot an owned string dictionary in target hash-slot iteration order.
 * A zero-length path selects the root; up to three keys select nested tables.
 * No allocation, creation or mutation. Entries borrow the owner's key/value
 * storage. Capacity must cover all retained slots, including removed leaves.
 * output_count commits only on success; scratch entries may change on failure. */
bool ab_settings_statistics_pairs(const AbSettingsStatistics *statistics,
                                  const AbSpan *path, size_t count,
                                  AbDataEntry *entries, size_t capacity,
                                  size_t *output_count, AbError *error);

/* SettingsWrapper.setExtraTutorialShown19668, using this owned root as the
 * actual settings table. getTutorials creates an empty table only for nil;
 * absent/false item records are retained, and truthy showHelp becomes false.
 * Item keys must be strings in this storage domain. No save or notification. */
bool ab_settings_statistics_extra_tutorial(AbSettingsStatistics *statistics,
                                           AbDataValue item, AbError *error);

/* Borrow the next actual owned row in native array/hash order. Cursor starts at
 * zero; dense arrays and owned dictionaries are supported. Output spans stay
 * borrowed, with no allocation/mutation. Late failure may retain cursor progress.
 * This binds serialization without copying the canonical settings tables. */
bool ab_settings_statistics_table_next(const AbSettingsStatistics *statistics,
                                       const AbDataTable *table, size_t *cursor,
                                       AbDataEntry *output, bool *present,
                                       AbError *error);

/* Source increaseLevelStarted43511 and getLevelStartedCount43733. Records live
 * under settings.levelStarted[installationId_ID][levelName], using the captured
 * installation key rather than the device key. Nil-only table creation and
 * false-or-nil local-count initialization are distinct. Start publishes the
 * actual local table and previous raw count at their source boundaries, so a
 * later failed addition/write can leave those outputs and created parents.
 * Count sums matching fields across installations in actual hash-slot order;
 * unrelated level values are not read as numbers. Both calls may initialize
 * the nil root; neither saves or emits events. Keys remain borrowed. */
bool ab_settings_statistics_start_level(AbSettingsStatistics *statistics, AbSpan name,
                                        const AbDataTable **local_table,
                                        AbDataValue *previous, AbError *error);
bool ab_settings_statistics_level_started_count(AbSettingsStatistics *statistics,
                                                AbSpan name, float *output,
                                                AbError *error);

/* getLocalAmount returns zero for a nil root/device or nil/false installation;
 * other values pass through unchanged. setLocalAmount creates nil-only parent
 * tables. Increment performs one binary32 addition after the getter and requires
 * its result to be a finite number. Earlier effects survive a failed write. */
bool ab_settings_statistics_get_local(const AbSettingsStatistics *statistics,
                                      AbSpan counter, AbDataValue *output,
                                      AbError *error);
bool ab_settings_statistics_set_local(AbSettingsStatistics *statistics, AbSpan counter,
                                      AbDataValue value, AbError *error);
bool ab_settings_statistics_increment(AbSettingsStatistics *statistics, AbSpan counter,
                                      AbError *error);
/* getDeviceSum sums all installation values for the current device, from +0
 * in target hash-slot order. A falsy root/non-table device contributes zero;
 * a truthy non-table root errors when indexed. Non-number entries reject at
 * addition. Other devices do not contribute. Output commits only on success. */
bool ab_settings_statistics_device_sum(const AbSettingsStatistics *statistics,
                                       AbSpan counter, float *output, AbError *error);

#endif
