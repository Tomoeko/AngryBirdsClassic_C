#ifndef AB_MENU_SETTINGS_PREVIEW_H
#define AB_MENU_SETTINGS_PREVIEW_H

#include <stdio.h>

static bool menu_highscores_open(PreviewMenu *menu, AbError *error) {
    if (menu->highscores)
        return true;
    AbSettingsStatisticsConfig storage = {menu_span("preview"), menu_span("preview"),
                                          1024, 16, 4 * 1024 * 1024};
    size_t capacities[1024] = {1024}, restored = 0;
    bool okay = true;
    if (menu->state_directory) {
        char path[1024];
        int count =
            snprintf(path, sizeof(path), "%s/highscores.lua", menu->state_directory);
        uint8_t key[32] = {0};
        okay =
            count > 0 && (size_t)count < sizeof(path) &&
            ab_target_save_key(menu->archive, 16 * 1024 * 1024, key, error) &&
            ab_settings_file_open(path, (AbSpan){key, sizeof(key)},
                                  (AbSettingsFileLimits){512 * 1024, 4 * 1024 * 1024},
                                  &menu->highscores_file, error) &&
            ab_settings_file_capacities(menu->highscores_file, 16, capacities, 1024,
                                        &restored, error);
        memset(key, 0, sizeof(key));
    }
    if (!okay)
        return error && error->message[0]
                   ? false
                   : ab_fail(error, "Cannot prepare persistent highscore storage");
    if (capacities[0] < 1024)
        capacities[0] = 1024;
    for (size_t index = restored ? restored : 1; index < 1024; ++index)
        capacities[index] = 16;
    return ab_settings_statistics_open_capacities(storage, capacities, 1024,
                                                  &menu->highscores, error) &&
           (!menu->highscores_file ||
            ab_settings_file_restore(menu->highscores_file, menu->highscores, error));
}

/* The interactive preview explicitly checkpoints its own offline profile.
 * Isolated component fixtures retain their caller-prepared in-memory settings.
 * This does not implement the original global save scheduler or migrate Android
 * app data. Borrowed restored strings stay alive until both settings close. */
static bool menu_settings_open(PreviewMenu *menu, AbError *error) {
    if (menu->settings || menu->local_settings)
        return menu->settings && menu->local_settings
                   ? menu_highscores_open(menu, error)
                   : ab_fail(error, "Incomplete application settings owners");
    AbSettingsStatisticsConfig storage = {menu_span("preview"), menu_span("preview"),
                                          64, 32, 1024 * 1024};
    if (!menu->state_directory)
        return ab_settings_statistics_open(storage, &menu->settings, error) &&
               ab_settings_statistics_open(storage, &menu->local_settings, error) &&
               menu_highscores_open(menu, error);
    CcPathKind kind = cc_path_information(menu->state_directory, false, NULL);
    if (kind != CC_PATH_DIRECTORY &&
        (kind != CC_PATH_MISSING || !cc_directory_create(menu->state_directory)))
        return ab_fail(error, "Cannot open the preview state directory");
    uint8_t key[32] = {0};
    bool okay = ab_target_save_key(menu->archive, 16 * 1024 * 1024, key, error);
    const char *const names[] = {"settings.lua", "settings_local.lua"};
    AbSettingsFile **files[] = {&menu->settings_file, &menu->local_file};
    AbSettingsStatistics **settings[] = {&menu->settings, &menu->local_settings};
    storage.tables = 256;
    storage.storage_bytes = 8 * 1024 * 1024;
    for (size_t index = 0; okay && index < 2; ++index) {
        char path[1024];
        int count =
            snprintf(path, sizeof(path), "%s/%s", menu->state_directory, names[index]);
        if (count < 1 || (size_t)count >= sizeof(path)) {
            okay = ab_fail(error, "Preview state path exceeds its bound");
            break;
        }
        size_t capacities[256] = {0}, restored = 0;
        okay =
            ab_settings_file_open(path, (AbSpan){key, sizeof(key)},
                                  (AbSettingsFileLimits){128 * 1024, 4 * 1024 * 1024},
                                  files[index], error) &&
            ab_settings_file_capacities(*files[index], 32, capacities, 256, &restored,
                                        error);
        if (!okay)
            break;
        if (capacities[0] < 1024)
            capacities[0] = 1024;
        /* Preserve restored creation order; reserve broad campaign maps before
         * ordinary small records for subsequent source table creation. */
        size_t reserved = restored + 16 < 256 ? restored + 16 : 256;
        for (size_t slot = restored; slot < 256; ++slot)
            capacities[slot] = slot < reserved ? 1024 : 32;
        okay = ab_settings_statistics_open_capacities(storage, capacities, 256,
                                                      settings[index], error) &&
               ab_settings_file_restore(*files[index], *settings[index], error);
    }
    memset(key, 0, sizeof(key));
    return okay && menu_highscores_open(menu, error);
}

static bool menu_preview_flush(PreviewMenu *menu, AbError *error) {
    if (!menu->state_directory)
        return true;
    return menu->settings_file && menu->local_file && menu->highscores_file
               ? ab_settings_file_save(menu->highscores_file, menu->highscores,
                                       error) &&
                     ab_settings_file_save(menu->settings_file, menu->settings,
                                           error) &&
                     ab_settings_file_save(menu->local_file, menu->local_settings,
                                           error)
               : ab_fail(error, "Preview checkpoint requires restored file owners");
}

static bool menu_cutscene_save(void *context, AbError *error) {
    PreviewMenu *menu = context;
    return menu->settings_file
               ? ab_settings_file_save(menu->settings_file, menu->settings, error)
               : ab_fail(error, "Cutscene completion requires persistent settings");
}

#endif
