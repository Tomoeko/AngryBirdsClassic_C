#ifndef AB_SETTINGS_FILE_H
#define AB_SETTINGS_FILE_H

#include "ab/settings_statistics.h"

typedef struct AbSettingsFile AbSettingsFile;

typedef struct {
    size_t plaintext_bytes, parsed_bytes;
} AbSettingsFileLimits;

/* Owned encrypted saved-table transport. Copy the supplied 32-byte APK save key,
 * path and fixed serialization buffer. Existing bytes decrypt and parse as
 * literal data; a missing file is a distinct empty initial state. No VM,
 * settings defaults, service replies or automatic overwrite on malformed input.
 * Common supplies the native regular-file/atomic-replacement boundary.
 * Open/close and explicit saves may allocate; ordinary frame drawing does not.
 *
 * Restore once into fresh owned settings storage. The supported storage domain
 * is string dictionaries through three path keys and dense scalar-array leaves;
 * other graphs reject. Constructor hash hints and actual table iteration remain
 * explicit. Restore failures retain prefix mutations: discard the settings
 * owner while this file still keeps imported spans alive. Close settings before
 * this file, including after failure. A natural save uses target serialization
 * and AES-CBC framing; containing-directory power-loss durability is not claimed.
 */
bool ab_settings_file_open(const char *path, AbSpan key, AbSettingsFileLimits limits,
                           AbSettingsFile **output, AbError *error);
bool ab_settings_file_loaded(const AbSettingsFile *file);
/* Before restore, plan entry capacities in the exact import creation order,
 * including the root. Each is at least default_entries and a power of two that
 * holds its saved table. Missing files require only the root. Caller-provided
 * scratch may retain a prefix on failure; count commits on success. No allocation
 * or file mutation. Bind this to open_capacities so a large saved campaign map
 * does not require the same capacity for every small record. */
bool ab_settings_file_capacities(const AbSettingsFile *file, size_t default_entries,
                                 size_t *capacities, size_t capacity, size_t *count,
                                 AbError *error);
bool ab_settings_file_restore(AbSettingsFile *file, AbSettingsStatistics *settings,
                              AbError *error);
bool ab_settings_file_save(AbSettingsFile *file, const AbSettingsStatistics *settings,
                           AbError *error);
void ab_settings_file_close(AbSettingsFile *file);

#endif
