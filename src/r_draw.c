//
// Copyright (C) 1993-1996 Id Software, Inc.
// Copyright (C) 2016-2017 Alexey Khokholov (Nuke.YKT)
// Copyright (C) 2017 Alexandre-Xavier Labonté-Lamoureux
// Copyright (C) 2023 Frenkel Smeijers
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

// R_draw.c

#include "doomdef.h"
#include "r_local.h"
#include "i_video.h"
#include "r_scalers.h"
#include "r_colormaps.h"

/*

All drawing to the view buffer is accomplished in this file.  The other refresh
files only know about ccordinates, not the architecture of the frame buffer.

*/

int32_t viewwidth, scaledviewwidth, viewheight, viewwindowx, viewwindowy;

/*
==================
=
= R_DrawColumn
=
= Source is the top of the column to scale
=
==================
*/

lighttable_t	*dc_colormap;
int32_t			dc_x			__attribute__ ((externally_visible));
int32_t			dc_yl			__attribute__ ((externally_visible));
int32_t			dc_yh			__attribute__ ((externally_visible));
fixed_t			dc_iscale		__attribute__ ((externally_visible));
fixed_t			dc_texturemid	__attribute__ ((externally_visible));
byte			*dc_source		__attribute__ ((externally_visible));		// first pixel in a column (possibly virtual)

static void R_DrawColumnShared(int low)
{
    int count = dc_yh - dc_yl + 1;
    if (count <= 0) return;
#ifdef RANGECHECK
    if (dc_x < 0 || (dc_x << low) >= scaledviewwidth || dc_yl < 0 || dc_yh >= viewheight)
        I_Error("Bad column at %i: %i..%i", dc_x, dc_yl, dc_yh);
#endif
    byte *dest = I_ColumnAddress(destview, dc_x << low, dc_yl, low);
    uint32_t frac = (uint32_t)dc_texturemid + (uint32_t)(dc_yl - centery) * dc_iscale;
    column_scaler_t *scale = I_WordPixels(low) ? R_ScaleColumnWord : R_ScaleColumn;
    scale(dest, video_pitch, count, dc_source, dc_colormap, frac, dc_iscale);
    unsigned second = I_SoftwarePlaneOffset(low);
    if (second)
        R_ScaleColumn(dest + second, video_pitch, count, dc_source, dc_colormap, frac, dc_iscale);
}

void R_DrawColumn (void)
{
    R_DrawColumnShared(0);
}

void R_DrawColumnLow (void)
{
    R_DrawColumnShared(1);
}


#define FUZZTABLE	50
#define FUZZOFF 1
static const int32_t		fuzzoffset[FUZZTABLE] = {
FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF,FUZZOFF,-FUZZOFF,FUZZOFF
};
static int32_t fuzzpos = 0;

void R_DrawFuzzColumn (void)
{
    if (!dc_yl) dc_yl = 1;
    if (dc_yh == viewheight - 1) dc_yh = viewheight - 2;
    int count = dc_yh - dc_yl + 1;
    if (count <= 0) return;
    unsigned x = dc_x << detailshift;
    I_ReadPlane(x);
    byte *dest = I_ColumnAddress(destview, x, dc_yl, detailshift);
    unsigned second = I_SoftwarePlaneOffset(detailshift);
    while (count--) {
        byte pixel = colormaps[6 * 256 + dest[fuzzoffset[fuzzpos] * (int)video_pitch]];
        if (I_WordPixels(detailshift)) *(uint16_t *)dest = pixel * 0x101u;
        else *dest = pixel;
        if (second) dest[second] = pixel;
        if (++fuzzpos == FUZZTABLE) fuzzpos = 0;
        dest += video_pitch;
    }
}

static byte *translated_colormaps;

/*
========================
=
= R_InitTranslatedColormaps
=
========================
*/

void R_InitTranslatedColormaps(void)
{
    unsigned bytes = colormap_count * COLORMAP_SIZE;
    translated_colormaps = Z_Malloc(PLAYER_TRANSLATIONS * bytes + 255, PU_STATIC, 0);
    translated_colormaps = (byte *)(((uintptr_t)translated_colormaps + 255) & ~(uintptr_t)255);
    R_BuildTranslatedColormaps(translated_colormaps, colormaps, colormap_count);
}

lighttable_t *R_TranslatedColormap(lighttable_t *map, unsigned translation)
{
    unsigned offset = (unsigned)(map - colormaps);
#ifdef RANGECHECK
    if (translation >= PLAYER_TRANSLATIONS || offset % COLORMAP_SIZE
        || offset / COLORMAP_SIZE >= colormap_count) I_Error("Invalid translated colormap");
#endif
    return translated_colormaps + translation * colormap_count * COLORMAP_SIZE + offset;
}

/*
================
=
= R_DrawSpan
=
================
*/

int32_t			ds_y;
int32_t			ds_x1;
int32_t			ds_x2;
lighttable_t	*ds_colormap;
fixed_t			ds_xfrac;
fixed_t			ds_yfrac;
fixed_t			ds_xstep;
fixed_t			ds_ystep;
byte			*ds_source;		// start of a 64*64 tile image

static void R_DrawSpanShared(int low)
{
    if (ds_x2 < ds_x1) return;
#ifdef RANGECHECK
    if (ds_x1 < 0 || (ds_x2 << low) >= scaledviewwidth || ds_y < 0 || ds_y >= viewheight)
        I_Error("Bad span: %i..%i at %i", ds_x1, ds_x2, ds_y);
#endif
    unsigned step = I_SpanStep(low);
    uint32_t frac = R_PackSpan(ds_xfrac, ds_yfrac);
    uint32_t fracstep = R_PackSpan(ds_xstep, ds_ystep);
    span_scaler_t *scale = I_WordPixels(low) ? R_ScaleSpanWord : R_ScaleSpan;
    for (unsigned pass = 0; pass < I_SpanPasses(low); pass++) {
        int first = ds_x1 + pass;
        if (first > ds_x2) break;
        unsigned count = (ds_x2 - first) / step + 1;
        byte *dest = I_ColumnAddress(destview, first << low, ds_y, low);
        scale(dest, I_SpanAdvance(low), count, ds_source, ds_colormap,
                    frac + fracstep * pass, fracstep * step);
        unsigned second = I_SoftwarePlaneOffset(low);
        if (second)
            R_ScaleSpan(dest + second, I_SpanAdvance(low), count, ds_source, ds_colormap,
                        frac + fracstep * pass, fracstep * step);
    }
}

void R_DrawSpan (void)
{
    R_DrawSpanShared(0);
} 

void R_DrawSpanLow (void) 
{
    R_DrawSpanShared(1);
}



/*
================
=
= R_InitBuffer
=
=================
*/

void R_InitBuffer (int32_t width, int32_t height)
{
	viewwindowx = (SCREENWIDTH-width) >> 1;

	if (width == SCREENWIDTH)
		viewwindowy = 0;
	else
		viewwindowy = (SCREENHEIGHT-SBARHEIGHT-height) >> 1;
}

 


/*
================
=
= R_FillBackScreen
=
= Fills the back screen with a pattern for variable screen sizes
= Also draws a beveled edge.
=================
*/

void R_FillBackScreen (void)
{
	byte		*src, *dest;
	int32_t		i, j;
	int32_t		x, y;
	patch_t		*patch;
	char		name1[] = "FLOOR7_2";
	char		name2[] = "GRNROCK";	
	char		*name;
	
	if (scaledviewwidth == SCREENWIDTH)
		return;
	
	if (commercial)
		name = name2;
	else
		name = name1;
    
	src = W_CacheLumpName (name, PU_CACHE);
	dest = screens[1];
	 
	for (y=0 ; y<SCREENHEIGHT-SBARHEIGHT ; y++)
	{
		for (x=0 ; x<SCREENWIDTH/64 ; x++)
		{
			memcpy (dest, src+((y&63)<<6), 64);
			dest += 64;
		}
	}
	
	patch = W_CacheLumpName ("brdr_t",PU_CACHE);
	for (x=0 ; x<scaledviewwidth ; x+=8)
		V_DrawPatch (viewwindowx+x,viewwindowy-8,1,patch);
	patch = W_CacheLumpName ("brdr_b",PU_CACHE);
	for (x=0 ; x<scaledviewwidth ; x+=8)
		V_DrawPatch (viewwindowx+x,viewwindowy+viewheight,1,patch);
	patch = W_CacheLumpName ("brdr_l",PU_CACHE);
	for (y=0 ; y<viewheight ; y+=8)
		V_DrawPatch (viewwindowx-8,viewwindowy+y,1,patch);
	patch = W_CacheLumpName ("brdr_r",PU_CACHE);
	for (y=0 ; y<viewheight ; y+=8)
		V_DrawPatch (viewwindowx+scaledviewwidth,viewwindowy+y,1,patch);

	V_DrawPatch (viewwindowx-8, viewwindowy-8, 1,
		W_CacheLumpName ("brdr_tl",PU_CACHE));
	V_DrawPatch (viewwindowx+scaledviewwidth, viewwindowy-8, 1,
		W_CacheLumpName ("brdr_tr",PU_CACHE));
	V_DrawPatch (viewwindowx-8, viewwindowy+viewheight, 1,
		W_CacheLumpName ("brdr_bl",PU_CACHE));
	V_DrawPatch (viewwindowx+scaledviewwidth, viewwindowy+viewheight, 1,
		W_CacheLumpName ("brdr_br",PU_CACHE));


}


void R_VideoErase (uint32_t ofs, int32_t count)
{
    while (count > 0) {
        unsigned x = ofs % SCREENWIDTH, y = ofs / SCREENWIDTH;
        unsigned width = SCREENWIDTH - x;
        if (width > (unsigned)count) width = count;
        I_BlitRect(screens[1], SCREENWIDTH, x, y, width, 1);
        ofs += width;
        count -= width;
    }
}


/*
================
=
= R_DrawViewBorder
=
= Draws the border around the view for different size windows
=
=================
*/

void R_DrawViewBorder (void)
{ 
    int32_t		top, side, ofs, i;
 
	if (scaledviewwidth == SCREENWIDTH)
		return;
  
	top = ((SCREENHEIGHT-SBARHEIGHT)-viewheight)/2;
	side = (SCREENWIDTH-scaledviewwidth)/2;

//
// copy top and one line of left side
//
	R_VideoErase (0, top*SCREENWIDTH+side);
 
//
// copy one line of right side and bottom
//
	ofs = (viewheight+top)*SCREENWIDTH-side;
	R_VideoErase (ofs, top*SCREENWIDTH+side);
 
//
// copy sides using wraparound
//
	ofs = top*SCREENWIDTH + SCREENWIDTH-side;
	side <<= 1;
    
	for (i=1 ; i<viewheight ; i++)
	{
		R_VideoErase (ofs, side);
		ofs += SCREENWIDTH;
	}
}
 
 
