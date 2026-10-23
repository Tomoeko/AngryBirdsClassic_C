#include "ab/classic_level5.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                               \
    do {                                                                               \
        if (!(condition)) {                                                            \
            fprintf(stderr, "Level5 script failed at %d: %s\n", __LINE__, #condition); \
            return 1;                                                                  \
        }                                                                              \
    } while (0)

static AbDataValue text(const char *name) {
    return (AbDataValue){.kind = AB_DATA_STRING,
                         .string = {(const uint8_t *)name, strlen(name)}};
}

static AbDataValue number(float value) {
    AbDataValue result = {.kind = AB_DATA_NUMBER};
    memcpy(&result.bits, &value, sizeof(value));
    return result;
}

static bool reward(void *context, AbSpan name, AbError *error) {
    if (name.size != 10 || memcmp(name.data, "LevelGE_15", 10))
        return ab_fail(error, "Wrong Level5 golden-egg name");
    ++*(size_t *)context;
    return true;
}

int main(void) {
    AbDataEntry position[] = {{text("x"), number(10)}, {text("y"), number(20)}};
    AbDataTable chest = {position, 2, 2, NULL};
    AbDataEntry world_row = {text("ExtraTreasureChest_1"),
                             {.kind = AB_DATA_TABLE, .table = &chest}};
    AbDataTable world = {&world_row, 1, 1, NULL};
    AbDataEntry pressed = {text("O"), {.kind = AB_DATA_BOOLEAN, .bits = 1}};
    AbDataTable keys = {&pressed, 1, 1, NULL};
    AbClassicLevel5 state = {0};
    AbClassicLevel5Input input = {
        .world = &world, .pressed = &keys, .cursor_physics = {12.5f, 20}};
    AbError error = {0};
    size_t calls = 0;
    CHECK(ab_classic_level5_update(&state, input, reward, &calls, &error) &&
          state.key_index == 1 && calls == 0);
    input.double_click = true;
    CHECK(ab_classic_level5_update(&state, input, reward, &calls, &error) &&
          calls == 0);
    input.cursor_physics.x = 12.499f;
    CHECK(ab_classic_level5_update(&state, input, reward, &calls, &error) &&
          calls == 1);
    world_row.value = (AbDataValue){0};
    CHECK(ab_classic_level5_update(&state, input, reward, &calls, &error) &&
          calls == 1);
    world_row.value = (AbDataValue){.kind = AB_DATA_TABLE, .table = &chest};
    state.key_index = 0;
    input.double_click = false;
    input.desktop = true;
    input.cursor_physics = (AbVec2){1000, 1000};
    const char *const sequence[] = {"O", "P", "E", "N"};
    for (size_t index = 0; index < 4; ++index) {
        pressed.key = text(sequence[index]);
        CHECK(ab_classic_level5_update(&state, input, reward, &calls, &error) &&
              state.key_index == index + 1 && calls == (index == 3 ? 2 : 1));
    }
    state.key_index = 0;
    input.desktop = false;
    input.accelerometer = true;
    pressed.key = text("KEY_GAMING_B");
    CHECK(ab_classic_level5_update(&state, input, reward, &calls, &error) &&
          state.key_index == 1 && calls == 3);
    CHECK(!ab_classic_level5_update(&state, input, NULL, NULL, &error));
    puts("Level5: strict treasure radius, absent chest, one-key progression, OPEN "
         "sequence, controller branch and real reward boundary.");
    return 0;
}
