#ifndef AB_ORACLE_ASSET_PROFILES_PRIVATE_H
#define AB_ORACLE_ASSET_PROFILES_PRIVATE_H

static void android_load_profiles(void *lua) {
    const char *path = "/data/local/tmp/ab520/android-asset-profiles.lua";
    char bytes[2584];
    int file = system_call(5, (int)(uintptr_t)path, NULL, 0);
    size_t offset = 0;
    if (file < 0) {
        fail("Cannot open original Android profile input\n");
    }
    while (offset < sizeof(bytes)) {
        int count = system_call(3, file, bytes + offset, sizeof(bytes) - offset);
        if (count <= 0 || (size_t)count > sizeof(bytes) - offset) {
            fail("Incomplete original Android profile input\n");
        }
        offset += (size_t)count;
    }
    char extra;
    if (system_call(3, file, &extra, 1) != 0 || system_call(6, file, NULL, 0) != 0 ||
        damage_lua.load(lua, bytes, sizeof(bytes), "original-android-profiles") ||
        damage_lua.call(lua, 0, 0, 0)) {
        fail("Cannot initialize original Android profiles\n");
    }
}

#endif
