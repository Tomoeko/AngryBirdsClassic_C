#ifndef AB_UI_VALUE_TEXT_H
#define AB_UI_VALUE_TEXT_H

#include "ab/ui_pointer.h"

/* Plain original tostring values: nil, Boolean, text and binary32 numbers.
 * Numbers use the target's %.9g conversion after promotion to double. Requires
 * C numeric locale and round-to-nearest; floating status flags are not retained.
 * Both NaN signs/payloads produce nan; infinities retain their sign.
 * Non-numeric results borrow input/static text; numeric results borrow caller
 * storage, without a trailing zero. Input/output storage must be disjoint.
 * Opaque object/function address formatting and custom __tostring are separate
 * boundaries. No allocation. Failure preserves bytes and output. */
bool ab_ui_value_text(AbUiPointerValue value, uint8_t *storage, size_t capacity,
                      AbSpan *output);

/* Native LuaTable::toString used by GameLua.drawUITextInternal. Numeric values
 * use the same %.9g conversion above; text stops at its first NUL because the
 * adapter constructs std::string from the returned C string. Other plain kinds
 * produce empty text, including nil/Boolean/object/function; no __tostring runs.
 * Input is a value copy, so numeric conversion does not replace the table field.
 * Lifetimes, locale/rounding, capacity and preserved failure rules above apply. */
bool ab_ui_native_value_text(AbUiPointerValue value, uint8_t *storage, size_t capacity,
                             AbSpan *output);

#endif
