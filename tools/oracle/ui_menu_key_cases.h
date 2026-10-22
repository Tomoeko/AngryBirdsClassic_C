#ifndef AB_ORACLE_UI_MENU_KEY_CASES_H
#define AB_ORACLE_UI_MENU_KEY_CASES_H

#include <stddef.h>
#include <stdint.h>

/* Authored input fixtures shared by the C/native comparison drivers. The
 * original installed method selects its own mappings and iteration order. */
static const char *const ui_key_names[] = {"KEY_BACK",
                                           "RETURN",
                                           "KEY_MENU",
                                           "ESCAPE",
                                           "A",
                                           "B",
                                           "C",
                                           "D",
                                           "E",
                                           "F",
                                           "G",
                                           "H",
                                           "I",
                                           "J",
                                           "K",
                                           "L",
                                           "M",
                                           "N",
                                           "O",
                                           "P",
                                           "Q",
                                           "R",
                                           "S",
                                           "T",
                                           "U",
                                           "V",
                                           "W",
                                           "X",
                                           "Y",
                                           "Z",
                                           "0",
                                           "1",
                                           "2",
                                           "3",
                                           "4",
                                           "5",
                                           "6",
                                           "7",
                                           "8",
                                           "9",
                                           "BACKSPACE",
                                           "SPACE",
                                           "SUBTRACT",
                                           "DASH",
                                           "LEFT",
                                           "RIGHT",
                                           "UP",
                                           "DOWN",
                                           "CONTROL",
                                           "SHIFT",
                                           "DELETE",
                                           "PAGEUP",
                                           "PAGEDOWN",
                                           "TAB",
                                           "F1",
                                           "F2",
                                           "F3",
                                           "F4",
                                           "F5",
                                           "F6",
                                           "F7",
                                           "F8",
                                           "F9",
                                           "F10",
                                           "F11",
                                           "F12",
                                           "PAUSE",
                                           "REPLAY",
                                           "FASTFORWARD",
                                           "REWIND",
                                           "KEY_GAMING_A",
                                           "KEY_GAMING_B",
                                           "BACK",
                                           "-",
                                           "PASTE",
                                           "RESTART",
                                           "FORWARD",
                                           "BACKWARD",
                                           "BUTTON_A",
                                           "BUTTON_B"};

enum { UI_KEY_INPUT_COUNT = 72, UI_KEY_LABEL_COUNT = 80 };

static size_t ui_key_name_index(const char *name) {
    for (size_t index = 0; index < UI_KEY_LABEL_COUNT; ++index) {
        size_t offset = 0;
        while (name[offset] && name[offset] == ui_key_names[index][offset]) {
            ++offset;
        }
        if (!name[offset] && !ui_key_names[index][offset]) {
            return index;
        }
    }
    return UI_KEY_LABEL_COUNT;
}

static void ui_key_fixture(const uint32_t command[16], uint8_t output[3][72]) {
    for (size_t phase = 0; phase < 3; ++phase) {
        for (size_t key = 0; key < UI_KEY_INPUT_COUNT; ++key) {
            output[phase][key] = 0;
        }
        uint32_t seed = command[6 + phase] ^ command[14] ^ command[15] ^
                        ((uint32_t)phase + 1) * UINT32_C(0x52017bc1);
        for (size_t item = 0; item < 3; ++item) {
            seed ^= seed << 13;
            seed ^= seed >> 17;
            seed ^= seed << 5;
            output[phase][seed % UI_KEY_INPUT_COUNT] = 1;
        }
    }
    unsigned mode = command[15] % 8;
    if (mode == 1) {
        for (size_t key = 0; key < 4; ++key) {
            output[2][key] = 1;
        }
    } else if (mode == 2) {
        for (size_t phase = 0; phase < 3; ++phase) {
            output[phase][ui_key_name_index("V")] = 1;
        }
    } else if (mode == 3 || mode == 5) {
        const char *const special[] = {"F1", "F12", "SUBTRACT", "DASH",
                                       "A",  "Z",   "0",        "9"};
        for (size_t phase = 0; phase < 3; ++phase) {
            for (size_t item = 0; item < 4; ++item) {
                output[phase][ui_key_name_index(special[(mode == 5 ? 4 : 0) + item])] =
                    1;
            }
        }
    } else if (mode == 4) {
        output[1][ui_key_name_index("UP")] = 1;
        output[1][ui_key_name_index("DOWN")] = 1;
        for (size_t phase = 0; phase < 3; ++phase) {
            output[phase][ui_key_name_index("PAUSE")] = 1;
            output[phase][ui_key_name_index("REPLAY")] = 1;
        }
    }
    output[1][ui_key_name_index("CONTROL")] = (uint8_t)((command[13] >> 17) & 1);
}

#endif
