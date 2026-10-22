#ifndef AB_VP8_FILTER_H
#define AB_VP8_FILTER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Filter count pixels along an edge. The caller supplies four pixels on each
 * side in the normal direction, including edge[-normal] and edge[0], for
 * every along step. Steps are positive byte distances in a padded plane.
 * macroblock selects the wider normal filter; simple always uses two taps.
 * The surrounding plane remains owned and bounded by the reconstruction host. */
void ab_vp8_filter_edge(uint8_t *edge, size_t normal, size_t along, size_t count,
                        uint8_t limit, uint8_t interior, uint8_t variance,
                        bool macroblock, bool simple);

#endif
