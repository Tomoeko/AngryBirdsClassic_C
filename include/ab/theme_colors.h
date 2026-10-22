#ifndef AB_THEME_COLORS_H
#define AB_THEME_COLORS_H

#include "ab/lua_data.h"

typedef struct {
    float background[3], ground[3], tint[3];
} AbThemeColors;

/* Read finite numeric r/g/b from mandatory color/groundColor tables. Background
 * channels remain raw; ground channels multiply by native binary32 reciprocal
 * 0x3b808081. Optional backgroundTintColor uses raw r/g/b when a table and255
 * otherwise. This is static schema interpretation, without a Lua runtime or
 * allocation. Numeric-string/nonnumeric channel coercion is outside this
 * contract. Output is unchanged on failure; all graph data stays borrowed.
 * Layer construction/lifetime and rendered color policy are separate. */
bool ab_theme_colors(const AbDataTable *theme, AbThemeColors *output, AbError *error);

#endif
