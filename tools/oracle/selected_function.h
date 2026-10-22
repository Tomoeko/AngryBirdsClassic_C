#ifndef AB_ORACLE_SELECTED_FUNCTION_H
#define AB_ORACLE_SELECTED_FUNCTION_H

static void load_selected_function(void *lua, const char *path, const char *name) {
    static char bytes[8192];
    int file = system_call(5, (int)(uintptr_t)path, NULL, 0);
    if (file < 0) {
        fail("Cannot open original selected function prototype\n");
    }
    size_t size = 0;
    int read_failed = 0;
    while (size < sizeof(bytes)) {
        int count = system_call(3, file, bytes + size, sizeof(bytes) - size);
        if (count < 0) {
            read_failed = 1;
            break;
        }
        if (!count) {
            break;
        }
        size += (size_t)count;
    }
    int closed = system_call(6, file, NULL, 0);
    if (read_failed || closed || size == sizeof(bytes) ||
        damage_lua.load(lua, bytes, (unsigned)size, "selected-native-function")) {
        fail("Cannot load original selected function prototype\n");
    }
    /* The selected prototype is the function itself, not a bootstrap chunk. */
    damage_lua.set_field(lua, -10002, name);
    damage_lua.set_top(lua, 0);
}

#endif
