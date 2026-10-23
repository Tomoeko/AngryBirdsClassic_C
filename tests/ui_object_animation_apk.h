#ifndef AB_UI_OBJECT_ANIMATION_APK_H
#define AB_UI_OBJECT_ANIMATION_APK_H

#include "object_animation_cases.h"

static bool object_animation_reference(PreviewGame *game, AbError *error) {
    const char *path = getenv("AB_OBJECT_ANIMATION_REFERENCE");
    if (!path)
        return true;
    FILE *file = fopen(path, "rb");
    if (!file)
        return ab_fail(error, "Cannot read private object animation reference");
    char marker[80];
    bool okay = fgets(marker, sizeof(marker), file) &&
                !strcmp(marker, "AB_ANDROID_OBJECT_ANIMATION_V1 256 8\n");
    AbClassicLevelView level = ab_classic_level_view(game->playfield.level);
    size_t object = 0;
    const char *const names[] = {"animationTimer", "currentAnimationFrame",
                                 "animationFinished", "damageSprites", "damageSprite"};
    for (size_t item = 0; okay && item < AB_OBJECT_ANIMATION_CASES; ++item) {
        AbObjectAnimationCase input = ab_object_animation_case(item);
        AbDataEntry entries[10];
        AbDataTable animation = {entries, 0, 10, NULL};
        if (input.duration_kind)
            entries[animation.count++] = (AbDataEntry){
                menu_data("timePerFrame"), input.duration_kind == 1
                                               ? (AbDataValue){.kind = AB_DATA_BOOLEAN}
                                               : menu_route_number(input.duration)};
        entries[animation.count++] = (AbDataEntry){
            menu_data("looping"), {.kind = AB_DATA_BOOLEAN, .bits = input.looping}};
        char keys[8][40];
        for (unsigned frame = 1; frame <= input.frames; ++frame) {
            snprintf(keys[frame - 1], sizeof(keys[0]), "damageSprites%u", frame);
            entries[animation.count++] = (AbDataEntry){
                menu_data(keys[frame - 1]), frame == input.false_frame
                                                ? (AbDataValue){.kind = AB_DATA_BOOLEAN}
                                                : menu_route_number((float)frame)};
        }
        AbDataEntry fields[] = {
            {menu_data("animation"),
             input.active ? (AbDataValue){.kind = AB_DATA_TABLE, .table = &animation}
                          : (AbDataValue){.kind = AB_DATA_BOOLEAN}},
            {menu_data("sprite"), menu_data("base")},
            {menu_data("definition"), {0}},
            {menu_data("dontUpdateDamageSprite"), {.kind = AB_DATA_BOOLEAN, .bits = 1}},
            {menu_data("animationFinished"),
             {.kind = AB_DATA_BOOLEAN, .bits = input.finished}},
            {menu_data("animationDisabled"),
             {.kind = AB_DATA_BOOLEAN, .bits = input.disabled}},
            {menu_data("animationTimer"),
             input.timer_present ? menu_route_number(input.timer) : (AbDataValue){0}},
            {menu_data("currentAnimationFrame"), menu_route_number(input.frame)},
            {menu_data("damageSprites"), menu_route_number(13)},
            {menu_data("damageSprite"), {.kind = AB_DATA_BOOLEAN}}};
        for (size_t field = 0; okay && field < sizeof(fields) / sizeof(*fields);
             ++field)
            okay = ab_level_property_set(level.properties, object,
                                         fields[field].key.string, fields[field].value,
                                         error);
        for (size_t step = 0; okay && step < AB_OBJECT_ANIMATION_STEPS; ++step) {
            okay = ab_classic_object_animation_update(
                game->playfield.level, game->playfield.visuals, &game->workspace,
                object, input.seconds[step], error);
            for (size_t field = 0; okay && field < 5; ++field) {
                AbDataValue value = {0};
                uint32_t expected[2];
                okay = fscanf(file, "%x %x", &expected[0], &expected[1]) == 2;
                value = menu_route_field(&level.properties->objects[object].physical,
                                         names[field]);
                uint32_t actual[2] = {(uint32_t)value.kind, value.bits};
                if (value.kind == AB_DATA_STRING)
                    actual[1] = menu_route_equal(value, menu_data("base")) ? 1 : 2;
                if (okay && memcmp(actual, expected, sizeof(actual)))
                    okay = ab_fail(error,
                                   "Native animation %zu/%zu/%s: %u:%08x != %u:%08x",
                                   item, step, names[field], actual[0], actual[1],
                                   expected[0], expected[1]);
            }
        }
        /* Never retain a borrowed stack table after this case. */
        if (!ab_level_property_set(level.properties, object, menu_span("animation"),
                                   (AbDataValue){0}, error))
            okay = false;
    }
    if (okay)
        okay = fgetc(file) == '\n' && fgets(marker, sizeof(marker), file) &&
               !strcmp(marker, "AB object animation reference complete\n") &&
               fgetc(file) == EOF && !ferror(file);
    if (fclose(file))
        okay = false;
    if (!okay)
        return error && error->message[0]
                   ? false
                   : ab_fail(error, "Invalid private object animation corpus");
    puts("Original object animation: 2,048 calls match all timer, frame, completion, "
         "damage-override and damage-sprite field words.");
    return true;
}

#endif
