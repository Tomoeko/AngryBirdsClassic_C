#ifndef AB_PCM_H
#define AB_PCM_H

#include "ab/base.h"

typedef struct {
    const int16_t *samples;
    size_t frames;
    uint16_t channels;
} AbPcm16View;

typedef struct {
    int16_t *samples;
    size_t frames;
    uint32_t sample_rate;
    uint16_t channels;
} AbPcm16;

/* Owned interleaved samples in host byte order. Release only after every
 * borrowing mixer voice, binding and snapshot has expired. Clear on release. */
void ab_pcm16_free(AbPcm16 *pcm);

#endif
