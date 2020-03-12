/*
Copyright 2005-2020 Kendall F. Morris

This file is part of the Xanalysis software suite.

    The Xanalysis software suite is free software: you can redistribute
    it and/or modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation, either
    version 3 of the License, or (at your option) any later version.

    The suite is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with the suite.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include "ps_starbase.h"

#define DIE (fprintf (stderr, "fatal error in %s at line %d \n", __FILE__, __LINE__), pss_die(), 0)

#define MARGIN .25
#define PAPER_LLX (MARGIN * 72);
#define PAPER_LLY (MARGIN * 72)
#define PAPER_URX ((8.5 - MARGIN) * 72)
#define PAPER_URY ((11 - MARGIN) * 72)

//#define PAPER_URX ((11 - MARGIN) * 72)
//#define PAPER_URY ((8.5 - MARGIN) * 72)

static double window_llx = PAPER_LLX;
static double window_lly = PAPER_LLY;
static double window_urx = PAPER_URX;
static double window_ury = PAPER_URY;

static double vdc_xmin = 0, vdc_xmax = 0, vdc_ymin = 1, vdc_ymax = 1;
static double port_x1 = 0, port_x2 = 0, port_y1 = 1, port_y2 = 1;
static double vwin_x1 = 0, vwin_x2 = 0, vwin_y1 = 1, vwin_y2 = 1;
static double character_height = .1;
static double character_width = 0;
static int mapping_mode;
static int docalc = 1;
static int line_type = 1;
static FILE *file;
static float curr_x, curr_y;
static float old_x = NAN, old_y = NAN;
static int in_line;
static double world_over_vdc_x, world_over_vdc_y;
static double world_0_in_vdc_x, world_0_in_vdc_y;
static double vdc_0_in_world_x, vdc_0_in_world_y;
static double left_side_bearing;
static int tax = TA_NORMAL_HORIZONTAL;
static int tay = TA_NORMAL_VERTICAL;
static float up_x = 0, up_y = 1, base_x = 1, base_y = 0;
static double xscale, yscale;
static int clip, text_path, text_line_path;
static int docolor;
static float linergb[3], fillrgb[3], textrgb[3], currrgb[3], edgergb[3];
static float dashrgb[3] = {.75, .75, .75};

static int interior_style_val, edged_val;

int window_width, window_height;

void pss_die (void)
{
  if (errno)
    perror ("last system error");
  fflush (stdout);
  exit (1);
}

void
ps_color_set (int *val)
{
  docolor = *val;
}

void
ps_interior_style (int *style, int *edged)
{
  interior_style_val = *style;
  edged_val = *edged;
}

void
ps_perimeter_color (float *red, float *green, float *blue)
{
  if (!docolor) return;
  edgergb[0] = *red;
  edgergb[1] = *green;
  edgergb[2] = *blue;
}

void
ps_line_color (float *red, float *green, float *blue)
{
  if (!docolor) return;
  linergb[0] = *red;
  linergb[1] = *green;
  linergb[2] = *blue;
  dashrgb[0] = (*red   + 3) / 4;
  dashrgb[1] = (*green + 3) / 4;
  dashrgb[2] = (*blue  + 3) / 4;
}

void
ps_text_color (float *red, float *green, float *blue)
{
  if (!docolor) return;
  textrgb[0] = *red;
  textrgb[1] = *green;
  textrgb[2] = *blue;
}

void
ps_fill_color (float *red, float *green, float *blue)
{
  if (!docolor) return;
  fillrgb[0] = *red;
  fillrgb[1] = *green;
  fillrgb[2] = *blue;
}

static void
maybe_stroke (void)
{
  if (in_line) {
    fprintf (file, "stroke\n");
    old_x = curr_x;
    old_y = curr_y;
    curr_x = NAN;
    curr_y = NAN;
    in_line = 0;
  }
}

static void
setrgb (float rgb[3])
{
  if (memcmp (rgb, currrgb, sizeof currrgb) == 0)
    return;
  maybe_stroke ();
  fprintf (file, "%g %g %g setrgbcolor\n", rgb[0], rgb[1], rgb[2]);
  memcpy (currrgb, rgb, sizeof currrgb);
}

void
ps_fopen (char *filename)
{
  (file = fopen (filename, "w")) || DIE;
  fputs ("%!PS-Adobe-1.0\n"

	 //	 "%%BoundingBox: 0 0 792 612 \n"
	 "%%BoundingBox: 0 0 612 792\n"

	 "%%EndComments\n"
	 "%%Pages: 1\n"
	 "%%EndProlog\n"
	 "%%Page: 1 1\n"
	 // "0 setlinewidth\n"
	 "gsave\n"
	 //	 "<< /PageSize [792 612] /Orientation 0 >> setpagedevice\n",
	 ,

	 file) >= 0 || DIE;
}

void
ps_fclose (void)
{
  fputs ("showpage\n"
	 "grestore\n"
	 "%%Trailer\n", file) >= 0 || DIE;

  fclose (file) == 0 || DIE;
  file = 0;
}

static void
maxpect (double aspect, double *llxp, double *llyp, double *urxp, double *uryp)
{
  double height = *uryp - *llyp;
  double width = *urxp - *llxp;
  double d;

  if (aspect > height / width) {
    d = (height - width / aspect) / 2;
    *llyp += d;
    *uryp -= d;
  }
  else {
    d = (width - height * aspect) / 2;
    *llxp += d;
    *urxp -= d;
  }
}

static void
calc (void)
{
  double llx = window_llx;
  double lly = window_lly;
  double urx = window_urx;
  double ury = window_ury;
  double vdc_width= vdc_xmax - vdc_xmin;
  double vdc_height = vdc_ymax - vdc_ymin;
  double vwin_width= vwin_x2 - vwin_x1;
  double vwin_height = vwin_y2 - vwin_y1;
  double port_width = port_x2 - port_x1;
  double port_height = port_y2 - port_y1;
  double ps_width, ps_height;
  double tx, ty;
  world_over_vdc_x = vwin_width / port_width;
  world_over_vdc_y = vwin_height / port_height;
  world_0_in_vdc_x = port_x1 - vwin_x1 / world_over_vdc_x;
  world_0_in_vdc_y = port_y1 - vwin_y1 / world_over_vdc_y;
  vdc_0_in_world_x = vwin_x1 - port_x1 * world_over_vdc_x;
  vdc_0_in_world_y = vwin_y1 - port_y1 * world_over_vdc_y;
  
  if (mapping_mode == 0) {
    maxpect (vdc_width / vdc_height, &llx, &lly, &urx, &ury);
  }
  ps_width = urx - llx;
  ps_height = ury - lly;
  xscale = ps_width / vdc_width;
  yscale = ps_height / vdc_height;

  llx += (port_x1 - vdc_xmin) * xscale;
  urx -= (vdc_xmax - port_x2) * xscale;
  lly += (port_y1 - vdc_ymin) * yscale;
  ury -= (vdc_ymax - port_y2) * yscale;

  ps_width = urx - llx;
  ps_height = ury - lly;
  xscale = ps_width / vwin_width;
  yscale = ps_height / vwin_height;
  tx = llx - vwin_x1 * xscale;
  ty = lly - vwin_y1 * yscale;
  maybe_stroke ();
  fprintf (file, "grestore gsave 612 0 translate 90 rotate %f %f translate %f %f scale\n", tx, ty, xscale, yscale) > 0 || DIE;
  if (character_width == 0)
    character_width = character_height * 21/40;
  fprintf (file, "/Helvetica findfont [%f 0 0 %f 0 0] makefont setfont\n",
	   character_width * 1000/667 * world_over_vdc_x,
	   character_height * 500/729 * world_over_vdc_y
	   );
  left_side_bearing = character_width*75/667*world_over_vdc_x;
  if (line_type == 0)
    fprintf (file, "[] 0 setdash\n") > 0 || DIE;
  else {
//    double dashlen = vdc_width / (32 * 24) * world_over_vdc_x;
//    fprintf (file, "[%f %f] 0 setdash\n", dashlen, dashlen) > 0 || DIE;
  }
  docalc = 0;
}
  
void
ps_geometry (float *llx, float *lly, float *urx, float *ury)
{
  window_llx = *llx;
  window_lly = *lly;
  window_urx = *urx;
  window_ury = *ury;
  docalc = 1;
}

void
ps_vdc_extent (float *xminp, float *yminp, float *zminp, float *xmaxp, float *ymaxp, float *zmaxp)
{
  vdc_xmin = *xminp;
  vdc_ymin = *yminp;
  vdc_xmax = *xmaxp;
  vdc_ymax = *ymaxp;
  docalc = 1;
}

void
ps_mapping_mode (int *modep)
{
  mapping_mode = *modep;
  docalc = 1;
}

void
ps_view_port (float *x1p, float *y1p, float *x2p, float *y2p)
{
  port_x1 = *x1p;
  port_y1 = *y1p;
  port_x2 = *x2p;
  port_y2 = *y2p;
  docalc = 1;
}

void
ps_view_window (float *x1p, float *y1p, float *x2p, float *y2p)
{
  vwin_x1 = *x1p;
  vwin_y1 = *y1p;
  vwin_x2 = *x2p;
  vwin_y2 = *y2p;
  docalc = 1;
}

void
ps_character_height (float *heightp)
{
  character_height = *heightp;
  docalc = 1;
}

void
ps_character_width (float *widthp)
{
  character_width = *widthp;
  docalc = 1;
}

void
ps_line_type (int *stylep)
{
  //  double vdc_width= vdc_xmax - vdc_xmin;
  line_type = *stylep;
  if (line_type == 0) {
    maybe_stroke ();
    fprintf (file, "[] 0 setdash\n") > 0 || DIE;
  }
  else {
//    double dashlen = vdc_width / (32 * 24) * world_over_vdc_x;
//    maybe_stroke ();
//    fprintf (file, "[%f %f] 0 setdash\n", dashlen, dashlen) > 0 || DIE;
  }
}

void
ps_draw2d (float *xp, float *yp)
{
  if (docalc) calc();
  
  setrgb (line_type == 0 ? linergb : dashrgb);
  if (isnan (curr_x) && !isnan (old_x)) {
    fprintf (file, "%f %f moveto\n", old_x, old_y);
    old_x = old_y = NAN;
  }
  fprintf (file, "%f %f lineto\n", *xp, *yp);
  in_line = 1;
  curr_x = *xp;
  curr_y = *yp;
}

void
ps_rectangle (float *x1p, float *y1p, float *x2p, float *y2p)
{
  float width = *x2p - *x1p;
  float height = *y2p - *y1p;

  if (docalc) calc();

  if (interior_style_val && width > 0 && height > 0) {
    setrgb (fillrgb);
    maybe_stroke ();
    fprintf (file, "%f %f %f %f rectfill\n", *x1p, *y1p, width, height);
  }

  if (edged_val) {
    setrgb (edgergb);
    maybe_stroke ();
    fprintf (file, "%f %f %f %f rectstroke\n", *x1p, *y1p, width, height);
  }
}

void
ps_move2d (float *xp, float *yp)
{
  if (docalc) calc();
  if (*xp != curr_x || *yp != curr_y) {
    if (in_line) {
      fprintf (file, "stroke\n");
      in_line = 0;
    }
    fprintf (file, "%f %f moveto\n", *xp, *yp);
    curr_x = *xp;
    curr_y = *yp;
  }
}

void
ps_text2d (float *xp, float *yp, char *string, int *xformp, int slen)
{
  double x, y;
  if (docalc) calc();
  if (*xformp == 0) {
    x = vdc_0_in_world_x + *xp * world_over_vdc_x;
    y = vdc_0_in_world_y + *yp * world_over_vdc_y;
  }
  else {
    x = *xp;
    y = *yp;
  }
  setrgb (textrgb);
  x -= left_side_bearing;
  if (tay == TA_TOP)
    y -= character_height * 500/729 * world_over_vdc_y;
  int rot = up_x != 0 || up_y != 1 || base_x != 1 || base_y != 0;

  if (rot) {
    maybe_stroke ();
    fprintf (file, "gsave %f %f translate 90 rotate  ", x, y);
    fprintf (file, "/Helvetica findfont [%f 0 0 %f 0 0] makefont setfont\n",
             character_width * 1000/667 * world_over_vdc_y,
             character_height * 500/729 * world_over_vdc_x
             );
    x = y = 0;
  }
  maybe_stroke ();
  fprintf (file, "%f", x);
  char *s = malloc (slen + 1);
  memcpy (s, string, slen);
  s[slen] = 0;
  while (slen > 0 && (s[slen - 1] == ' ' || s[slen - 1] == 0))
    s[--slen] = 0;
  if (tax == TA_RIGHT) fprintf (file, " (%s) stringwidth pop sub", s);
  fprintf (file, " %f moveto (%s) show\n", y, s);
  free (s);
  if (rot) fprintf (file, " grestore\n");
}

void
ps_append_text (char *string, int *xformp)
{
  setrgb (textrgb);
  maybe_stroke ();
  fprintf (file, "(%s) show\n", string);
}

void
ps_direct (char *string)
{
  maybe_stroke ();
  fprintf (file, "%s", string);
}

void
ps_text_alignment (int *tax_in, int *tay_in, int *h, int *v)
{
  tax = *tax_in;
  tay = *tay_in;
}

void
ps_text_orientation2d (float *up_xp, float *up_yp, float *base_xp, float *base_yp)
{
  up_x = *up_xp;
  up_y = *up_yp;
  base_x = *base_xp;
  base_y = *base_yp;
}

void
ps_clip_indicator (int *clip_level)
{
  clip = *clip_level;
}

void
ps_text_path (int *path)
{
  text_path = *path;
}

void
ps_text_line_path (int *path)
{
  text_line_path = *path;
}

