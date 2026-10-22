#ifndef AB_JSON_H
#define AB_JSON_H

#include "ab/base.h"

#include <stdio.h>

/* Escapes each non-ASCII byte as one U+00xx scalar; preserves arbitrary bytes. */
bool ab_json_bytes(FILE *file, AbSpan value);

#endif
