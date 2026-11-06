/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Destination addressing only. Scaling loops are independent of the backend. */
#ifndef I_VIDEO_H
#define I_VIDEO_H
#include "doomdef.h"
#if defined(__DJGPP__)
#include "dos/compiler.h"
#endif

typedef enum { VIDEO_VGA, VIDEO_MCGA, VIDEO_VBE1, VIDEO_VBE2 } video_mode_t;
extern video_mode_t video_mode;

typedef struct {
    byte *pixels;
    unsigned pitch;
} drawbuffer_t;

extern unsigned video_pitch;
drawbuffer_t I_GetDrawBuffer(void);

static inline int I_PlanarPixels(void) { return video_mode == VIDEO_VGA; }
static inline int I_VbePixels(void) { return video_mode >= VIDEO_VBE1; }
static inline unsigned I_SpanPasses(int low) { return I_PlanarPixels() ? 4 >> low : 1; }
static inline unsigned I_SpanStep(int low) { return I_SpanPasses(low); }
static inline unsigned I_SpanAdvance(int low) { return I_PlanarPixels() ? 1 : 1 << low; }
static inline unsigned I_WordPixels(int low) { return !I_PlanarPixels() && low; }

static inline unsigned I_SoftwarePlaneOffset(int low)
{
#ifndef __DJGPP__
    return I_PlanarPixels() && low ? 65536 : 0;
#else
    (void)low;
    return 0;
#endif
}

static inline byte *I_ColumnAddress(byte *base, unsigned x, unsigned y, int low)
{
    if (!I_PlanarPixels()) return base + y * video_pitch + x;
    unsigned plane = x & 3;
#ifdef __DJGPP__
    outpw(0x3c4, 2 | ((low ? 3 : 1) << (plane + 8)));
    return base + y * video_pitch + (x >> 2);
#else
    (void)low;
    return base + y * video_pitch + (x >> 2) + (plane << 16);
#endif
}

static inline void I_ReadPlane(unsigned x)
{
#ifdef __DJGPP__
    if (I_PlanarPixels()) outpw(0x3ce, 4 | ((x & 3) << 8));
#else
    (void)x;
#endif
}

static inline byte *I_ViewAddress(byte *base, unsigned x, unsigned y)
{
    return base + y * video_pitch + (I_PlanarPixels() ? x >> 2 : x);
}

/* Copy buffered HUD/UI rectangles into the current destination, never a
 * full-frame VESA staging blit at presentation time. */
static inline void I_BlitRect(const byte *source, unsigned source_pitch,
                             unsigned x, unsigned y, unsigned width, unsigned height)
{
    if (!I_PlanarPixels()) {
        for (unsigned row = y; row < y + height; row++)
            memcpy(destscreen + row * video_pitch + x, source + row * source_pitch + x, width);
        return;
    }
    for (unsigned pass = 0; pass < 4 && pass < width; pass++) {
        unsigned first = x + pass;
        byte *dest = I_ColumnAddress(destscreen, first, y, 0);
        const byte *src = source + y * source_pitch + first;
        unsigned count = (width - pass + 3) / 4;
        for (unsigned row = 0; row < height; row++) {
            for (unsigned i = 0; i < count; i++) dest[i] = src[i * 4];
            dest += video_pitch;
            src += source_pitch;
        }
    }
}
#endif
