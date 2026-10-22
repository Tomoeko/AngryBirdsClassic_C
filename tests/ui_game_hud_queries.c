#include "ab/ui_game_hud_services.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static AbDataValue text(const char *name) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)name, strlen(name)}};
}

static AbDataValue number(float value) {
    AbDataValue output = {.kind = AB_DATA_NUMBER};
    memcpy(&output.bits, &value, sizeof(value));
    return output;
}

static AbDataValue input_value(uint32_t code) {
    switch (code) {
        case 0:
            return (AbDataValue){0};
        case 1:
        case 2:
            return (AbDataValue){.kind = AB_DATA_BOOLEAN, .bits = code == 2};
        case 3:
        case 4:
            return number(code == 3 ? 0 : -1);
        default:
            return text("");
    }
}

static AbDataEntry field(const char *name, AbDataValue value) {
    return (AbDataEntry){text(name), value};
}

static AbDataValue table_value(AbDataTable *table) {
    return (AbDataValue){.kind = AB_DATA_TABLE, .table = table};
}

static bool check_case(const uint32_t input[13], const uint32_t expected[6],
                       size_t item) {
    for (size_t index = 0; index < 13; ++index)
        if (input[index] >= (index == 12 ? 3u : 6u))
            return false;
    AbDataTable empty = {0};
    AbDataEntry episode_fields[] = {
        field("powerup_button_disabled", input_value(input[7])),
        field("rmf_button_enabled", input_value(input[8]))};
    AbDataTable episode = {episode_fields, 2, 2, NULL};
    AbDataEntry page_fields[] = {field("golden_egg_end_screen", input_value(input[4])),
                                 field("rmf_button_enabled", input_value(input[9]))};
    AbDataTable page = {page_fields, 2, 2, NULL};
    AbDataEntry episode_entry = {number(1), table_value(&episode)};
    AbDataTable episodes = {&episode_entry, 1, 1, NULL};
    AbDataEntry purchased =
        field("redsMightyFeathersPurchased", input_value(input[10]));
    AbDataEntry unlocked = field("shockwavePowerupUnlocked", input_value(input[11]));
    AbDataTable flags = {&purchased, 1, 1, NULL};
    AbDataTable synced = {&unlocked, 1, 1, NULL};
    AbDataEntry settings_fields[] = {field("flagTable", table_value(&flags)),
                                     field("syncedFlagTable", table_value(&synced))};
    AbDataTable settings = {settings_fields, 2, 2, NULL};
    const char *const models[] = {"android", "iphone", "ipad"};
    AbDataEntry globals_fields[] = {
        field("g_powerups_enabled", input_value(input[0])),
        field("eagleBaitLaunched", input_value(input[1])),
        field("g_level_completing", input_value(input[2])),
        field("g_enable_powerups_in_golden_egg_levels", input_value(input[3])),
        field("levelName", text("Level1")),
        field("currentEpisode", input[6] < 2 ? input_value(input[6]) : number(1)),
        field("deviceModel", text(models[input[12]]))};
    AbDataTable globals = {globals_fields, 7, 7, NULL};
    AbClassicLevelRequest request = {
        .episode = &episode, .page = &page, .level = &empty, .name = text("Level1")};
    AbUiEagleServices eagle = {.progress = {&episodes, &empty, &globals, &empty},
                               .settings = &settings,
                               .level = &request};
    AbUiGameHudServices services = {.eagle = &eagle};
    AbUiGameHudControls controls = {.host = {.globals = &globals, .level = &request}};
    AbError error = {0};
    if (!ab_ui_eagle_services_bind(&eagle, &controls.host.eagle, &error))
        return false;
    AbDataValue hidden = input_value(input[5]);
    if (hidden.kind == AB_DATA_BOOLEAN)
        controls.hide_powerups = (AbUiPointerValue){.kind = AB_UI_POINTER_BOOLEAN,
                                                    .boolean = hidden.bits != 0};
    else if (hidden.kind == AB_DATA_NUMBER)
        controls.hide_powerups = (AbUiPointerValue){.kind = AB_UI_POINTER_NUMBER,
                                                    .number_bits = hidden.bits};
    else if (hidden.kind == AB_DATA_STRING)
        controls.hide_powerups =
            (AbUiPointerValue){.kind = AB_UI_POINTER_TEXT, .text = hidden.string};
    const AbUiGameHudQuery queries[] = {
        AB_UI_HUD_POWERUP_VISIBLE, AB_UI_HUD_RMF_VISIBLE, AB_UI_HUD_SHOCKWAVE_VISIBLE};
    for (size_t query = 0; query < 3; ++query) {
        AbUiPointerValue result;
        if (!ab_ui_game_hud_services_query(&services, &controls, queries[query],
                                           &result, &error)) {
            fprintf(stderr, "HUD query %zu/%zu failed: %s\n", item, query,
                    error.message);
            return false;
        }
        uint32_t type = result.kind == AB_UI_POINTER_NONE      ? 0u
                        : result.kind == AB_UI_POINTER_BOOLEAN ? 1u
                        : result.kind == AB_UI_POINTER_NUMBER  ? 3u
                                                               : UINT32_MAX;
        uint32_t bits = type == 1   ? (uint32_t)result.boolean
                        : type == 3 ? result.number_bits
                                    : 0;
        if (type != expected[query * 2] || bits != expected[query * 2 + 1]) {
            fprintf(stderr,
                    "HUD query %zu/%zu differs: %u/%08" PRIx32 " != %u/%08" PRIx32 "\n",
                    item, query, type, bits, expected[query * 2],
                    expected[query * 2 + 1]);
            return false;
        }
    }
    return true;
}

static bool words(FILE *file, uint32_t *output, size_t count) {
    for (size_t index = 0; index < count; ++index)
        if (fscanf(file, "%8" SCNx32, output + index) != 1)
            return false;
    return fgetc(file) == '\n';
}

static bool compare(FILE *file) {
    char header[80];
    if (!fgets(header, sizeof(header), file) ||
        strcmp(header, "AB_ANDROID_GAME_HUD_QUERIES_V1 4096\n"))
        return false;
    for (size_t item = 0; item < 4096; ++item) {
        uint32_t input[13], expected[6];
        if (!words(file, input, 13) || !words(file, expected, 6) ||
            !check_case(input, expected, item))
            return false;
    }
    return fgets(header, sizeof(header), file) &&
           !strcmp(header, "AB game HUD queries reference complete\n") &&
           fgetc(file) == EOF && !ferror(file);
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: ab_ui_game_hud_queries_tests NATIVE_CORPUS\n");
        return 2;
    }
    FILE *file = fopen(argv[1], "rb");
    if (!file)
        return 1;
    bool okay = compare(file);
    if (fclose(file))
        okay = false;
    if (okay)
        printf("All 12,288 original GameHud visibility replies match exactly.\n");
    return okay ? 0 : 1;
}
