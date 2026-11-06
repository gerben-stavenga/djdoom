/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef R_COLORMAPS_H
#define R_COLORMAPS_H
#include <stdint.h>

#define PLAYER_TRANSLATIONS 3
#define COLORMAP_SIZE 256

/* Compose lighting after player colour translation, for every map in the WAD. */
static inline void R_BuildTranslatedColormaps(uint8_t *dest, const uint8_t *maps,
                                             unsigned count)
{
    static const unsigned ramps[PLAYER_TRANSLATIONS] = {0x60, 0x40, 0x20};
    for (unsigned t = 0; t < PLAYER_TRANSLATIONS; t++)
        for (unsigned light = 0; light < count; light++)
            for (unsigned pixel = 0; pixel < COLORMAP_SIZE; pixel++) {
                unsigned translated = pixel >= 0x70 && pixel <= 0x7f
                    ? ramps[t] + (pixel & 15) : pixel;
                dest[(t * count + light) * COLORMAP_SIZE + pixel] =
                    maps[light * COLORMAP_SIZE + translated];
            }
}
#endif
