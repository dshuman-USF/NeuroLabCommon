#define _GNU_SOURCE
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <limits.h>
#include <sys/time.h>
#include "ps_starbase.h"
#include "sbparam.h"

#define DIE (fprintf (stderr, "fatal error in %s at line %d \n", __FILE__, __LINE__), die(), 0)
#define TMALLOC(buf, n) (buf = malloc ((n) * sizeof *(buf))) || DIE
#define TREALLOC(buf, n) (buf = realloc (buf, (n) * sizeof *(buf))) || DIE
#define INT_HOLLOW 0
#define INT_SOLID 1

static Display *display;
static int screen;
enum {BLACK, WHITE, RED, BLUE, GREEN, COLOR_COUNT};
static int colors[COLOR_COUNT];
static Atom wm_protocols, wm_delete_window;

static void
die (void)
{
  if (errno)
    perror ("last system error");
  fflush (stdout);
  exit (1);
}

typedef struct {
  Window win;
  Pixmap pix;
  int mapping_distort, shade_mode_val, shading_val, font_index;
  float text_r, text_g, text_b;
  float vdc_xmin, vdc_ymin, vdc_zmin, vdc_xmax, vdc_ymax, vdc_zmax;
  unsigned int width, height;
  float char_height, char_width, line_r, line_g, line_b, char_expansion_factor;
  int clear_mode, line_style;
  int port_done;
  float port_x1, port_y1, port_x2, port_y2;
  float window_x1, window_y1, window_x2, window_y2;
  float pen_x, pen_y;
  GC line_gc, text_gc, fill_gc, edge_gc, back_gc;
  int perimeter_color_index_val;
  int fill_color_index_val;
  int interior_style_val;
  int edged_val;
  int char_ht, char_wd;
  float up_x, up_y, base_x, base_y;
  int minx, miny, maxx, maxy;
  int tax, tay;
  int clip, text_path, text_line_path;
  XFontStruct *fontinfo;
} WinData;

static void
do_port (WinData *w)
{
  if (!w->port_done) {
    w->port_x1 = w->vdc_xmin;
    w->port_x2 = w->vdc_xmax;
    w->port_y1 = w->vdc_ymin;
    w->port_y2 = w->vdc_ymax;
    w->port_done = 1;
  }
}

static void
phys_to_vdc (WinData *w, int x, int y, float *vdc_x, float *vdc_y)
{
  *vdc_x = w->vdc_xmin + (double)x / w->width * (w->vdc_xmax - w->vdc_xmin);
  *vdc_y = w->vdc_ymin + (w->height - (double)y) / w->height * (w->vdc_ymax - w->vdc_ymin);
}

static void
vdc_to_phys (WinData *w, float x, float y, int *phys_x, int *phys_y)
{
  *phys_x = (double)w->width / (w->vdc_xmax - w->vdc_xmin) * (x - w->vdc_xmin);
  *phys_y = (double)w->height / (w->vdc_ymax - w->vdc_ymin) * (w->vdc_ymax - y);
}

void
sb_vdc_to_wc (long *fildes, float *vdcx, float *vdcy, float *vdcz, float *wcx, float *wcy, float *wcz)
{
  WinData *w = (WinData *) *fildes;

  do_port (w);
  *wcx = w->window_x1 + (*vdcx - w->port_x1) / (w->port_x2 - w->port_x1) * (w->window_x2 - w->window_x1);
  *wcy = w->window_y1 + (*vdcy - w->port_y1) / (w->port_y2 - w->port_y1) * (w->window_y2 - w->window_y1);
}

void
sb_wc_to_vdc (long *fildes, float *x, float *y, float *z, float *vdcx, float *vdcy, float *vdcz)
{
  WinData *w = (WinData *) *fildes;
  do_port (w);
  *vdcx = w->port_x1 + (double)(w->port_x2 - w->port_x1) / (w->window_x2 - w->window_x1) * (*x - w->window_x1);
  *vdcy = w->port_y1 + (double)(w->port_y2 - w->port_y1) / (w->window_y2 - w->window_y1) * (*y - w->window_y1);
  /* z dimension not implemented  */
}

void
sb_world_to_phys (WinData *w, float *x, float *y, int *phys_x, int *phys_y)
{
  float vdc_x, vdc_y, vdc_z, z = 0;
  long *fildes = (long *)&w;
  sb_wc_to_vdc (fildes, x, y, &z, &vdc_x, &vdc_y, &vdc_z);
  vdc_to_phys (w, vdc_x, vdc_y, phys_x, phys_y);

  if (0) {
    printf ("window %f %f to %f %f\n", w->window_x1, w->window_y1, w->window_x2, w->window_y2);
    printf ("port %f %f to %f %f\n", w->port_x1, w->port_y1, w->port_x2, w->port_y2);
    printf ("vdc %f %f to %f %f\n", w->vdc_xmin, w->vdc_ymin, w->vdc_xmax, w->vdc_ymax);
  }
}


void
sb_character_height (long *fildes, float *height)
{
  WinData *w = (WinData *) *fildes;
  w->char_height = *height;
}

void
intcharacter_height (long *fildes, int *height)
{
  WinData *w = (WinData *) *fildes;
  w->char_height = *height;
}

void
sb_character_width (long *fildes, float *width)
{
  WinData *w = (WinData *) *fildes;
  w->char_width = *width;
  w->char_expansion_factor = 0;
}

void
sb_character_expansion_factor (long *fildes, float *factor)
{
  WinData *w = (WinData *) *fildes;
  w->char_width = 0;
  w->char_expansion_factor = *factor;
}

void
sb_clear_control (long *fildes, int *mode)
{
  WinData *w = (WinData *) *fildes;
  w->clear_mode = *mode;
}

void
sb_clear_view_surface (long *fildes)
{
  WinData *w = (WinData *) *fildes;
  if (w->clear_mode == 17) {
    int px1, px2, py1, py2, wd, ht, x, y, x2, y2;
    vdc_to_phys (w, w->port_x1, w->port_y1, &px1, &py1);
    vdc_to_phys (w, w->port_x2, w->port_y2, &px2, &py2);

    if (px1 < px2) x = px1, x2 = px2; 
    else           x = px2, x2 = px1;

    if (py1 < py2) y = py1, y2 = py2; 
    else           y = py2, y2 = py1;

    wd = x2 - x;
    ht = y2 - y;

    if (x < w->minx) w->minx = x;
    if (y < w->miny) w->miny = y;
    if (x2 > w->maxx) w->maxx = x2;
    if (y2 > w->maxy) w->maxy = y2;
    XFillRectangle (display, w->pix, w->back_gc, x, y, wd, ht);
    return;
  }
  XFillRectangle (display, w->pix, w->back_gc, 0, 0, w->width, w->height);
  w->minx = 0;
  w->maxx = w->width - 1;
  w->miny = 0;
  w->maxy = w->height - 1;
}

void
sb_draw2d (long *fildes, float *x, float *y)
{
  WinData *w = (WinData *) *fildes;
  int x1, x2, y1, y2;

  sb_world_to_phys (w, &w->pen_x, &w->pen_y, &x1, &y1);
  sb_world_to_phys (w, x, y, &x2, &y2);

  if (x1 < w->minx) w->minx = x1;
  if (y1 < w->miny) w->miny = y1;
  if (x2 > w->maxx) w->maxx = x2;
  if (y2 > w->maxy) w->maxy = y2;

  //printf ("draw2d %f %f to %f %f: %d %d to %d %d\n", w->pen_x, w->pen_y, *x, *y, x1, y1, x2, y2);

  XDrawLine (display, w->pix, w->line_gc, x1, y1, x2, y2);
  w->pen_x = *x;
  w->pen_y = *y;
}

void
sb_line_color (long *fildes, float *red, float *green, float *blue)
{
  WinData *w = (WinData *) *fildes;
  static XGCValues values;
  XColor c;
  
  c.red   = *red   * 65535;
  c.green = *green * 65535;
  c.blue  = *blue  * 65535;
  XAllocColor (display, DefaultColormap (display, screen), &c) || DIE;
  values.foreground = c.pixel;
  XChangeGC (display, w->line_gc, GCForeground, &values);

  w->line_r = *red;
  w->line_g = *green;
  w->line_b = *blue;
}

void
sb_line_width (long *fildes, float *width, int *mode)
{
  WinData *w = (WinData *) *fildes;
  static XGCValues values;

  if (*mode != VDC_UNITS)
    fprintf (stderr, "line_width: MC_UNITS not implemented\n");

  values.line_width = nearbyint ((double)w->width / (w->vdc_xmax - w->vdc_xmin) * *width);
  XChangeGC (display, w->line_gc, GCLineWidth, &values);
}

void
sb_line_type (long *fildes, int *style)
{
  WinData *w = (WinData *) *fildes;
  static XGCValues values;
  
  values.line_style = *style == 0 ? LineSolid : LineOnOffDash;
  XChangeGC (display, w->line_gc, GCLineStyle, &values);
  w->line_style = *style;
}

void
sb_make_picture_current (long *fildes)
{
  WinData *w = (WinData *) *fildes;
  int width, height;

  width = w->maxx - w->minx + 1;
  height= w->maxy - w->miny + 1;
  if (width >= 1 && height >= 1) 
    XCopyArea (display, w->pix, w->win, w->line_gc, w->minx, w->miny, width, height, w->minx, w->miny);
  w->miny = w->minx = INT_MAX;
  w->maxy = w->maxx = -1;

  XFlush (display);
}

void
sb_mapping_mode (long *fildes, int *distort)
{
  WinData *w = (WinData *) *fildes;
  w->mapping_distort = *distort;
}

void
sb_move2d (long *fildes, float *x, float *y)
{
  WinData *w = (WinData *) *fildes;
  w->pen_x = *x;
  w->pen_y = *y;
}

void
sb_shade_mode (long *fildes, int *mode, int *shading)
{
  WinData *w = (WinData *) *fildes;
  w->shade_mode_val = *mode;
  w->shading_val = *shading;
}

static void
XDrawRotatedString(WinData *w, Drawable d, GC gc, int x, int y,
		   char *string, int length, XFontStruct *font_struct,
		   float base_x, float base_y, int pixel_size)
{
  double offset = 0;

  XCharStruct *sp = font_struct->per_char + ('_' - font_struct->min_char_or_byte2);
  int spwidth = (sp->ascent + sp->descent) / 2;

  while (length-- > 0)
    {
      unsigned char cval;
      int x0, y0;

      cval = string[0];
      x0 = x + (int)floor (offset * base_x + .5);
      y0 = y + (int)floor (offset * -base_y + .5);
      XDrawString (display, d, gc, x0, y0, string, 1);
      if (cval >= font_struct->min_char_or_byte2 && cval <= font_struct->max_char_or_byte2) {
	XCharStruct *cs = font_struct->per_char + (cval - font_struct->min_char_or_byte2);
	int x1 = x0 + cs->lbearing;
	int x2 = x0 + cs->rbearing - 1;
	int y1 = y0 - cs->ascent;
	int y2 = y0 + cs->descent - 1;
        int width = cval == ' ' ? spwidth : cs->ascent - cs->descent;

	if (x1 < w->minx) w->minx = x1;
	if (y1 < w->miny) w->miny = y1;
	if (x2 > w->maxx) w->maxx = x2;
	if (y2 > w->maxy) w->maxy = y2;

	offset += width;
      }
      else printf("char out of range: %d is not between %d and %d inclusive\n",
		  cval, font_struct->min_char_or_byte2, font_struct->max_char_or_byte2);
      string++;
    }
}

static void
text (long fildes, float x_in, float y_in, char *string_in, int xform, int more, int xy, int slen)
{
  WinData *w = (WinData *) fildes;
  int ht, wd, phys_x, phys_y;
  char *fontname_fmt = "-adobe-helvetica-medium-r-normal--%d-*-*-*-p-%s-iso8859-1";
  static float x, y;
  static int xy_ok;
  static char *string;
  static int stringlen;
  if (w->font_index == 1)
    fontname_fmt = "-adobe-courier-bold-r-normal--%d-*-*-*-m-%s-iso8859-1";
  if (xy) {
    xy_ok = 1;
    x = x_in;
    y = y_in;
  }
  string_in || DIE;

  if (string == 0) {
    TMALLOC (string, slen + 1);
    string[0] = 0;
  }
  else TREALLOC (string, stringlen + slen + 1);
  memcpy (string + stringlen, string_in, slen);
  string[stringlen += slen] = 0;

  while (stringlen > 0 && (string[stringlen - 1] == ' ' || string[stringlen - 1] == 0))
    string[--stringlen] = 0;
  
  if (more)
    return;

  xy_ok || DIE;

  ht = floor (.8 * w->char_height * w->height / fabs (w->vdc_ymax - w->vdc_ymin) + .5);
  
  if (0)
    printf ("floor (.8 * %g * %u / (%g - %g) + .5) = %d\n",
            w->char_height, w->height, w->vdc_ymax, w->vdc_ymin, ht);
  
  wd = 0;
  if (w->char_width)
    wd = floor (.8 * w->char_width * w->width / (w->vdc_xmax - w->vdc_xmin) * 10 + .5);
  else if (w->char_expansion_factor)
    wd = nearbyint (ht * .56 * w->char_expansion_factor * 10);

//  if      (ht > 25) {ht = 25; wd = 130;}
//  else if (ht < 20) {ht = 17; wd =  88;}
//  else              {ht = 18; wd = 98;}
  if (ht < 2) {ht = 2; wd = 0; }
  char wd_c[12];
  if (wd)
    sprintf (wd_c, "%d", wd);
  else
    sprintf (wd_c, "*");
  
  if (xform == 0)
    vdc_to_phys (w, x, y, &phys_x, &phys_y);
  else
    sb_world_to_phys (w, &x, &y, &phys_x, &phys_y);

  if (w->up_x != 0 || w->up_y != 1 || w->base_x != 1 || w->base_y != 0) {
    XFontStruct *fontinfo;
    int len, n;
    char matrix[128];
    char name[256];
    char *fmt = "-adobe-helvetica-medium-r-normal--%s-*-100-100-p-%d-iso8859-1";
    if (w->font_index == 1)
      fmt = "-adobe-courier-medium-r-normal--%s-*-100-100-p-%d-iso8859-1";

    sprintf (matrix, "[%f %f %f %f]", ht * w->base_x, ht * w->base_y, ht * w->up_x, ht * w->up_y);
    len = strlen (matrix);
    for (n = 0; n < len; n++)
      if (matrix[n] == '-')
        matrix[n] = '~';
    sprintf (name, fmt, matrix, wd);

    fontinfo = XLoadQueryFont (display, name);
    if (fontinfo == NULL)
      fprintf (stderr, "%s line %d: Cannot open font \"%s\"\n", __FILE__, __LINE__, name);
    else {
      XCharStruct overall;
      int dir, ascent, descent;
      XFontStruct *horiz_fontinfo;
      char *fontname = 0;
      TMALLOC (fontname, strlen (fontname_fmt) + 23);
      sprintf (fontname, fontname_fmt, ht, wd_c);
      horiz_fontinfo = XLoadQueryFont (display, fontname);
      free (fontname);
      XTextExtents (horiz_fontinfo, string, strlen (string), &dir, &ascent, &descent, &overall);
      fontinfo = XLoadQueryFont (display, name);
      if (w->tax == TA_RIGHT) phys_y += overall.rbearing;
      if (w->tay == TA_TOP  ) phys_x += overall.ascent;
      XSetFont (display, w->text_gc, fontinfo->fid);
      XDrawRotatedString (w, w->pix, w->text_gc, phys_x, phys_y - 2, string, strlen (string),
			  fontinfo, w->base_x, w->base_y, ht);
      XFlush (display);
      w->char_ht = 0;
      w->char_wd = 0;
    }
    string[0] = 0;
    stringlen = 0;
    return;
  }

  if (ht != w->char_ht || wd != w->char_wd) {
    char *fontname = 0;
    TMALLOC (fontname, strlen (fontname_fmt) + 23);
    sprintf (fontname, fontname_fmt, ht, wd_c);
    if (w->fontinfo)
      XFreeFontInfo (0, w->fontinfo, 1);

    w->fontinfo = XLoadQueryFont (display, fontname);

    Atom nameAtom;
    XGetFontProperty(w->fontinfo, XA_FONT, &nameAtom);
    char *sfontname = XGetAtomName(display, nameAtom);
    XFree (sfontname);
    

    if (!w->fontinfo) {
      fprintf (stderr, "Can't load %dx%d font %s\n", ht, wd, fontname);
      exit (1);
    }
    if (getenv ("SHOWFONTS")) {
#include <X11/Xatom.h>
      int i;
      XFontStruct *f = w->fontinfo;
      XFontProp *xfp;
      char *name = "";
      for (i = 0, xfp = f->properties; i < f->n_properties; i++, xfp++) {
        if (xfp->name == XA_FONT) {
          name = XGetAtomName(display, (Atom) xfp->card32);
          break;
        }
      }
      printf ("%s line %d:\nasked for font %s,\nsetting font %s\n", __FILE__, __LINE__, fontname, name);
      XFree (name);
    }

    free (fontname);
    XSetFont (display, w->text_gc, w->fontinfo->fid);
    w->char_ht = ht;
    w->char_wd = wd;
  }
  {
    int len = strlen (string);
    int dir, ascent, descent;
    int x1, x2, y1, y2;
    XCharStruct overall;
    XTextExtents (w->fontinfo, string, len, &dir, &ascent, &descent, &overall);

    if (w->tax == TA_RIGHT) phys_x -= overall.rbearing;
    if (w->tay == TA_TOP  ) phys_y += ascent + 2;

    x1 = phys_x + overall.lbearing;
    x2 = phys_x + overall.rbearing + 1;
    y1 = phys_y - 2  - overall.ascent;
    y2 = phys_y - 2  + overall.descent - 1;

    if (x1 < w->minx) w->minx = x1;
    if (y1 < w->miny) w->miny = y1;
    if (x2 > w->maxx) w->maxx = x2;
    if (y2 > w->maxy) w->maxy = y2;

    XDrawString (display, w->pix, w->text_gc, phys_x, phys_y - 2, string, len);
  }
  string[0] = 0;
  stringlen = 0;
}

void
text_precision (long *fildes, int *precision)
{
}

void
sb_text2d (long *fildes, float *x_in, float *y_in, char *string_in, int *xform, int *more, int slen)
{
  text (*fildes, *x_in, *y_in, string_in, *xform, *more, 1, slen);
}

void
inttext2d (long *fildes, int *x_in, int *y_in, char *string_in, int *xform, int slen)
{
  text (*fildes, *x_in, *y_in, string_in, *xform, 0, 1, slen);
}

void
sb_append_text (long *fildes, char *string_in, int *xform, int *more, int slen)
{
  text (*fildes, 0, 0, string_in, *xform, *more, 0, slen);
}

void
sb_text_alignment (long *fildes, int *tax, int *tay, int *h, int *v)
{
  WinData *w = (WinData *) *fildes;
  w->tax = *tax;
  w->tay = *tay;
}

void
sb_text_color (long *fildes, float *red, float *green, float *blue)
{
  WinData *w = (WinData *) *fildes;
  static XGCValues values;
  XColor c;
  
  c.red   = *red   * 65535;
  c.green = *green * 65535;
  c.blue  = *blue  * 65535;
  XAllocColor (display, DefaultColormap (display, screen), &c) || DIE;
  values.foreground = c.pixel;
  XChangeGC (display, w->text_gc, GCForeground, &values);

  w->text_r = *red;
  w->text_g = *green;
  w->text_b = *blue;
}

void
sb_text_font_index (long *fildes, int *index)
{
  WinData *w = (WinData *) *fildes;
  w->font_index = *index;
}

void
sb_vdc_extent (long *fildes, float *xmin, float *ymin, float *zmin, float *xmax, float *ymax, float *zmax)
{
  WinData *w = (WinData *) *fildes;

  //  printf ("vdc extent %f %f %f, %f %f %f\n", *xmin, *ymin, *zmin, *xmax, *ymax, *zmax);
  
  w->vdc_xmin = *xmin;
  w->vdc_ymin = *ymin;
  w->vdc_zmin = *zmin;
  w->vdc_xmax = *xmax;
  w->vdc_ymax = *ymax;
  w->vdc_zmax = *zmax;
}

void
intvdc_extent (long *fildes, int *xmin, int *ymin, int *xmax, int *ymax)
{
  float x0 = *xmin;
  float y0 = *ymin;
  float z0 = 0;
  float x1 = *xmax;
  float y1 = *ymax;
  float z1 = 0;
  sb_vdc_extent (fildes, &x0, &y0, &z0, &x1, &y1, &z1);
}

void
sb_view_port (long *fildes, float *x1, float *y1, float *x2, float *y2)
{
  WinData *w = (WinData *) *fildes;
  //  printf ("view port %f %f %f %f\n", *x1, *y1, *x2, *y2);
  w->port_x1 = *x1;
  w->port_y1 = *y1;
  w->port_x2 = *x2;
  w->port_y2 = *y2;
  w->port_done = 1;
}

void
intview_port (long *fildes, int *x1, int *y1, int *x2, int *y2)
{
  WinData *w = (WinData *) *fildes;
  //  printf ("view port %f %f %f %f\n", *x1, *y1, *x2, *y2);
  w->port_x1 = *x1;
  w->port_y1 = *y1;
  w->port_x2 = *x2;
  w->port_y2 = *y2;
  w->port_done = 1;
}

void
sb_view_window (long *fildes, float *x1, float *y1, float *x2, float *y2)
{
  WinData *w = (WinData *) *fildes;
  //  printf ("view window %f %f %f %f\n", *x1, *y1, *x2, *y2);
  w->window_x1 = *x1;
  w->window_y1 = *y1;
  w->window_x2 = *x2;
  w->window_y2 = *y2;
}

void
intview_window (long *fildes, int *x1, int *y1, int *x2, int *y2)
{
  WinData *w = (WinData *) *fildes;
  //  printf ("view window %f %f %f %f\n", *x1, *y1, *x2, *y2);
  w->window_x1 = *x1;
  w->window_y1 = *y1;
  w->window_x2 = *x2;
  w->window_y2 = *y2;
}

void
sb_fill_color_index (long *fildes, int *index)
{
  WinData *w = (WinData *) *fildes;
  static XGCValues values;

  w->fill_color_index_val = *index;
  *index < COLOR_COUNT || DIE;
  values.foreground = colors[*index];
  XChangeGC (display, w->fill_gc, GCForeground, &values);
}

void
sb_fill_color (long *fildes, float *red, float *green, float *blue)
{
  WinData *w = (WinData *) *fildes;
  static XGCValues values;
  XColor c;
  
  c.red   = *red   * 65535;
  c.green = *green * 65535;
  c.blue  = *blue  * 65535;
  XAllocColor (display, DefaultColormap (display, screen), &c) || DIE;
  values.foreground = c.pixel;
  XChangeGC (display, w->fill_gc, GCForeground, &values);
}

void
sb_perimeter_color_index (long *fildes, int *index)
{
  WinData *w = (WinData *) *fildes;
  static XGCValues values;

  w->perimeter_color_index_val = *index;
  
  *index < COLOR_COUNT || DIE;
  values.foreground = colors[*index];
  XChangeGC (display, w->edge_gc, GCForeground, &values);
}

void
sb_perimeter_color (long *fildes, float *red, float *green, float *blue)
{
  WinData *w = (WinData *) *fildes;
  static XGCValues values;
  XColor c;
  
  c.red   = *red   * 65535;
  c.green = *green * 65535;
  c.blue  = *blue  * 65535;
  XAllocColor (display, DefaultColormap (display, screen), &c) || DIE;
  values.foreground = c.pixel;
  XChangeGC (display, w->edge_gc, GCForeground, &values);
}

void
sb_interior_style (long *fildes, int *style, int *edged)
{
  WinData *w = (WinData *) *fildes;
  w->interior_style_val = *style;
  w->edged_val = *edged;
}

void
sb_rectangle (long *fildes, float *x1, float *y1, float *x2p, float *y2p)
{
  WinData *w = (WinData *) *fildes;
  int px1, px2, py1, py2, wd, ht, x, y;
  float x2 = *x2p, y2 = *y2p;

  sb_world_to_phys (w, x1, y1, &px1, &py1);
  sb_world_to_phys (w, &x2, &y2, &px2, &py2);

  if (px1 < px2) x = px1, x2 = px2; 
  else           x = px2, x2 = px1;

  if (py1 < py2) y = py1, y2 = py2; 
  else           y = py2, y2 = py1;

  wd = x2 - x;
  ht = y2 - y;

  if (x < w->minx) w->minx = x;
  if (y < w->miny) w->miny = y;
  if (x2 > w->maxx) w->maxx = x2;
  if (y2 > w->maxy) w->maxy = y2;

  //  printf ("rectangle at %d %d, %d wide, %d high, style %d\n", x, y, wd, ht, w->interior_style_val);
  if (w->edged_val) 
    XDrawRectangle (display, w->pix, w->edge_gc, x, y, wd, ht);
  if (w->interior_style_val == INT_SOLID && wd > 0 && ht > 0)
    XFillRectangle (display, w->pix, w->fill_gc, x+1, y+1, wd, ht);
}

void
intpolygon2d (long *fildes, int *clist, int *numverts, int *flags)
{
  WinData *w = (WinData *) *fildes;
  XPoint p[*numverts];
  for (int i = 0; i < *numverts; i++) {
    float x = clist[2 * i];
    float y = clist[2 * i + 1];
    int px, py;
    sb_world_to_phys (w, &x, &y, &px, &py);
    p[i].x = px;
    p[i].y = py;
  }
  if (w->edged_val) {
    for (int i = 1; i < *numverts; i++)
      XDrawLine (display, w->pix, w->line_gc, p[i - 1].x, p[i - 1].y, p[i].x, p[i].y);
    XDrawLine (display, w->pix, w->line_gc, p[*numverts - 1].x, p[*numverts - 1].y, p[0].x, p[0].y);
  }
  if (w->interior_style_val == INT_SOLID)
    XFillPolygon (display, w->pix, w->fill_gc, p, *numverts, Complex, CoordModeOrigin);
}

void
sb_ellipse (long *fildes, float *x_radius, float *y_radius, float *x_center, float *y_center, float *rotation)
{
  WinData *w = (WinData *) *fildes;

  int px_center, py_center;
  sb_world_to_phys (w, x_center, y_center, &px_center, &py_center);
  
  float x_edge = *x_center + *x_radius;
  float y_edge = *y_center + *y_radius;
  int px_edge, py_edge;
  sb_world_to_phys (w, &x_edge, &y_edge, &px_edge, &py_edge);
  float px_radius = fabs (px_edge - px_center);
  float py_radius = fabs (py_edge - py_center);
  int ulx = nearbyint (px_center - px_radius);
  int uly = nearbyint (py_center - py_radius);
  unsigned int width  = nearbyint (2 * px_radius);
  unsigned int height = nearbyint (2 * py_radius);
  int lrx = ulx + width;
  int lry = uly + height;

  if (ulx < w->minx) w->minx = ulx;
  if (uly < w->miny) w->miny = uly;
  if (lrx > w->maxx) w->maxx = lrx;
  if (lry > w->maxy) w->maxy = lry;

  if (w->edged_val) 
    XDrawArc (display, w->pix, w->edge_gc, ulx, uly, width, height, 0, 360*64);
  if (w->interior_style_val == INT_SOLID && width > 0 && height > 0)
    XFillArc (display, w->pix, w->edge_gc, ulx, uly, width, height, 0, 360*64);
}

void
sb_sample_locator (long *fildes, int *ordinal, int *valid, float *x, float *y, float *z)
{
  WinData *w = (WinData *) *fildes;
  Window root, child;
  int root_x, root_y, win_x, win_y;
  unsigned int mask;

  XQueryPointer (display, w->win, &root, &child, &root_x, &root_y, &win_x, &win_y, &mask);
  *z = 0;
  phys_to_vdc (w, win_x, win_y, x, y);
}

void
sb_request_locator (long *fildes, int *ordinal, float *ftimeout, int *valid, float *x, float *y, float *z)
{
  WinData *w = (WinData *) *fildes;
  int nf, nfds, cn = ConnectionNumber (display);
  struct timeval timeout, *timer;
  fd_set rset, tset;
  XEvent e;

  FD_ZERO (&rset);
  FD_SET (cn, &rset);
  nfds = cn + 1;

  timeout.tv_sec  = floor (*ftimeout);
  timeout.tv_usec = (*ftimeout - timeout.tv_sec) * 1000000;
  timer = &timeout;

  while (1) {
    tset = rset;
    int xpending = XPending (display);
    nf = 0;
    if (!xpending) {
      nf = select (nfds, &tset, (fd_set *)0, (fd_set *)0, timer);
      xpending = nf > 0 && FD_ISSET(cn, &tset);
    }
    if (xpending) {
      //      XWindowEvent (display, w->win, ButtonPressMask|ExposureMask|KeyPressMask, &e);
      XNextEvent (display, &e);
      if (e.type == ClientMessage) {
        XClientMessageEvent *m = (XClientMessageEvent *)&e;
        if (m->message_type == wm_protocols && m->data.l[0] == wm_delete_window) {
          phys_to_vdc (w, w->width, 0, x, y);
          *valid = m->window == w->win ? 3 : 4;
          return;
        }
        else printf ("unrecognized ClientMessage\n");
        continue;
      }
      if (e.xany.window != w->win)
        continue;
      if (e.xbutton.type == ButtonPress) {
	*z = 0;
	phys_to_vdc (w, e.xbutton.x, e.xbutton.y, x, y);
	break;
      }
      else if (e.type == KeyPress) {
        if (*ordinal == 1)
          continue;
	KeySym ks;
	int len = 10;
	char buf[len];
	XLookupString (&e.xkey, buf, len, &ks, 0);
	*valid = 2;
	*ordinal = buf[0];
	return;
      }
      else if (e.type == NoExpose)
	;
      else if (e.type == Expose) {
	int x1 = e.xexpose.x;
	int y1 = e.xexpose.y;
	int x2 = x1 + e.xexpose.width - 1;
	int y2 = y1 + e.xexpose.height - 1;

	if (x1 < w->minx) w->minx = x1;
	if (y1 < w->miny) w->miny = y1;
	if (x2 > w->maxx) w->maxx = x2;
	if (y2 > w->maxy) w->maxy = y2;

	sb_make_picture_current (fildes);
      }
      else
	printf ("event type %d\n", e.type);
    }
    else if (nf == 0) {
      *valid = 0;
      return;
    }
    else if (nf > 0)
      printf ("bug in request_locator\n");
    else if (nf < 0) {
      if (errno == EINTR) {
	printf ("interrupted by a signal in request_locator, restarting timeout (%f seconds)\n", *ftimeout);
	timeout.tv_sec  = floor (*ftimeout);
	timeout.tv_usec = (*ftimeout - timeout.tv_sec) * 1000000;
      }
      else
	printf ("unexpected error %d in request_locator\n", errno);
    }
  }
  *valid = 1;
}

void
sb_set_locator (long *fildes, int *ordinal, int *x, int *y, int *z)
{
}

void
sb_track (int *indev, int *outdev, int *locator_num)
{
}

void
sb_echo_type (long *fildes, int *echo_number, int *echo_value, float *x, float *y, float *z)
{
}

void
sb_gerr_print_control (int *print_gerr)
{
}

void
sb_text_orientation2d (long *fildes, float *up_x, float *up_y, float *base_x, float *base_y)
{
  WinData *w = (WinData *) *fildes;
  w->up_x = *up_x;
  w->up_y = *up_y;
  w->base_x = *base_x;
  w->base_y = *base_y;
}

void
sb_get_colors (void)
{
  static char *name[COLOR_COUNT];
  int n;
  Colormap cmap;
  XColor exact_def;

  name[BLACK] = "black";
  name[WHITE] = "white";
  name[RED]   = "red";
  name[GREEN] = "green";
  name[BLUE]  = "blue";

  cmap = DefaultColormap (display, screen);
  for (n = 0; n < COLOR_COUNT; n++) {
    XParseColor (display, cmap, name[n], &exact_def) || DIE;
    XAllocColor (display, cmap, &exact_def) || DIE;
    colors[n] = exact_def.pixel;
  }
}

int
sb_gclose (long *fildes)
{
  WinData *w = (WinData *) *fildes;
//  printf ("destroy %d\n", (int)w->win); //
  XDestroyWindow (display, w->win);
  XFreePixmap (display, w->pix);
  XFlush (display);
  free (w);
  return 0;
}

void
sb_background_color (long *fildes, float *red, float *green, float *blue)
{
  WinData *w = (WinData *) *fildes;
  static XGCValues values;
  XColor c;
  
  c.red   = *red   * 65535;
  c.green = *green * 65535;
  c.blue  = *blue  * 65535;
  XAllocColor (display, DefaultColormap (display, screen), &c) || DIE;
  XSetWindowBackground (display, w->win, c.pixel);

  values.background = c.pixel;
  values.foreground = c.pixel;
  XChangeGC (display, w->line_gc, GCBackground, &values);
  XChangeGC (display, w->text_gc, GCBackground, &values);
  XChangeGC (display, w->edge_gc, GCBackground, &values);
  XChangeGC (display, w->fill_gc, GCBackground, &values);
  XChangeGC (display, w->back_gc, GCBackground|GCForeground, &values);
}

static long
sb_gopen (int *width_p, int *height_p, int *x_p, int *y_p, char* title_in, int three_d)
{
  int x = *x_p, y = *y_p;
  unsigned int border_width = 0;
  static XGCValues values;
  unsigned long mask;
  WinData *w;
  XSizeHints hints;
  XWindowAttributes wa;
  char title[21];
  char *titlep = title;
  XTextProperty tprop;
  unsigned long valuemask;
  XSetWindowAttributes attributes;

  if (display == 0) {
    display = XOpenDisplay (0);
    sb_get_colors ();
  }
  
  w = calloc (1, sizeof (WinData));
  w->up_x = 0;
  w->up_y = 1;
  w->base_x = 1;
  w->base_y = 0;
  w->tax = TA_NORMAL_HORIZONTAL;
  w->tay = TA_NORMAL_VERTICAL;

  strncpy (title, title_in, 21);
  title[20] = 0;
  w->width = *width_p;
  w->height = *height_p;

  valuemask = CWBackingStore | CWBackPixel | CWBorderPixel;
  //valuemask = CWBackPixel | CWBorderPixel;

  attributes.backing_store = Always;
  attributes.background_pixel = WhitePixel(display, screen);
  attributes.border_pixel = BlackPixel(display, screen);


  w->win = XCreateWindow(display, RootWindow(display, screen),
			       x, y, w->width, w->height, border_width = 0,
			       CopyFromParent, CopyFromParent, CopyFromParent,
			       valuemask, &attributes);

  XStringListToTextProperty (&titlep, 1, &tprop);
  XSetWMName (display, w->win, &tprop);
  hints.x = x;
  hints.y = y;
  if (x < 0 || y < 0) {
    Window root;
    int rx, ry;
    unsigned int rw, rh, depth;
    XGetGeometry (display, DefaultRootWindow (display), &root, &rx, &ry, &rw, &rh, &border_width, &depth);
    if (x < 0)
      hints.x = rw - w->width + x;
    if (y < 0)
      hints.y = rh - w->height + y;

  }
  hints.flags = USPosition;
  XSetNormalHints (display, w->win, &hints);
  XSelectInput(display, w->win, ButtonPressMask | ExposureMask | KeyPressMask);

  wm_protocols = XInternAtom(display, "WM_PROTOCOLS", False);
  wm_delete_window = XInternAtom(display, "WM_DELETE_WINDOW", False);
  XChangeProperty(display, w->win, wm_protocols, XA_ATOM, 32,
                  PropModeAppend, (unsigned char * ) &wm_delete_window, 1);

  XMapWindow (display, w->win);
  XSync (display, 0);
  XGetWindowAttributes (display, w->win, &wa);
  w->width = wa.width;
  w->height = wa.height;
  w->pix = XCreatePixmap (display, w->win, wa.width, wa.height, wa.depth);
  //XWindowEvent (display, w->win, ExposureMask, &e); removed this line - problem with backing store
  values.foreground = colors[WHITE];
  values.background = colors[BLACK];
  values.graphics_exposures = 0;
  values.dashes = 1;
  //mask = GCForeground | GCBackground | GCDashList;
  mask = GCForeground | GCBackground | GCDashList | GCGraphicsExposures;
  w->line_gc = XCreateGC (display, w->win, mask, &values);
  w->text_gc = XCreateGC (display, w->win, mask, &values);
  w->fill_gc = XCreateGC (display, w->win, mask, &values);
  w->edge_gc = XCreateGC (display, w->win, mask, &values);
  w->back_gc = XCreateGC (display, w->win, mask, &values);

  XSync (display, 0);
  XGetWindowAttributes (display, w->win, &wa);

  return (long)w;
}

void
sb_clip_indicator (long *fildes, int *clip_level)
{
  WinData *w = (WinData *) *fildes;
  w->clip = *clip_level;
}

void
sb_text_path (long *fildes, int *path)
{
  WinData *w = (WinData *) *fildes;
  w->text_path = *path;
}

void
sb_text_line_path (long *fildes, int *path)
{
  WinData *w = (WinData *) *fildes;
  w->text_line_path = *path;
}

static int p;

void
append_text (long *fildes, char *string_in, int *xform, int *more, int slen)
{
  if (p) {
    char *s = malloc (slen + 1);
    memcpy (s, string_in, slen);
    s[slen] = 0;
    while (slen > 0 && (s[slen - 1] == ' ' || s[slen - 1] == 0))
      s[--slen] = 0;
    ps_append_text (string_in, xform);
    free (s);
  }
  else   sb_append_text (fildes, string_in, xform, more, slen);
}

void
character_height (long *fildes, float *height)
{
  if (p) ps_character_height (height);
  else   sb_character_height (fildes, height);
}

void
character_width (long *fildes, float *width)
{
  if (p) ps_character_width (width);
  else   sb_character_width (fildes, width);
}

void
draw2d (long *fildes, float *x, float *y)
{
  if (p) ps_draw2d (x, y);
  else   sb_draw2d (fildes, x, y);
}

void
intdraw2d (long *fildes, int *x, int *y)
{
  float xf = *x;
  float yf = *y;
  if (p) ps_draw2d (&xf, &yf);
  else   sb_draw2d (fildes, &xf, &yf);
}

void
line_type (long *fildes, int *style)
{
  if (p) ps_line_type (style);
  else   sb_line_type (fildes, style);
}

void
mapping_mode (long *fildes, int *distort)
{
  if (p) ps_mapping_mode (distort);
  else   sb_mapping_mode (fildes, distort);
}

void
move2d (long *fildes, float *x, float *y)
{
  if (p) ps_move2d (x, y);
  else   sb_move2d (fildes, x, y);
}

void
intmove2d (long *fildes, int *x, int *y)
{
  float xf = *x;
  float yf = *y;
  if (p) ps_move2d (&xf, &yf);
  else   sb_move2d (fildes, &xf, &yf);
}

void
rectangle (long *fildes, float *x1, float *y1, float *x2p, float *y2p)
{
  if (p) ps_rectangle (x1, y1, x2p, y2p);
  else   sb_rectangle (fildes, x1, y1, x2p, y2p);
}

void
text2d (long *fildes, float *x_in, float *y_in, char *string_in, int *xform, int *more, int slen)
{
  if (p) {
    char *s = malloc (slen + 1);
    memcpy (s, string_in, slen);
    s[slen] = 0;
    while (slen > 0 && (s[slen - 1] == ' ' || s[slen - 1] == 0))
      s[--slen] = 0;
    ps_text2d (x_in, y_in, s, xform, slen);
    free (s);
  }
  else   sb_text2d (fildes, x_in, y_in, string_in, xform, more, slen);
}

void
vdc_extent (long *fildes, float *xmin, float *ymin, float *zmin, float *xmax, float *ymax, float *zmax)
{
  if (p) ps_vdc_extent (xmin, ymin, zmin, xmax, ymax, zmax);
  else   sb_vdc_extent (fildes, xmin, ymin, zmin, xmax, ymax, zmax);
}

void
view_port (long *fildes, float *x1, float *y1, float *x2, float *y2)
{
  if (p) ps_view_port (x1, y1, x2, y2);
  else   sb_view_port (fildes, x1, y1, x2, y2);
}

void
view_window (long *fildes, float *x1, float *y1, float *x2, float *y2)
{
  if (p) ps_view_window (x1, y1, x2, y2);
  else   sb_view_window (fildes, x1, y1, x2, y2);
}

void
gclose (long *fildes)
{
  if (p) ps_fclose ();
  else   sb_gclose (fildes);
}

long
gopen (int *width_p, int *height_p, int *x_p, int *y_p, char* title_in, int len)
{
  if (p) {
    char *s;
    TMALLOC (s, len + 4);
    sprintf (s, "%s.ps", title_in);
    ps_fopen (s);
    free (s);
    return 0;
  }
  else {
     char *title = strndup(title_in,len);
     long ret;
     ret = sb_gopen (width_p, height_p, x_p, y_p, title, 0);
     free(title);
     return ret;
  }
}

long
gopen3d (int *width_p, int *height_p, int *x_p, int *y_p, char* title_in, int len)
{
  char *title = strndup(title_in,len);
  long ret;
  return sb_gopen (width_p, height_p, x_p, y_p, title, 1);
  free(title);
  return ret;
}

void
text_font_index (long *fildes, int *index)
{
  if (p) ;
  else   sb_text_font_index (fildes, index);
}

void
clear_view_surface (long *fildes)
{
  if (p) ;
  else   sb_clear_view_surface (fildes);
}

void
clear_control (long *fildes, int *mode)
{
  if (p) ;
  else   sb_clear_control (fildes, mode);
}

void
make_picture_current (long *fildes)
{
  if (p) ;
  else   sb_make_picture_current (fildes);
}

void
line_color (long *fildes, float *red, float *green, float *blue)
{
  if (p) ps_line_color (red, green, blue);
  else   sb_line_color (fildes, red, green, blue);
}

void
shade_mode (long *fildes, int *mode, int *shading)
{
  if (p) ;
  else   sb_shade_mode (fildes, mode, shading);
}

void
text_color (long *fildes, float *red, float *green, float *blue)
{
  if (p) ps_text_color (red, green, blue);
  else   sb_text_color (fildes, red, green, blue);
}

void
get_colors (void)
{
  if (p) ;
  else   sb_get_colors ();
}

void
world_to_phys (WinData *w, float *x, float *y, int *phys_x, int *phys_y)
{
  if (p) ;
  else   sb_world_to_phys (w, x, y, phys_x, phys_y);
}

void
set_print (int *do_print, int *old_val)
{
  if (old_val) *old_val = p;
  p = *do_print;
}

int
is_print (void)
{
  return p;
}

void
perimeter_color (long *fildes, float *red, float *green, float *blue)
{
  if (p) ps_perimeter_color (red, green, blue);
  else   sb_perimeter_color (fildes, red, green, blue);
}

void
interior_style (long *fildes, int *style, int *edged)
{
  if (p) ps_interior_style (style, edged);
  else   sb_interior_style (fildes, style, edged);
}

void
background_color (long *fildes, float *red, float *green, float *blue)
{
  if (p) ;
  else   sb_background_color (fildes, red, green, blue);
}

void
track (int *indev, int *outdev, int *locator_num)
{
  if (p) ;
  else   sb_track (indev, outdev, locator_num);
}

void
echo_type (long *fildes, int *echo_number, int *echo_value, float *x, float *y, float *z)
{
  if (p) ;
  else   sb_echo_type (fildes, echo_number, echo_value, x, y, z);
}

void
set_locator (long *fildes, int *ordinal, int *x, int *y, int *z)
{
  if (p) ;
  else   sb_set_locator (fildes, ordinal, x, y, z);
}

void
gerr_print_control (int *print_gerr)
{
  if (p) ;
  else   sb_gerr_print_control (print_gerr);
}

void
vdc_to_wc (long *fildes, float *vdcx, float *vdcy, float *vdcz, float *wcx, float *wcy, float *wcz)
{
  if (p) ;
  else   sb_vdc_to_wc (fildes, vdcx, vdcy, vdcz, wcx, wcy, wcz);
}

void
text_orientation2d (long *fildes, float *up_x, float *up_y, float *base_x, float *base_y)
{
  if (p) ps_text_orientation2d (up_x, up_y, base_x, base_y);
  else   sb_text_orientation2d (fildes, up_x, up_y, base_x, base_y);
}

void
fill_color (long *fildes, float *red, float *green, float *blue)
{
  if (p) ps_fill_color (red, green, blue);
  else   sb_fill_color (fildes, red, green, blue);
}

void
request_locator (long *fildes, int *ordinal, float *ftimeout, int *valid, float *x, float *y, float *z)
{
  if (p) ;
  else   sb_request_locator (fildes, ordinal, ftimeout, valid, x, y, z);
}

void
text_alignment (long *fildes, int *tax, int *tay, int *h, int *v)
{
  if (p) ps_text_alignment (tax, tay, h, v);
  else   sb_text_alignment (fildes, tax, tay, h, v);
}

/* unimplemented */

/* may not be needed */
void
clip_indicator (long *fildes, int *clip_level)
{
  if (p) ps_clip_indicator (clip_level);
  else   sb_clip_indicator (fildes, clip_level);
}

/* used in first 5 gravity outside threed */

void
text_path (long *fildes, int *path) /* no-op, only used to set PATH_RIGHT, the default */
{
  if (p) ps_text_path (path);
  else   sb_text_path (fildes, path);
}

void
text_line_path (long *fildes, int *path) /* no-op, only used to set PATH_DOWN, the default */
{
  if (p) ps_text_line_path (path);
  else   sb_text_line_path (fildes, path);
}

void
wc_to_vdc (long *fildes, float *wcx, float *wcy, float *wcz, float *vdcx, float *vdcy, float *vdcz)
{
  if (p) ;
  else   sb_wc_to_vdc (fildes, wcx, wcy, wcz, vdcx, vdcy, vdcz);
}

void
ellipse (long *fildes, float *x_radius, float *y_radius, float *x_center, float *y_center, float *rotation)
{
  if (p) ;
  else   sb_ellipse (fildes, x_radius, y_radius, x_center, y_center, rotation);
}

void
line_width (long *fildes, float *width, int *mode)
{
  if (p) ;
  else sb_line_width (fildes, width, mode);
}

void
character_expansion_factor (long *fildes, float *factor)
{
  if (p) ;
  else sb_character_expansion_factor (fildes, factor);
}

void
line_endpoint (long *fildes, int *endpoint)
{
}

/* used in first five gravity only in threed */

void
view_volume (long *fildes, float *x1, float *y1, float *z1, float *x2, float *y2, float *z2)
{
}

void
view_camera (long *fildes, float *camera)
{
}

void
set_p1_p2 (long *fildes, int *units,  float *p1_x, float *p1_y, float *p1_z, float *p2_x, float *p2_y, float *p2_z)
{
}

void
move3d (long *fildes, float *x, float *y, float *z)
{
}

void
draw3d (long *fildes, float *x, float *y, float *z)
{
}

void
polyline3d (long *fildes, float *clist, int *numpts, int *flags)
{
}

void
text3d (long *fildes, float *x, float *y, float *z, char *string, int *xform, int *more, int slen)
{
}

void
line_repeat_length (long *fildes, float *length)
{
}

void
text_orientation3d (long *fildes, float *up_x, float *up_y, float *up_z, float *base_x, float *base_y, float *base_z)
{
}
