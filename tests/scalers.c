/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../src/r_scalers.h"
#include "../src/r_colormaps.h"

int main(void)
{
    uint8_t texture[4096], map[256], translate[256], combined[256];
    uint8_t pixels[320 * 8], planes[4][80], packed[320];
    for (unsigned i = 0; i < 4096; i++) texture[i] = (i * 13 + i / 64) & 255;
    for (unsigned i = 0; i < 256; i++) { map[i] = i ^ 0x55; translate[i] = 255 - i; }

    for (unsigned i = 0; i < 256; i++) combined[i] = map[translate[i]];

    /* Compose every player colour and all lighting/special tables. Check exact
     * equivalence to the former two-lookup path, including custom map counts. */
    static uint8_t maps[40 * 256], translated[3 * 40 * 256 + 1];
    for (unsigned i = 0; i < sizeof(maps); i++) maps[i] = (i * 19 + i / 256) & 255;
    const unsigned counts[] = {1, 34, 40}, ramps[] = {0x60, 0x40, 0x20};
    for (unsigned n = 0; n < 3; n++) {
        unsigned count = counts[n];
        memset(translated, 0xa5, sizeof(translated));
        R_BuildTranslatedColormaps(translated, maps, count);
        assert(translated[3 * count * 256] == 0xa5);
        for (unsigned t = 0; t < 3; t++)
            for (unsigned light = 0; light < count; light++)
                for (unsigned pixel = 0; pixel < 256; pixel++) {
                    unsigned remap = pixel >= 0x70 && pixel <= 0x7f ? ramps[t] + (pixel & 15) : pixel;
                    assert(translated[(t * count + light) * 256 + pixel] == maps[light * 256 + remap]);
                }
    }

    /* Column sampling, row strides, low detail and translated sprites. */
    for (unsigned pitch = 80; pitch <= 320; pitch *= 4) {
        memset(pixels, 0xa5, sizeof(pixels));
        R_ScaleColumn(pixels + 3, pitch, 4, texture, map, 3 << 16, 1 << 15);
        const unsigned samples[] = {3, 3, 4, 4};
        for (unsigned y = 0; y < 4; y++) {
            assert(pixels[y * pitch + 2] == 0xa5 && pixels[y * pitch + 5] == 0xa5);
            assert(pixels[y * pitch + 3] == map[texture[samples[y]]]);
            assert(pixels[y * pitch + 4] == 0xa5);
        }
        R_ScaleColumn(pixels, pitch, 3, texture, combined, 127 << 16, 1 << 16);
        assert(pixels[0] == map[translate[texture[127]]]);
        assert(pixels[pitch] == map[translate[texture[0]]]);
        assert(pixels[pitch * 2] == map[translate[texture[1]]]);
    }

    /* All unrolled-loop tails, original and precomposed maps at different destination offsets. */
    static uint8_t backing[2 * 65536 + 8192];
    const unsigned offsets[] = {0, 1, 65536};
    for (unsigned count = 0; count <= 65; count++) {
        for (unsigned dup = 0; dup < 3; dup++) {
            for (unsigned trans = 0; trans < 2; trans++) {
                memset(backing, 0xa5, sizeof(backing));
                R_ScaleColumn(backing + 2 + offsets[dup], 80, count, texture,
                              trans ? combined : map, 125u << 16, 1u << 15);
                uint32_t frac = 125u << 16;
                for (unsigned y = 0; y < count; y++, frac += 1u << 15) {
                    unsigned sample = texture[(frac >> 16) & 127];
                    if (trans) sample = translate[sample];
                    assert(backing[2 + offsets[dup] + y * 80] == map[sample]);
                }
                assert(backing[2 + offsets[dup] + count * 80] == 0xa5);
            }
        }
        memset(backing, 0xa5, sizeof(backing));
        uint32_t frac = R_PackSpan(62u << 16, 63u << 16);
        uint32_t step = R_PackSpan(3u << 15, 1u << 15);
        R_ScaleSpan(backing, 2, count, texture, map, frac, step);
        for (unsigned x = 0; x < count; x++, frac += step) {
            unsigned spot = ((frac >> 20) & 4032) | ((frac >> 10) & 63);
            assert(backing[x * 2] == map[texture[spot]]);
            assert(backing[x * 2 + 1] == 0xa5);
        }
        assert(backing[count * 2] == 0xa5);
    }

    /* Low-detail packed output scales once, then stores a repeated word. */
    for (unsigned count = 0; count <= 65; count++) {
        memset(backing, 0xa5, sizeof(backing));
        R_ScaleColumnWord(backing + 2, 80, count, texture, map, 127u << 16, 1u << 15);
        uint32_t frac = 127u << 16;
        for (unsigned y = 0; y < count; y++, frac += 1u << 15) {
            unsigned pixel = map[texture[(frac >> 16) & 127]];
            assert(backing[2 + y * 80] == pixel && backing[3 + y * 80] == pixel);
            assert(backing[4 + y * 80] == 0xa5);
        }
        assert(backing[2 + count * 80] == 0xa5);
        memset(backing, 0xa5, sizeof(backing));
        frac = R_PackSpan(62u << 16, 63u << 16);
        uint32_t step = R_PackSpan(3u << 15, 1u << 15);
        R_ScaleSpanWord(backing, 2, count, texture, map, frac, step);
        for (unsigned x = 0; x < count; x++, frac += step) {
            unsigned spot = ((frac >> 20) & 4032) | ((frac >> 10) & 63);
            assert(backing[x * 2] == map[texture[spot]]);
            assert(backing[x * 2 + 1] == backing[x * 2]);
        }
        assert(backing[count * 2] == 0xa5);
    }

    /* Check packed stepping against two 16-bit accumulators, including
     * negative steps, coordinate wrap and carry from X into Y. */
    const uint32_t coordinates[][4] = {
        {0x003fffffu, 0x003fffffu, 0x00018040u, 0xffff8000u},
        {0xfffe1234u, 0x00023456u, 0xffff4000u, 0x00008040u},
        {0x00018040u, 0x00024080u, 0, 0},
    };
    assert(R_PackSpan(0x00018040u, 0x00024080u) == 0x09020601u);
    for (unsigned c = 0; c < 3; c++) {
        uint32_t frac = R_PackSpan(coordinates[c][0], coordinates[c][1]);
        uint32_t step = R_PackSpan(coordinates[c][2], coordinates[c][3]);
        R_ScaleSpan(backing, 1, 65, texture, map, frac, step);
        unsigned x = (coordinates[c][0] >> 6) & 65535;
        unsigned y = (coordinates[c][1] >> 6) & 65535;
        unsigned dx = (coordinates[c][2] >> 6) & 65535;
        unsigned dy = (coordinates[c][3] >> 6) & 65535;
        for (unsigned i = 0; i < 65; i++) {
            assert(backing[i] == map[texture[(y >> 10) * 64 + (x >> 10)]]);
            unsigned next_x = x + dx;
            y = (y + dy + (next_x >> 16)) & 65535;
            x = next_x & 65535;
        }
    }

    /* A packed run and four plane runs must produce exactly the same pixels,
     * including unaligned endpoints, fractional steps and 64x64 wrapping. */
    for (unsigned low = 0; low <= 1; low++) {
        memset(packed, 0xa5, sizeof(packed));
        memset(planes, 0xa5, sizeof(planes));
        unsigned start = 1, end = 13, step = 4 >> low;
        uint32_t xf = 62u << 16, yf = 63u << 16, xs = 3u << 15, ys = 1u << 15;
        uint32_t frac = R_PackSpan(xf, yf), fracstep = R_PackSpan(xs, ys);
        span_scaler_t *scale = low ? R_ScaleSpanWord : R_ScaleSpan;
        scale(packed + (start << low), 1 << low, end - start + 1,
              texture, map, frac, fracstep);
        for (unsigned pass = 0; pass < step; pass++) {
            unsigned first = start + pass, plane = (first << low) & 3;
            R_ScaleSpan(planes[plane] + ((first << low) >> 2), 1,
                        (end - first) / step + 1, texture, map,
                        frac + fracstep * pass, fracstep * step);
            if (low) memcpy(planes[plane + 1], planes[plane], 80);
        }
        for (unsigned x = 0; x < 80; x++)
            assert(packed[x] == planes[x & 3][x >> 2]);
    }
    puts("SCALERS-PASS");
    return 0;
}
