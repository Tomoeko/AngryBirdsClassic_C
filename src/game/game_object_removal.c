#include "ab/game_object_removal.h"

static bool remove_teleporter(AbGameObjectRemoval *removal, AbSpan name,
                              AbError *error) {
    AbStringMap *teleporters = removal->teleporters;
    uintptr_t handle = 0;
    bool present = false;
    if (!teleporters ||
        !ab_string_map_get(teleporters, name, &handle, &present, error)) {
        return ab_fail(error, "Missing object-removal teleporter registry");
    }
    if (!present) {
        return true;
    }
    AbGameTeleporter *teleporter = (AbGameTeleporter *)handle;
    if (!teleporter || (teleporter->kind != AB_GAME_TELEPORTER_SIMPLE &&
                        teleporter->kind != AB_GAME_TELEPORTER_PORTAL)) {
        return ab_fail(error, "Unsupported object-removal teleporter metadata");
    }
    if (teleporter->kind == AB_GAME_TELEPORTER_PORTAL &&
        teleporter->keep_on_object_removal) {
        return true;
    }
    if (removal->destroy_teleporter) {
        removal->destroy_teleporter(removal->teleporter_context, teleporter);
    }
    return ab_string_map_set(teleporters, name, 0, false, error);
}

bool ab_game_object_remove(AbGameObjectRemoval *removal, AbSpan name,
                           bool *physical_removed, AbError *error) {
    if (!removal || !physical_removed || (name.size && !name.data)) {
        return ab_fail(error, "Invalid complete native object-removal input");
    }
    bool removed = false;
    if (!ab_game_object_remove_core(removal->objects, name, removal->object_host,
                                    &removed, error) ||
        !ab_classic_joints_forget_object(removal->joints, removal->objects, name,
                                         error) ||
        !remove_teleporter(removal, name, error)) {
        return false;
    }
    *physical_removed = removed;
    return true;
}
