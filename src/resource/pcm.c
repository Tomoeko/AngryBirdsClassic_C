#include "ab/pcm.h"

#include <stdlib.h>
#include <string.h>

void ab_pcm16_free(AbPcm16 *pcm) {
    if (pcm) {
        free(pcm->samples);
        memset(pcm, 0, sizeof(*pcm));
    }
}
