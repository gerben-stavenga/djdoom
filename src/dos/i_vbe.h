/* SPDX-License-Identifier: GPL-2.0-or-later */
/* VBE 1.2/2.0 320x200x8 display backend. Included only by i_ibm.c. */
#include <sys/movedata.h>
#include <pc.h>

static unsigned vbe_version;

static unsigned vbe_pitch;
static uint32_t vbe_page_bytes; /* VRAM distance between page starts. */
static unsigned vbe_page_rows;
static unsigned vbe_visible_page, vbe_draw_page;
#define VBE_PAGES 3
static unsigned vbe_granularity, vbe_window_size, vbe_window;
static int vbe_bank = -1;
static __dpmi_meminfo vbe_mapping;

static unsigned I_VBE_U16(const byte *p)
{
    return p[0] | (p[1] << 8);
}

static uint32_t I_VBE_U32(const byte *p)
{
    return I_VBE_U16(p) | ((uint32_t)I_VBE_U16(p + 2) << 16);
}

static void I_VBE_Call(__dpmi_regs *r)
{
    if (__dpmi_int(0x10, r) != 0 || r->x.ax != 0x004f)
        I_Error("VBE BIOS call failed");
}

/* Reads as well as writes for the disk icon's save/restore operation. */
static void I_VBE_Copy(uint32_t offset, byte *data, unsigned size, int write)
{
    if (!vbe_linear) {
        while (size) {
            unsigned bank = offset / vbe_granularity;
            unsigned within = offset % vbe_granularity;
            unsigned chunk = vbe_window_size - within;
            if (chunk > size) chunk = size;
            if (vbe_bank != (int)bank) {
                __dpmi_regs r = {0};
                r.x.ax = 0x4f05;
                r.x.bx = vbe_window;
                r.x.dx = bank;
                I_VBE_Call(&r);
                vbe_bank = bank;
            }
            if (write) memcpy(screen + within, data, chunk);
            else memcpy(data, screen + within, chunk);
            offset += chunk;
            data += chunk;
            size -= chunk;
        }
    } else {
        if (write) memcpy(screen + offset, data, size);
        else memcpy(data, screen + offset, size);
    }
}

/* Same unthrottled page rotation as Mode X: no retrace wait. */
static void I_VBE_Flip(void)
{
    __dpmi_regs r = {0};
    r.x.ax = 0x4f07;
    r.x.dx = vbe_draw_page * vbe_page_rows;
    I_VBE_Call(&r);
    vbe_visible_page = vbe_draw_page;
    vbe_draw_page = (vbe_draw_page + 1) % VBE_PAGES;
}

/* The renderer writes into this VRAM address, never into a staging frame. */
static byte *I_VBE_SelectPage(unsigned page)
{
    uint32_t offset = page * vbe_page_bytes;
    if (!vbe_linear) {
        unsigned bank = offset / vbe_granularity;
        if (vbe_bank != (int)bank) {
            __dpmi_regs r = {0};
            r.x.ax = 0x4f05;
            r.x.bx = vbe_window;
            r.x.dx = bank;
            I_VBE_Call(&r);
            vbe_bank = bank;
        }
        return screen;
    } else {
        return screen + offset;
    }
}

static void I_VBE_SetPalette(byte *palette)
{
    if (vbe_version < 0x0200) {
        outportb(0x3c8, 0);
        for (unsigned i = 0; i < 768; i++)
            outportb(0x3c9, gammatable[usegamma][palette[i]] >> 2);
        return;
    }
    byte entries[256 * 4];
    for (int i = 0; i < 256; i++) {
        entries[i * 4] = gammatable[usegamma][palette[i * 3 + 2]] >> 2;
        entries[i * 4 + 1] = gammatable[usegamma][palette[i * 3 + 1]] >> 2;
        entries[i * 4 + 2] = gammatable[usegamma][palette[i * 3]] >> 2;
        entries[i * 4 + 3] = 0;
    }
    dosmemput(entries, sizeof(entries), __tb);
    __dpmi_regs r = {0};
    r.x.ax = 0x4f09;
    r.x.cx = 256;
    r.x.es = __tb >> 4;
    r.x.di = __tb & 15;
    I_VBE_Call(&r);
}

static void I_VBE_Init(void)
{
    byte info[512] = {0}, mode_info[256];
    unsigned modes[512], count = 0, chosen = 0xffff;
    __dpmi_regs r = {0};
    memcpy(info, "VBE2", 4);
    dosmemput(info, sizeof(info), __tb);
    r.x.ax = 0x4f00;
    r.x.es = __tb >> 4;
    r.x.di = __tb & 15;
    I_VBE_Call(&r);
    dosmemget(__tb, sizeof(info), info);
    vbe_version = I_VBE_U16(info + 4);
    if (memcmp(info, "VESA", 4) || vbe_version < (vbe_linear ? 0x0200 : 0x0102))
        I_Error(vbe_linear ? "VBE 2.0 or newer is required" : "VBE 1.2 or newer is required");
    uint32_t list = (I_VBE_U32(info + 14) >> 16) * 16
                  + (I_VBE_U32(info + 14) & 0xffff);
    /* Copy before 4F01h reuses the transfer buffer containing this list. */
    for (; count < 512; count++) {
        byte number[2];
        if (list + count * 2 > 0xffffe) I_Error("Invalid VBE mode list");
        dosmemget(list + count * 2, 2, number);
        modes[count] = I_VBE_U16(number);
        if (modes[count] == 0xffff) break;
    }
    for (unsigned i = 0; i < count; i++) {
        memset(&r, 0, sizeof(r));
        r.x.ax = 0x4f01;
        r.x.cx = modes[i];
        r.x.es = __tb >> 4;
        r.x.di = __tb & 15;
        if (__dpmi_int(0x10, &r) || r.x.ax != 0x004f) continue;
        dosmemget(__tb, sizeof(mode_info), mode_info);
        unsigned attr = I_VBE_U16(mode_info);
        /* NumberOfImagePages counts additional complete images after page 0. */
        unsigned image_pages = mode_info[29];
        if (I_VBE_U16(info + 4) >= 0x0300) {
            if (!vbe_linear) {
                image_pages = mode_info[52];
            } else {
                image_pages = mode_info[53];
            }
        }
        if (image_pages < VBE_PAGES - 1 || (vbe_version < 0x0200 && (attr & 0x20))) continue;
        if ((attr & 0x19) != 0x19 || I_VBE_U16(mode_info + 18) != 320
            || I_VBE_U16(mode_info + 20) != 200 || mode_info[25] != 8
            || mode_info[24] != 1 || mode_info[27] != 4
            || I_VBE_U16(mode_info + 16) < 320) continue;
        if (!vbe_linear) {
            if (attr & 0x40) continue; /* banked window unavailable */
            vbe_window = (mode_info[2] & 7) == 7 ? 0 : 1;
            if ((mode_info[2 + vbe_window] & 7) != 7) continue;
            vbe_granularity = I_VBE_U16(mode_info + 4) * 1024;
            vbe_window_size = I_VBE_U16(mode_info + 6) * 1024;
            unsigned segment = I_VBE_U16(mode_info + 8 + vbe_window * 2);
            if (!segment || !vbe_granularity || vbe_window_size > 65536
                || vbe_granularity > vbe_window_size) continue;
            screen = (byte *)(segment * 16 + __djgpp_conventional_base);
        } else {
            if (!(attr & 0x80) || !I_VBE_U32(mode_info + 40)) continue;
        }
        chosen = modes[i];
        break;
    }
    if (chosen == 0xffff) I_Error("No VBE 320x200x8 mode with three VRAM pages for this display backend");
    vbe_pitch = I_VBE_U16(mode_info + 16);
    if (vbe_linear) {
        if (I_VBE_U16(info + 4) >= 0x0300)
            vbe_pitch = I_VBE_U16(mode_info + 50);
    }
    if (vbe_pitch < SCREENWIDTH) I_Error("Invalid VBE scanline pitch");
    vbe_page_bytes = vbe_pitch * SCREENHEIGHT;
    if (!vbe_linear) {
        if (vbe_page_bytes > vbe_window_size)
            I_Error("VBE page does not fit in one bank window");
        /* A page starts on both a bank boundary and a scanline boundary.
         * For 320-byte pitch and 64 KiB granularity this is every fifth bank. */
        unsigned a = vbe_pitch, b = vbe_granularity;
        while (b) { unsigned rem = a % b; a = b; b = rem; }
        unsigned alignment = vbe_pitch / a * vbe_granularity;
        vbe_page_bytes = (vbe_page_bytes + alignment - 1) / alignment * alignment;
    }
    vbe_page_rows = vbe_page_bytes / vbe_pitch;
    uint32_t required = (VBE_PAGES - 1) * vbe_page_bytes + vbe_pitch * SCREENHEIGHT;
    if (required > I_VBE_U16(info + 18) * 65536u
        || vbe_page_rows * (VBE_PAGES - 1) + SCREENHEIGHT > 65535)
        I_Error("Insufficient VBE memory for three directly writable pages");
    memset(&r, 0, sizeof(r));
    r.x.ax = 0x4f02;
    r.x.bx = chosen;
    if (vbe_linear) {
        r.x.bx |= 0x4000;
    }
    I_VBE_Call(&r);
    grmode = true; /* Ensure later failures restore text mode. */
    if (vbe_linear) {
        vbe_mapping.address = I_VBE_U32(mode_info + 40);
        vbe_mapping.size = (required + 4095) & ~4095;
        if (__dpmi_physical_address_mapping(&vbe_mapping) != 0) {
            vbe_mapping.size = 0;
            I_Error("Cannot map VBE linear framebuffer");
        }
        screen = (byte *)(vbe_mapping.address + __djgpp_conventional_base);
    } else {
        vbe_bank = -1;
    }
    /* Validate and reset the display start before the first frame. */
    vbe_draw_page = 0;
    I_VBE_Flip();
}
