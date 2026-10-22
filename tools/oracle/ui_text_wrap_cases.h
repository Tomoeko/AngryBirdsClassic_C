#ifndef AB_ORACLE_UI_TEXT_WRAP_CASES_H
#define AB_ORACLE_UI_TEXT_WRAP_CASES_H

#include <stddef.h>

typedef struct {
    const char *bytes;
    size_t size;
} UiTextWrapCase;

#define UI_TEXT_WRAP_CASE(value) {value, sizeof(value) - 1}
static const UiTextWrapCase ui_text_wrap_cases[] = {
    UI_TEXT_WRAP_CASE(""),
    UI_TEXT_WRAP_CASE("A"),
    UI_TEXT_WRAP_CASE("AB"),
    UI_TEXT_WRAP_CASE("A B"),
    UI_TEXT_WRAP_CASE("A  B"),
    UI_TEXT_WRAP_CASE(" A"),
    UI_TEXT_WRAP_CASE("  A"),
    UI_TEXT_WRAP_CASE("A "),
    UI_TEXT_WRAP_CASE("A  "),
    UI_TEXT_WRAP_CASE("A-B"),
    UI_TEXT_WRAP_CASE("A--B"),
    UI_TEXT_WRAP_CASE("--"),
    UI_TEXT_WRAP_CASE("-A"),
    UI_TEXT_WRAP_CASE("A-"),
    UI_TEXT_WRAP_CASE("A\nB"),
    UI_TEXT_WRAP_CASE("\n"),
    UI_TEXT_WRAP_CASE("\n\n"),
    UI_TEXT_WRAP_CASE("A\n"),
    UI_TEXT_WRAP_CASE("\nA"),
    UI_TEXT_WRAP_CASE("A\n\nB"),
    UI_TEXT_WRAP_CASE(" A B "),
    UI_TEXT_WRAP_CASE("ABABABABABAB"),
    UI_TEXT_WRAP_CASE("A\tB"),
    UI_TEXT_WRAP_CASE("A\rB"),
    UI_TEXT_WRAP_CASE("A\xe2\x80\x8b"
                      "B"),
    UI_TEXT_WRAP_CASE("A \xe2\x80\x8b B"),
    UI_TEXT_WRAP_CASE("\xe4\xb8\x80\xf0\x9f\x98\x80 A"),
    UI_TEXT_WRAP_CASE("\xf0\x9f\x98\x80-\xe4\xb8\x80"),
    UI_TEXT_WRAP_CASE("A\0B"),
    UI_TEXT_WRAP_CASE("A\0 B"),
    UI_TEXT_WRAP_CASE("\xc0\x80"
                      "A"),
    UI_TEXT_WRAP_CASE("A\xf8"
                      "B"),
    UI_TEXT_WRAP_CASE("\xf8"),
    UI_TEXT_WRAP_CASE("a0 B-a0 B\nAB"),
    UI_TEXT_WRAP_CASE("\xe2\x80\x8b\xe2\x80\x8b"),
    UI_TEXT_WRAP_CASE("A\xe2\x80\x8b\nB")};
#undef UI_TEXT_WRAP_CASE

#endif
