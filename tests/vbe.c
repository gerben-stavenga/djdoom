/* SPDX-License-Identifier: GPL-2.0-or-later */
/* DOS integration probe for the same VBE backend used by DOOM. */
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <dpmi.h>
#include <go32.h>
#include <sys/nearptr.h>

typedef unsigned char byte;
#define SCREENWIDTH 320
#define SCREENHEIGHT 200
static byte *screen;
static int grmode, usegamma, vbe_linear;
static byte gammatable[1][256];

static void textmode(void)
{
    __dpmi_regs r = {0};
    r.x.ax = 3;
    __dpmi_int(0x10, &r);
}

static void I_Error(const char *format, ...)
{
    va_list args;
    textmode();
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    puts("\nVBE-PROBE FAIL");
    exit(1);
}

#include "../src/dos/i_vbe.h"

int main(int argc, char **argv)
{
    byte pattern[64], result[64], palette[768];
    vbe_linear = argc > 1 && !strcmp(argv[1], "-vbe2");
    if (!__djgpp_nearptr_enable()) I_Error("nearptr unavailable");
    I_VBE_Init();
    for (unsigned i = 0; i < 64; i++) pattern[i] = i * 7 + 3;
    /* First/last pixels exercise the visible aperture and scanline pitch. */
    uint32_t offsets[] = {0, 17, 199 * vbe_pitch + 256,
        65520, 131056, /* Reads/writes spanning successive 64 KiB banks. */
    };
    for (unsigned i = 0; i < (vbe_linear ? 3 : 5); i++) {
        I_VBE_Copy(offsets[i], pattern, 64, 1);
        memset(result, 0, sizeof(result));
        I_VBE_Copy(offsets[i], result, 64, 0);
        if (memcmp(pattern, result, 64)) I_Error("VBE copy mismatch at %lu", (unsigned long)offsets[i]);
    }
    /* Bank-zero contents must survive visits to the higher banks. */
    I_VBE_Copy(0, result, 64, 0);
    /* Offset 17 overlaps this first block; only the first 17 bytes remain. */
    if (memcmp(pattern, result, 17)) I_Error("VBE bank contents lost");
    /* Give all three physical pages distinct first/last cache lines. */
    for (unsigned page = 0; page < VBE_PAGES; page++) {
        memset(pattern, 0x31 + page, sizeof(pattern));
        byte *target = I_VBE_SelectPage(page);
        /* Write through the renderer's actual VRAM pointer, with no upload. */
        memset(target, 0x31 + page, 64);
        memset(target + 199 * vbe_pitch + 256, 0x31 + page, 64);
    }
    for (unsigned flip = 0; flip < 6; flip++) {
        unsigned expected = (flip + 1) % VBE_PAGES;
        if (vbe_draw_page != expected || vbe_draw_page == vbe_visible_page)
            I_Error("VBE drawing into visible page");
        I_VBE_Flip();
        __dpmi_regs r = {0};
        r.x.ax = 0x4f07;
        r.x.bx = 1;
        I_VBE_Call(&r);
        if (r.x.cx || r.x.dx != expected * vbe_page_rows || vbe_visible_page != expected)
            I_Error("VBE display start did not flip to page %u", expected);
        memset(pattern, 0x31 + expected, sizeof(pattern));
        I_VBE_Copy(expected * vbe_page_bytes, result, 64, 0);
        if (memcmp(pattern, result, 64)) I_Error("VBE page contents lost");
        I_VBE_Copy(expected * vbe_page_bytes + 199 * vbe_pitch + 256, result, 64, 0);
        if (memcmp(pattern, result, 64)) I_Error("VBE page end contents lost");
    }
    for (unsigned i = 0; i < 256; i++) {
        gammatable[0][i] = i;
        palette[i * 3] = i;
        palette[i * 3 + 1] = i;
        palette[i * 3 + 2] = i;
    }
    I_VBE_SetPalette(palette);
    puts("VBE-DIRECT-TRIPLE-PASS");
    textmode();
    if (vbe_mapping.size) __dpmi_free_physical_address_mapping(&vbe_mapping);
    puts(vbe_linear ? "VBE-LFB-PASS" : "VBE-BANKED-PASS");
    return 0;
}
