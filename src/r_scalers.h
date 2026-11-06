/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef R_SCALERS_H
#define R_SCALERS_H
#include <stdint.h>

/* Same loop macros with byte stores or low-detail word stores. */
typedef void column_scaler_t(uint8_t *dest, unsigned pitch, unsigned count,
                              const uint8_t *source, const uint8_t *map,
                              uint32_t frac, uint32_t step);
typedef void span_scaler_t(uint8_t *dest, unsigned advance, unsigned count,
                            const uint8_t *source, const uint8_t *map,
                            uint32_t frac, uint32_t step);
column_scaler_t R_ScaleColumnWord;
span_scaler_t R_ScaleSpanWord;
void R_ScaleColumn(uint8_t *dest, unsigned pitch,
                   unsigned count, const uint8_t *source,
                   const uint8_t *map,
                   uint32_t frac, uint32_t step);

void R_ScaleSpan(uint8_t *dest, unsigned advance,
                 unsigned count, const uint8_t *source, const uint8_t *map,
                 uint32_t frac, uint32_t step);

/* Original 64x64 flat format: Y6.10 in the high half, X6.10 in the low half.
 * Packed addition has the same cross-half carry behaviour as the old scaler. */
static inline uint32_t R_PackSpan(uint32_t x, uint32_t y)
{
    return ((x >> 6) & 0xffffu) | ((y << 10) & 0xffff0000u);
}
#endif
