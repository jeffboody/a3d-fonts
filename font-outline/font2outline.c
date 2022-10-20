/*
 * Copyright (c) 2022 Jeff Boody
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include <stdlib.h>
#include <freetype2/ft2build.h>
#include <freetype2/freetype/ftglyph.h>
#include FT_FREETYPE_H

#define LOG_TAG "font2outline"
#include "libcc/cc_log.h"

// The 26.6 fixed float format used to define fractional
// pixel coordinates. Here, 1 unit = 1/64 pixel.
// https://freetype.org/freetype2/docs/reference/ft2-base_interface.html#ft_glyph_metrics
// https://freetype.org/freetype1/docs/api/freetype1.txt
FT_Pos TT_F26Dot6 = 64;

// font height includes line spacing
#define FONT_HEIGHT 100
#define FONT_SIZE   90

/***********************************************************
* private                                                  *
***********************************************************/

static int compute_top(FT_Face face, int c, FT_Pos* top)
{
	assert(face);
	assert(top);

	// treat the "unit separator" as the cursor
	// but give it the half width of '.'
	int glyph_c = c;
	if(c == 31)
	{
		glyph_c = '.';
	}

	FT_UInt glyph_index = FT_Get_Char_Index(face, (FT_ULong) glyph_c);
	if(glyph_index == 0)
	{
		LOGE("FT_Get_Char_Index failed c=0x%X=%c", glyph_c, (char) glyph_c);
		return 0;
	}

	if(FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT) != 0)
	{
		LOGE("FT_Load_Glyph failed");
		return 0;
	}

	FT_Glyph_Metrics* metrics = &(face->glyph->metrics);
	if(metrics->horiBearingY > *top)
	{
		*top = metrics->horiBearingY;
	}
	return 1;
}

static float pos2float(FT_Pos pos)
{
	FT_Pos h = FONT_HEIGHT*TT_F26Dot6;
	return ((float) pos)/((float) h);
}

/***********************************************************
* public                                                   *
***********************************************************/

int main(int argc, char** argv)
{
	// FreeType Outlines
	// https://freetype.org/freetype2/docs/glyphs/glyphs-6.html

	if(argc != 3)
	{
		LOGE("usage: %s <font.ttf> <outline.json>",
		     argv[0]);
		return EXIT_FAILURE;
	}
	const char* fname = argv[1];
	const char* oname = argv[2];

	FT_Library library;
	if(FT_Init_FreeType(&library) != 0)
	{
		LOGE("FT_Init_FreeType failed");
		return EXIT_FAILURE;
	}

	FT_Face face;
	if(FT_New_Face(library, fname, 0, &face) != 0)
	{
		LOGE("FT_New_Face failed");
		goto fail_face;
	}

	if(FT_Set_Pixel_Sizes(face, 0, FONT_SIZE) != 0)
	{
		LOGE("FT_Set_Pixel_Sizes failed");
		goto fail_pixel_size;
	}

	// measure the maximum distance from the baseline to the
	// top of any printable ascii character
	// ignore the "unit separator" character (31)
	int    c;
	FT_Pos top = 0;
	for(c = 32; c <= 126; ++c)
	{
		if(compute_top(face, c, &top) == 0)
		{
			goto fail_top;
		}
	}

	FILE* f = fopen(oname, "w");
	if(f == NULL)
	{
		LOGE("invalid %s", oname);
		goto fail_fopen;
	}

	fprintf(f, "[");

	// output printable ascii characters as JSON
	// treat the "unit separator" as the cursor
	for(c = 32; c <= 126; ++c)
	{
		if(c > 32)
		{
			fprintf(f, ",");
		}
		fprintf(f, "{\"i\":%i", c);

		FT_UInt glyph_index = FT_Get_Char_Index(face, (FT_ULong) c);
		if(glyph_index == 0)
		{
			LOGE("FT_Get_Char_Index failed c=0x%X=%c",
			     (unsigned int) c, c);
			goto fail_get_char_index;
		}

		if(FT_Load_Glyph(face, glyph_index, FT_LOAD_DEFAULT) != 0)
		{
			LOGE("FT_Load_Glyph failed");
			goto fail_load_glyph;
		}

		FT_Outline*       outline = &face->glyph->outline;
		FT_Glyph_Metrics* metrics = &face->glyph->metrics;

		// check glyph position
		FT_Pos w    = metrics->horiAdvance;
		FT_Pos h    = FONT_HEIGHT*TT_F26Dot6;
		FT_Pos offx = metrics->horiBearingX;
		FT_Pos offy = h - top;

		// freetype has a weird origin leading to
		// some characters who are outside the expected
		// bounding box
		if(offx < 0.0f)
		{
			w    += -offx;
			offx  = 0;
		}
		fprintf(f, ",\"w\":%f", pos2float(w));
		fprintf(f, ",\"h\":%f", pos2float(h));

		fprintf(f, ",\"np\":%i", outline->n_points);
		fprintf(f, ",\"p\":[");
		int i;
		for(i = 0; i < outline->n_points; ++i)
		{
			if(i > 0)
			{
				fprintf(f, ",");
			}
			fprintf(f, "%f,%f",
			        pos2float(outline->points[i].x),
			        pos2float((h - (outline->points[i].y + offy))));
		}
		fprintf(f, "]");

		fprintf(f, ",\"t\":[");
		for(i = 0; i < outline->n_points; ++i)
		{
			if(i > 0)
			{
				fprintf(f, ",");
			}
			fprintf(f, "%i", (int) FT_CURVE_TAG(outline->tags[i]));
		}
		fprintf(f, "]");

		fprintf(f, ",\"nc\":%i", outline->n_contours);
		fprintf(f, ",\"c\":[");
		for(i = 0; i < outline->n_contours; ++i)
		{
			if(i > 0)
			{
				fprintf(f, ",");
			}
			fprintf(f, "%i", (int) (outline->contours[i]));
		}
		fprintf(f, "]");

		fprintf(f, "}");
	}

	fprintf(f, "]");

	fclose(f);
	FT_Done_Face(face);
	FT_Done_FreeType(library);

	// success;
	return EXIT_SUCCESS;

	// failure
	fail_load_glyph:
	fail_get_char_index:
		fclose(f);
	fail_fopen:
	fail_top:
	fail_pixel_size:
	fail_face:
		FT_Done_FreeType(library);
	return EXIT_FAILURE;
}
