/*
 * Copyright (c) 2018 Jeff Boody
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
#include <assert.h>
#include <unistd.h>
#include "texgz/texgz_tex.h"
#include "texgz/texgz_png.h"
#include <freetype2/ft2build.h>
#include <freetype2/freetype/ftglyph.h>
#include FT_FREETYPE_H

#define LOG_TAG "fontgen"
#include "libcc/cc_log.h"

/***********************************************************
* private                                                  *
***********************************************************/

#define FONTGEN_TEX_WIDTH  1024
#define FONTGEN_TEX_HEIGHT 1024

typedef struct
{
	int x;
	int y;
	int w;
} glyph_coords_t;

static int fontgen_exportXml(glyph_coords_t* coords,
                             const char* xmlname,
                             const char* name,
                             int font_size,
                             int tex_height)
{
	assert(coords);
	assert(xmlname);
	assert(name);

	FILE* f = fopen(xmlname, "w");
	if(f == NULL)
	{
		LOGE("fopen %s failed", xmlname);
		return 0;
	}

	fprintf(f, "%s", "<?xml version='1.0' encoding='UTF-8'?>\n");
	fprintf(f, "<font name=\"%s\" size=\"%i\" h=\"%i\">\n",
	        name, font_size, tex_height);

	// treat the "unit separator" as the cursor
	int c;
	for(c = 31; c <= 126; ++c)
	{
		fprintf(f, "\t<coords c=\"0x%X\" x=\"%i\" y=\"%i\" w=\"%i\" />\n",
		        c, coords[c].x, coords[c].y, coords[c].w);
	}

	fprintf(f, "</font>\n");
	fclose(f);

	return 1;
}

static void fontgen_printMetrics(int c,
                                 FT_Bitmap* bitmap,
                                 FT_Glyph_Metrics* metrics)
{
	assert(bitmap);
	assert(metrics);

	LOGI("char=0x%X=%c", c, (char) c);
	LOGI("Bitmap: rows=%u, width=%u, pitch=%i, num_grays=%u",
	     bitmap->rows, bitmap->width, bitmap->pitch, bitmap->num_grays);
	LOGI("Metrics: horiBearingX=%f, horiBearingY=%f, horiAdvance=%f",
	     (float) metrics->horiBearingX/64.0f,
	     (float) metrics->horiBearingY/64.0f,
	     (float) metrics->horiAdvance/64.0f);
}

static int fontgen_top(FT_Face face, int c, int* top)
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

	int glyph_index = FT_Get_Char_Index(face, (FT_ULong) glyph_c);
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

	if(FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL) != 0)
	{
		LOGE("FT_Render_Glyph failed");
		return 0;
	}

	FT_Glyph_Metrics* metrics = &(face->glyph->metrics);
	int horiBearingY = metrics->horiBearingY/64;
	//int bitmap_top = face->glyph->bitmap_top;
	if(horiBearingY > *top)
	{
		*top = horiBearingY;
	}
	return 1;
}

static int fontgen_render(FT_Face face,
                          int top, int tex_height, int font_size, int c,
                          texgz_tex_t* tex, int* x, int* y,
                          glyph_coords_t* coords)
{
	assert(face);
	assert(tex);
	assert(x);
	assert(y);
	assert(coords);

	// treat the "unit separator" as the cursor
	// but give it the half width of '.'
	int is_cursor = 0;
	int glyph_c   = c;
	if(c == 31)
	{
		is_cursor = 1;
		glyph_c   = '.';
	}

	// workaround for BarlowCondensed font where the right paren
	// is cropped to the wrong width when rendered with freetype
	// https://fonts.google.com/specimen/Barlow+Condensed
	int reverse = 0;
	if(c == (int) ')')
	{
		reverse = 1;
		glyph_c   = '(';
	}

	int glyph_index = FT_Get_Char_Index(face, (FT_ULong) glyph_c);
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

	if(FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL) != 0)
	{
		LOGE("FT_Render_Glyph failed");
		return 0;
	}

	FT_Bitmap*        bitmap  = &(face->glyph->bitmap);
	FT_Glyph_Metrics* metrics = &(face->glyph->metrics);
	fontgen_printMetrics(c, bitmap, metrics);

	// origin
	int origin_y = top + (tex_height - font_size)/2;
	LOGI("Glyph: origin_y=%i, top=%i", origin_y, top);

	// check texture position
	int xx   = *x;
	int yy   = *y;
	int w    = metrics->horiAdvance/64;
	int h    = tex_height;
	int offx = metrics->horiBearingX/64;
	int offy = origin_y - metrics->horiBearingY/64;
	if(xx + w > FONTGEN_TEX_WIDTH)
	{
		xx  = 0;
		yy += tex_height;
		if(yy + h > FONTGEN_TEX_HEIGHT)
		{
			LOGE("invalid x=%i, y=%i", xx, yy);
			return 0;
		}
	}

	// freetype has a weird origin leading to
	// some characters who are outside the expected
	// bounding box
	if(offx < 0)
	{
		LOGI("Tex: w=%i, offx=%i", w, offx);
		w    += -offx;
		offx  = 0;
	}
	LOGI("Tex: c=0x%x=%c, x=%i, y=%i, w=%i, h=%i, offx=%i, offy=%i",
	     c, (char) c, xx, yy, w, h, offx, offy);

	// fill buffer
	int i;
	int j;
	unsigned char* pixels = tex->pixels;
	if(is_cursor)
	{
		int width2 = bitmap->width/2;
		if(width2 == 0)
		{
			width2 = 1;
		}

		for(i = 0; i < tex_height; ++i)
		{
			for(j = 0; j < width2; ++j)
			{
				int dst = i*FONTGEN_TEX_WIDTH + (xx + j + offx);
				pixels[dst] = 0xFF;
			}
		}
	}
	else
	{
		for(i = 0; i < bitmap->rows; ++i)
		{
			for(j = 0; j < bitmap->width; ++j)
			{
				int src = i*bitmap->width + j;
				int dst = (yy + i + offy)*FONTGEN_TEX_WIDTH + (xx + j + offx);
				if(reverse)
				{
					dst = (yy + i + offy)*FONTGEN_TEX_WIDTH +
					      (xx + (w - 1) - j - offx);
				}
				pixels[dst] = bitmap->buffer[src];
			}
		}
	}

	// fill coords
	coords[c].x = xx;
	coords[c].y = yy;
	coords[c].w = w;

	// print padded buffer
	printf("%s", "\n");
	for(i = 0; i < h; ++i)
	{
		printf("%2i: ", i);
		for(j = 0; j < w; ++j)
		{
			int idx = (yy + i)*FONTGEN_TEX_WIDTH + (xx + j);
			printf("%02X", pixels[idx]);
		}
		printf("%s", "\n");
	}
	printf("%s", "\n");

	// update next glyph position
	xx += w;
	*x = xx;
	*y = yy;
	return 1;
}

/***********************************************************
* public                                                   *
***********************************************************/

int main(int argc, char** argv)
{
	const char* string = NULL;
	if(argc == 5)
	{
		// a debug string may be rendered in place of font map
		string = argv[4];
	}
	else if(argc != 4)
	{
		LOGE("usage: %s <tex_height> <font.ttf> <font_name> [string]", argv[0]);
		return EXIT_FAILURE;
	}

	// parse tex_height
	int tex_height = strtol(argv[1], NULL, 0);
	if(tex_height <= 0)
	{
		LOGE("invalid %s", argv[1]);
		return EXIT_FAILURE;
	}

	// compute font_size
	int font_size = (90*tex_height)/100;
	if(font_size <= 0)
	{
		LOGE("invalid %s", argv[1]);
		return EXIT_FAILURE;
	}

	// parse filenames
	char fname[256];
	snprintf(fname, 256, "%s", argv[2]);
	fname[255] = '\0';

	const char* name = argv[3];

	char pngname[256];
	snprintf(pngname, 256, "%s-%i.png",
	         name, tex_height);
	pngname[255] = '\0';

	char xmlname[256];
	snprintf(xmlname, 256, "%s-%i.xml",
	         name, tex_height);
	xmlname[255] = '\0';

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

	if(FT_Set_Pixel_Sizes(face, 0, font_size) != 0)
	{
		LOGE("FT_Set_Pixel_Sizes failed");
		goto fail_pixel_size;
	}

	// measure printable ascii characters
	// because freetype has a weird origin
	// ignore the "unit separator" character
	int c;
	int top = 0;
	for(c = 32; c <= 126; ++c)
	{
		if(fontgen_top(face, c, &top) == 0)
		{
			goto fail_top;
		}
	}

	texgz_tex_t* tex = texgz_tex_new(FONTGEN_TEX_WIDTH,
	                                 FONTGEN_TEX_HEIGHT,
	                                 FONTGEN_TEX_WIDTH,
	                                 FONTGEN_TEX_HEIGHT,
	                                 TEXGZ_UNSIGNED_BYTE,
	                                 TEXGZ_ALPHA,
	                                 NULL);
	if(tex == NULL)
	{
		goto fail_tex;
	}

	// initialize coords
	glyph_coords_t coords[128];
	memset(coords, 0, sizeof(coords));

	int x = 0;
	int y = 0;
	if(string)
	{
		// render string from command line
		c = 0;
		while(string[c] != '\0')
		{
			// check for non-printable ascii characters
			// ignore the "unit separator" character
			if((string[c] < 32) || (string[c] > 126))
			{
				++c;
				continue;
			}

			if(fontgen_render(face,
			                  top, tex_height, font_size,
			                  (int) string[c],
			                  tex, &x, &y, coords) == 0)
			{
				goto fail_render;
			}

			++c;
		}
	}
	else
	{
		// render printable ascii characters
		// treat the "unit separator" as the cursor
		for(c = 31; c <= 126; ++c)
		{
			if(fontgen_render(face,
			                  top, tex_height, font_size, c,
			                  tex, &x, &y, coords) == 0)
			{
				goto fail_render;
			}
		}
	}

	// export xml
	if(fontgen_exportXml(coords, xmlname, name,
	                     font_size, tex_height) == 0)
	{
		goto fail_xml;
	}

	// crop and export tex
	if(texgz_tex_crop(tex, 0, 0,
	                  y + tex_height - 1,
	                  string ? x - 1 : FONTGEN_TEX_WIDTH - 1) == 0)
	{
		goto fail_crop;
	}

	if(texgz_png_export(tex, pngname) == 0)
	{
		goto fail_export;
	}

	texgz_tex_delete(&tex);
	FT_Done_Face(face);
	FT_Done_FreeType(library);

	// success;
	return EXIT_SUCCESS;

	// failure
	fail_export:
	fail_crop:
		unlink(xmlname);
	fail_xml:
	fail_render:
		texgz_tex_delete(&tex);
	fail_tex:
	fail_top:
	fail_pixel_size:
		FT_Done_Face(face);
	fail_face:
		FT_Done_FreeType(library);
	return EXIT_FAILURE;
}
