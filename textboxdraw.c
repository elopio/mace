#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <err.h>

#include <cairo.h>
#include <freetype2/ft2build.h>
#include FT_FREETYPE_H
#include <utf8proc.h>

#include "mace.h"

static struct colour nfg = { 0, 0, 0 };

static bool
buffergrow(struct textbox *t, int hn)
{
  cairo_surface_t *sfc, *osfc;
  cairo_t *cr, *ocr;
  int h;
  
  h = cairo_image_surface_get_height(t->sfc);
  
  if (h > hn) {
    return true;
  }

  osfc = t->sfc;
  ocr = t->cr;
  
  sfc = cairo_image_surface_create(CAIRO_FORMAT_RGB24,
				   t->linewidth, hn);
  if (sfc == NULL) {
    return false;
  }

  cr = cairo_create(sfc);
  if (cr == NULL) {
    cairo_surface_destroy(sfc);
    return false;
  }

  cairo_set_source_surface(cr, osfc, 0, 0);
  cairo_rectangle(cr, 0, 0, t->linewidth, h);
  cairo_fill(cr);

  t->sfc = sfc;
  t->cr = cr;

  cairo_destroy(ocr);
  cairo_surface_destroy(osfc);

  return true;
}

static bool
nextline(struct textbox *t, int *x, int *y)
{
  *x = 0;
  *y += lineheight;

  if (buffergrow(t, *y + lineheight * 10)) {
    t->height = *y + lineheight;
    return true;
  } else {
    return false;
  }
}

static void 
drawglyph(struct textbox *t, int x, int y, int32_t pos,
	  struct colour *fg, struct colour *bg)
{
  uint8_t buf[1024]; /* Hopefully this is big enough */
  FT_Bitmap *map = &face->glyph->bitmap;
  cairo_surface_t *s;
  int stride, h;

  /* The buffer needs to be in a format cairo accepts */

  stride = cairo_format_stride_for_width(CAIRO_FORMAT_A8, map->width);
  for (h = 0; h < map->rows; h++) {
    memmove(buf + h * stride,
	    map->buffer + h * map->width,
	    map->width);
  }

  s = cairo_image_surface_create_for_data(buf,
					  CAIRO_FORMAT_A8,
					  map->width,
					  map->rows,
					  stride);
  if (s == NULL) {
    return;
  }

  cairo_set_source_rgb(t->cr, bg->r, bg->g, bg->b);
  cairo_rectangle(t->cr, x, y, 
		  face->glyph->advance.x >> 6, lineheight);

  cairo_fill(t->cr);
  
  cairo_set_source_rgb(t->cr, fg->r, fg->g, fg->b);

  cairo_mask_surface(t->cr, s, x + face->glyph->bitmap_left,
		     y + baseline - face->glyph->bitmap_top);

  cairo_surface_destroy(s);
}

static void
drawcursor(struct textbox *t, int x, int y)
{
  cairo_set_source_rgb(t->cr, 0, 0, 0);
  cairo_set_line_width (cr, 1.0);

  cairo_move_to(t->cr, x, y);
  cairo_line_to(t->cr, x, y + lineheight - 1);
  cairo_stroke(t->cr);

  cairo_move_to(t->cr, x - 2, y);
  cairo_line_to(t->cr, x + 2, y);
  cairo_stroke(t->cr);

  cairo_move_to(t->cr, x - 2, y + lineheight - 1);
  cairo_line_to(t->cr, x + 2, y + lineheight - 1);
  cairo_stroke(t->cr);
}

bool
textboxpredraw(struct textbox *t)
{
  int32_t code, i, a, pos;
  struct piece *p;
  int x, y, ww;

  cairo_set_source_rgb(t->cr, t->bg.r, t->bg.g, t->bg.b);
  cairo_paint(t->cr);
 
  pos = 0;
  x = 0;
  y = 0;
  
  if (buffergrow(t, lineheight * 10)) {
    t->height = lineheight;
  } else {
    return false;
  }
  
  for (p = t->pieces; p != NULL; p = p->next) {
    for (a = 0, i = 0; i < p->pl; i += a, pos += a) {
      a = utf8proc_iterate(p->s + i, p->pl - i, &code);
      if (a <= 0) {
	a = 1;
	continue;
      }
      
      if (islinebreak(code, p->s + i, p->pl - i, &a)) {
	if (pos == t->cursor) {
	  drawcursor(t, x, y);
	}

	if (!nextline(t, &x, &y)) {
	  return false;
	}
      }
      
      if (!loadglyph(code)) {
	continue;
      }

      ww = face->glyph->advance.x >> 6;

      if (x + ww >= t->linewidth) {
	if (!nextline(t, &x, &y)) {
	  return false;
	}
      }

      drawglyph(t, x, y, pos, &nfg, &t->bg);

      if (pos == t->cursor) {
	drawcursor(t, x, y);
      }

      x += ww;
    }
  }

  if (pos == t->cursor) {
    drawcursor(t, x, y);
  }

  return true;
} 
