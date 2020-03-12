#define _GNU_SOURCE
#include <string.h>
#include <error.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <X11/Xlib.h>
#include <GL/glx.h>
#include <GL/glu.h>
#include <X11/Xatom.h>
#include <pthread.h>
#include <png.h>
#include "windata.h"
#include "sbparam.h"
#include "ogl_sb.h"

#define DBLBUF 1
static int Debug=0;
static int screen;
static Display *display;

void
fill_dither (long *fildes, int n)
{
}

static void
flip_bw (unsigned char *d, int pxlcnt)
{
  typedef struct {uint8_t r; uint8_t g; uint8_t b;}  Pxl;
  assert (sizeof (Pxl) == 3);
  Pxl *p = (Pxl *)d;
  int black = 0, white = 0;
  for (int i = 0; i < pxlcnt; i++)
    if (p[i].r == 0 && p[i].g == 0 && p[i].b == 0)
      black++;
    else if (p[i].r == 255 && p[i].g == 255 && p[i].b == 255)
      white++;
  if (black < white * 2)
    return;
  for (int i = 0; i < pxlcnt; i++)
    if (p[i].r == 0 && p[i].g == 0 && p[i].b == 0)
      p[i].r = 255, p[i].g = 255, p[i].b = 255;
    else //if (p[i].r == 255 && p[i].g == 255 && p[i].b == 255)
      p[i].r = 0, p[i].g = 0, p[i].b = 0;
}

void
x_to_wc (WinData *w, int win_x, int win_y, float *vdcx, float *vdcy, float *vdcz, float *wcx, float *wcy, float *wcz)
{
  int win_z;

  GLint viewport[4];
  glGetIntegerv (GL_VIEWPORT, viewport);

  GLdouble projection[16];
  glGetDoublev (GL_PROJECTION_MATRIX, projection);
  win_y = viewport[3] - win_y;
  glReadPixels (win_x, win_y, 1, 1, GL_DEPTH_COMPONENT, GL_INT, &win_z);
  GLdouble x, y, z;
  GLdouble modelview[16];

  glGetDoublev (GL_MODELVIEW_MATRIX, modelview);
  gluUnProject (win_x, win_y, win_z, modelview, projection, viewport, &x, &y, &z);
  *wcx = x;
  *wcy = y;
  *wcz = z;
  
  glPushMatrix ();
  glLoadIdentity ();
  glTranslatef (w->T[0], w->T[1], w->T[2]);
  glScalef (w->S[0], w->S[1], w->S[2]);
  glGetDoublev (GL_MODELVIEW_MATRIX, modelview);
  gluUnProject (win_x, win_y, win_z, modelview, projection, viewport, &x, &y, &z);
  *vdcx = x;
  *vdcy = y;
  *vdcz = z;

  glPopMatrix ();
}

static bool
write_png (char *file_name, unsigned char *d, int w, int h)
{
   FILE *fp;
   png_structp png_ptr;
   png_infop info_ptr;
   png_colorp palette;

   fp = fopen (file_name, "wb");
   if (fp == NULL)
      return false;

   png_ptr = png_create_write_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
   
   if (png_ptr == NULL) {
     fclose (fp);
     return false;
   }

   info_ptr = png_create_info_struct (png_ptr);
   if (info_ptr == NULL) {
     fclose (fp);
     png_destroy_write_struct (&png_ptr, NULL);
     return false;
   }

   if (setjmp(png_jmpbuf(png_ptr))) {
     /* If we get here, we had a problem writing the file */
     fclose(fp);
     png_destroy_write_struct (&png_ptr, &info_ptr);
     return false;
   }

   png_init_io (png_ptr, fp);

   png_set_IHDR (png_ptr, info_ptr, w, h, 8, PNG_COLOR_TYPE_RGB,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

   png_set_sRGB_gAMA_and_cHRM (png_ptr, info_ptr, PNG_sRGB_INTENT_RELATIVE);

   png_write_info (png_ptr, info_ptr);

   png_bytep row_pointers[h];
   for (int i = 0; i < h; i++)
     row_pointers[h - 1 - i] = d + i * w * 3;

   png_write_image (png_ptr, row_pointers);

   png_write_end(png_ptr, info_ptr);

   png_destroy_write_struct (&png_ptr, &info_ptr);

   fclose (fp);

   return true;
}

void
print_window (long *fildes, char *pname, enum PW where)
{
  WinData *v = (WinData *) *fildes;
  int w = v->width;
  int h = v->height;
  double win = w / v->resolution; /* width in inches */
  double hin = h / v->resolution; /* height in inches */
  double lm = .55;            /* left margin (room for holes) */
  double rm = .25;            /* right margin */
  double tm = .25;            /* top margin */
  double bm = .25;            /* bottom margin */
  double uph = 11 - tm - bm;  /* usable paper height */
  double upw = 8.5 - lm - rm; /* usable paper width */
  bool landscape = w > h && win > upw && where != PW_MOVIE;
  double tx, ty, sx, sy;
  if (landscape) {
    double scale1 = win > uph ? uph / win : 1;
    double scale2 = hin > upw ? upw / hin : 1;
    double scale = fmin (scale1, scale2);
    sx = win * scale;
    sy = hin * scale;
    rm += (upw - sy) / 2;
    bm += (uph - sx) / 2;
    tx = 8.5 - rm;
    ty = bm;
  }
  else {
    double scale1 = win > upw ? upw / win : 1;
    double scale2 = hin > uph ? uph / hin : 1;
    double scale = fmin (scale1, scale2);
    sx = win * scale;
    sy = hin * scale;
    lm += (upw - sx) / 2;
    bm += (uph - sy) / 2;
    tx = lm;
    ty = bm;
  }
  unsigned char *d = malloc (w * h * 3);
  glReadPixels (0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, d);

  if (where == PW_MOVIE) {
    if (!write_png (pname, d, w, h))
      error (0, 0, "Can't write %s", pname);
  }
  else {
    flip_bw (d, w * h);
    char *cmd;
    if (asprintf (&cmd, "lpr -P '%s'", pname) == -1) exit (1);
    //  printf ("cmd: %s\n", cmd);
    bool isfile = where == PW_FILE;
    FILE *f = isfile ? fopen (pname, "w") : popen (cmd, "w");
    free (cmd);
    fprintf (f,
             "%%!PS-Adobe-3.0\n"
             "72 72 scale\n"
             "%g %g translate\n"
             "%s"
             "%g %g scale\n"
             "%d %d 8\n"
             "[%d 0 0 %d 0 0]\n"
             "currentfile\n"
             "false 3\n"
             "colorimage\n",
             tx, ty,
             landscape ? "90 rotate\n" : "",
             sx, sy, w, h, w, h);
    fwrite (d, 1, w * h * 3, f);
    fprintf (f, "\nshowpage\n");
    where == isfile ? fclose (f) : pclose (f);
  }
  free (d);
}

void
character_width (long *fildes, float *width)
{
  WinData *w = (WinData *) *fildes;
  w->character_width = *width;
  w->character_width_valid = 1;
}

void
character_expansion_factor (long *fildes, float *expansion_factor)
{
  WinData *w = (WinData *) *fildes;
  w->character_expansion_factor = *expansion_factor;
  w->character_width_valid = 0;
}

void
echo_type (long *fildes, int *echo_number, int *echo_value, float *x, float *y, float *z)
{
}

void
track (int *indev, int *outdev, int *locator_num)
{
}

void
sample_choice (long *fildes, int *ordinal, int *valid, int *value)
{
  WinData *w = (WinData *) *fildes;
  Window root, child;
  int root_x, root_y, win_x, win_y, win_z;
  unsigned int mask;

  XQueryPointer (display, w->win, &root, &child, &root_x, &root_y, &win_x, &win_y, &mask);
  *valid = 1;
  *value = 0;
  if (mask & Button1Mask)
    *value = 1;
  if (mask & Button2Mask)
    *value = 2;
  if (mask & Button3Mask)
    *value = 3;
  return;
}

void
sample_locator_2 (long *fildes, int *ordinal, int *valid, float *vdcx, float *vdcy, float *vdcz, float *wcx, float *wcy, float *wcz)
{
  WinData *w = (WinData *) *fildes;
  Window root, child;
  int root_x, root_y, win_x, win_y, win_z;
  unsigned int mask;

  XQueryPointer (display, w->win, &root, &child, &root_x, &root_y, &win_x, &win_y, &mask);

  x_to_wc (w, win_x, win_y, vdcx, vdcy, vdcz, wcx, wcy, wcz);
}

void
clip_indicator (long *fildes, int *clip_level)
{
}

void
line_type (long *fildes, int *style)
{
  WinData *w = (WinData *) *fildes;
  w->line_type = *style;
}

void
line_repeat_length (long *fildes, float *len)
{
  WinData *w = (WinData *) *fildes;
  w->repeat_length = *len;
}

void
perimeter_color (long *fildes, float *red, float*green, float *blue)
{
  WinData *w = (WinData *) *fildes;
  w->peri_r = *red;
  w->peri_g = *green;
  w->peri_b = *blue;
}

void
interior_style (long *fildes, int *style, int *edged)
{
  WinData *w = (WinData *) *fildes;
  w->interior_style_val = *style;
  w->edged_val = *edged;
}

void
print_mat (float *m, char *name)
{
  printf ("%s:\n", name);
  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 4; c++)
      printf (" %11g", m[r * 4 + c]);
    printf ("\n");
  }
  printf ("\n");
}

void
fill_color (long *fildes, float *red, float *green, float *blue)
{
  WinData *w = (WinData *) *fildes;
  w->fill_r = *red;
  w->fill_g = *green;
  w->fill_b = *blue;
}

void
background_color (long *fildes, float *red, float *green, float *blue)
{
  glClearColor (*red, *green, *blue, 1);
}

void
clear_control (long *fildes, int *mode)
{
  WinData *w = (WinData *) *fildes;
  w->clear_mode = *mode;
}

void
clear_view_surface (long *fildes)
{
  WinData *w = (WinData *) *fildes;
  if (w->clear_mode == CLEAR_VDC_EXTENT || w->clear_mode == CLEAR_DISPLAY_SURFACE 
        || w->clear_mode == CLEAR_VIEWPORT) {
    glClear (GL_COLOR_BUFFER_BIT);
    return;
  }
  float rgb[4];
  glGetFloatv (GL_COLOR_CLEAR_VALUE, rgb);
  glPushMatrix ();
  glLoadIdentity ();
  glTranslatef (w->T[0], w->T[1], w->T[2]);
  glScalef (w->S[0], w->S[1], w->S[2]);
  glColor3f (rgb[0], rgb[1], rgb[2]);
  glBegin (GL_POLYGON);
  glVertex2f (w->px1, w->py1);
  glVertex2f (w->px1, w->py2);
  glVertex2f (w->px2, w->py2);
  glVertex2f (w->px2, w->py1);
  glEnd ();
  glPopMatrix ();
}

void
make_picture_current (void)
{
  if (DBLBUF) {
    glReadBuffer (GL_BACK);
    glDrawBuffer (GL_FRONT);

    XWindowAttributes wa;
    XGetWindowAttributes (display, glXGetCurrentDrawable (), &wa);
    glCopyPixels (0, 0, wa.width, wa.height, GL_COLOR);

    /* work around opengl bug on russhome */
    double clr[4];
    glGetDoublev (GL_CURRENT_COLOR, clr);
    glColor4f (0, 0, 0, 0);
    glBegin (GL_POINTS);
    glVertex4f (0, 0, 0, 1);
    glEnd ();
    glColor4dv (clr);
    
    glDrawBuffer (GL_BACK);
  }
  glFinish ();
}

void
wait_for_input_and_handle_exposures (void)
{
  int xfd = ConnectionNumber (display);
  while(1) {
    fd_set readfds;

    FD_ZERO (&readfds);
    FD_SET (xfd, &readfds);
    FD_SET (0, &readfds);

    select (xfd + 1, &readfds, NULL, NULL, NULL);
    if (FD_ISSET (0, &readfds))
      return;

    XEvent e;
    while (XPending (display)) {
      XNextEvent (display, &e);
      if (e.type == Expose && e.xexpose.count == 0)
        make_picture_current ();
    }      
  }
}

void
get_click (long *fildes, float *vdcx, float *vdcy, float *vdcz, float *wcx, float *wcy, float *wcz)
{
  WinData *w = (WinData *) *fildes;

  XEvent e;
  while (1) {
    XNextEvent (display, &e);
    if (e.type  == ButtonPress && e.xbutton.window == w->win)
      break;
    if (e.type == Expose && e.xexpose.count == 0)
      make_picture_current ();
  }

  x_to_wc (w, e.xbutton.x, e.xbutton.y, vdcx, vdcy, vdcz, wcx, wcy, wcz);
}

void
move2d (long *fildes, float *x, float *y)
{
  WinData *w = (WinData *) *fildes;
  w->pen_x = *x;
  w->pen_y = *y;
}

void
move3d (long *fildes, float *x, float *y, float *z)
{
  WinData *w = (WinData *) *fildes;
  w->pen_x = *x;
  w->pen_y = *y;
  w->pen_z = *z;
}

void
draw3d (long *fildes, float *x, float *y, float *z)
{
  WinData *w = (WinData *) *fildes;
  glColor3f (w->line_r, w->line_g, w->line_b);
  glBegin(GL_LINES);
  glVertex3f (w->pen_x, w->pen_y, w->pen_z);
  glVertex3f (*x, *y, *z);
  glEnd();
  w->pen_x = *x;
  w->pen_y = *y;
  w->pen_z = *z;
}

void
draw2d (long *fildes, float *x, float *y)
{
  WinData *w = (WinData *) *fildes;
  draw3d (fildes, x, y, &w->pen_z);
}

void
combineCallback (GLdouble coords[3], void *d[4], GLfloat w[4], void **dataOut)
{
  double *new = malloc (3 * sizeof (double));
  new[0] = coords[0];
  new[1] = coords[1];
  new[2] = coords[2];
  *dataOut = new;
}

static GLvoid
beginCallback (GLenum which)
{
  glBegin (which);
}

static GLvoid
endCallback (void)
{
  glEnd ();
}

static GLvoid
errorCallback (GLenum errorCode)
{
  const GLubyte *estring;

  estring = gluErrorString (errorCode);
  fprintf (stderr, "Tessellation Error: %s\n", estring);
  exit (0);
}

static void
vertexCallback (void *vertex_data)
{
  double *v = (double *)vertex_data;
  printf ("glVertex %g %g %g\n", v[0], v[1], v[2]);
  
  glVertex3dv (vertex_data);
}

static void
tess_polygon (GLUtesselator *tess, float *clist, int numpts)
{
  gluTessBeginPolygon (tess, NULL);
  gluTessBeginContour (tess);

  GLdouble v[numpts][3];
  for (int i = 0; i < numpts; i++) {
    v[i][0] = clist[i*3];
    v[i][1] = clist[i*3+1];
    v[i][2] = clist[i*3+2];
    gluTessVertex (tess, v[i], v[i]);
  }

  gluTessEndContour (tess);
  gluTessEndPolygon (tess);
}

void
polygon3d (long *fildes, float *clist, int *numpts, int *flags)
{
  WinData *w = (WinData *) *fildes;

  static GLUtesselator *tess;
  if (!tess) {
    tess = gluNewTess ();
    if (1)
      gluTessCallback (tess, GLU_TESS_VERTEX, (_GLUfuncptr)glVertex3dv  );
    else
      gluTessCallback (tess, GLU_TESS_VERTEX, (_GLUfuncptr)vertexCallback);
    gluTessCallback (tess, GLU_TESS_BEGIN , (_GLUfuncptr)beginCallback);
    gluTessCallback (tess, GLU_TESS_END   ,   (_GLUfuncptr)endCallback);
    gluTessCallback (tess, GLU_TESS_ERROR , (_GLUfuncptr)errorCallback);
    gluTessCallback (tess, GLU_TESS_COMBINE, (_GLUfuncptr)combineCallback);
  }
//  if (w->interior_style_val == INT_HOLLOW)
//    glColor3f (w->peri_r, w->peri_g, w->peri_b);
//  else
//    glColor3f (w->fill_r, w->fill_g, w->fill_b);
//  gluTessProperty (tess, GLU_TESS_BOUNDARY_ONLY, w->interior_style_val == INT_HOLLOW);
//
//  tess_polygon (tess, clist, *numpts);

  if (w->interior_style_val != INT_HOLLOW) {
    glColor3f (w->fill_r, w->fill_g, w->fill_b);
    gluTessProperty (tess, GLU_TESS_BOUNDARY_ONLY, false);
    tess_polygon (tess, clist, *numpts);
  }
  if (w->edged_val == 1) {
    glColor3f (w->peri_r, w->peri_g, w->peri_b);
    gluTessProperty (tess, GLU_TESS_BOUNDARY_ONLY, true);
    tess_polygon (tess, clist, *numpts);
  }
  return;
}

void
polygon2d (long *fildes, float *clist, int *numpts, int *flags)
{
  float v[*numpts][3];
  for (int i = 0; i < *numpts; i++) {
    v[i][0] = clist[i*2];
    v[i][1] = clist[i*2+1];
    v[i][2] = 0;
  }
  polygon3d (fildes, &v[0][0], numpts, flags);
}

void
ellipse (long *fildes, float *x_radius, float *y_radius, float *x_center, float *y_center, float *rotation)
{
  if (*rotation != 0) error_at_line (1, 0, __FILE__, __LINE__, "rotated ellipses not implemented");
  int numpts = 100;
  double increment = 2 * M_PI / numpts;
  float v[numpts][3];
  for (int i = 0; i < numpts; i++) {
    double a = i * increment;
    v[i][0] = *x_center + *x_radius * cos (a);
    v[i][1] = *y_center + *y_radius * sin (a);
    v[i][2] = 0;
  }
  int flags = 0;
  polygon3d (fildes, &v[0][0], &numpts, &flags);
}

static void
set_stipple (WinData *v)
{
  if (v->line_type == SOLID) {
    glDisable (GL_LINE_STIPPLE);
    return;
  }
  glEnable (GL_LINE_STIPPLE);
  int     x = nearbyint (fmin (v->p1x, v->p2x) * (v->width - 1));
  GLsizei w = nearbyint (fmax (v->p1x, v->p2x) * (v->width - 1)) - x + 1;
  double repeat_len_pxl = v->repeat_length * v->S[0] / 2 * w;
  double min = repeat_len_pxl / 8;
  double max = repeat_len_pxl / 4;
  int perdot;
  for (int i = 2; i <= 16; i *= 2)
    if (i >= min && i <= max) {
      perdot = i;
      break;
    }
  if (max < 2) perdot = 2;
  if (min > 16) perdot = 16;
  GLushort pat[] = {0,0,0xaaaa,0, 0x8888,0,0,0, 0x8080,0,0,0,0,0,0,0, 0x8000};
  assert (perdot == 2 || perdot == 4 || perdot == 8 || perdot == 16);
  GLushort pattern = pat[perdot];
  glLineStipple (1, pattern);
}

void
polyline3d (long *fildes, float *clist, int *numpts, int *flags)
{
  WinData *w = (WinData *) *fildes;
  glColor3f (w->line_r, w->line_g, w->line_b);
  set_stipple (w);
  glBegin (GL_LINE_STRIP);
  for (int i = 0; i < *numpts; i++)
    glVertex3fv (&clist[i * 3]);
  glEnd ();
  int n = (*numpts - 1) * 3;
  w->pen_x = clist[n];
  w->pen_y = clist[n + 1];
  w->pen_z = clist[n + 2];
}

void
rectangle (long *fildes, float *x1, float *y1, float *x2, float *y2)
{
  WinData *w = (WinData *) *fildes;
  float v[4][3] = 
    {{*x1, *y1, w->pen_z},
     {*x2, *y1, w->pen_z},
     {*x2, *y2, w->pen_z},
     {*x1, *y2, w->pen_z}};
  int numpts = 4;
  polygon3d (fildes, &v[0][0], &numpts, 0);
}

void
text_orientation2d (long *fildes, float *up_x, float *up_y, float *base_x, float *base_y)
{
  WinData *w = (WinData *) *fildes;
  w->up = (XYZ) {*up_x, *up_y, 0};
  w->up.x = *up_x;
  w->up.y = *up_y;
  w->up.z = 0;
  w->base.x = *base_x;
  w->base.y = *base_y;
  w->base.z = 0;
  norm (w->up);
  norm (w->base);
}

void
text_orientation3d (long *fildes, float *up_x, float *up_y, float *up_z, float *base_x, float *base_y, float *base_z)
{
  WinData *w = (WinData *) *fildes;
  w->up.x = *up_x;
  w->up.y = *up_y;
  w->up.z = *up_z;
  w->base.x = *base_x;
  w->base.y = *base_y;
  w->base.z = *base_z;
  w->up = norm (w->up);
  w->base = norm (w->base);
}

void
text_font_index (long *fildes, int *index)
{
  WinData *w = (WinData *) *fildes;
  w->font_index = *index;
}

void
text_color (long *fildes, float *red, float *green, float *blue)
{
  WinData *w = (WinData *) *fildes;

  w->text_r = *red;
  w->text_g = *green;
  w->text_b = *blue;
}

void
line_color (long *fildes, float *red, float *green, float *blue)
{
  WinData *w = (WinData *) *fildes;

  w->line_r = *red;
  w->line_g = *green;
  w->line_b = *blue;
}

void
text_alignment (long *fildes, int *tax, int *tay, int *h, int *v)
{
  WinData *w = (WinData *) *fildes;
  w->tax = *tax;
  w->tay = *tay;
}

void
character_height (long *fildes, float *height)
{
  WinData *w = (WinData *) *fildes;
  w->char_height = *height;
}

void
push (char *s)
{
  static int alloc;
  static int count;
  static char **stack;
  if (!s) {
    for (int i = 0; i < count; i++)
      free (stack[i]);
    count = 0;
    return;
  }
  if (count == alloc)
    stack = realloc (stack, ++alloc * sizeof *stack);
  stack[count++] = s;
}

char *
pfloat (float x)
{
  if (isnan (x)) {return "NaN";}
  if (x == 0) return "0";
  union {float f; uint32_t i;} u;
  assert (sizeof u == sizeof (float));
  assert (sizeof u == sizeof (uint32_t));
  u.f = x;
  int fraction = (u.i & 0x7fffff) | 0x800000;
  int exponent = (u.i >> 23) & 0xff;
  if (exponent == 0) {
    fraction = u.i & 0x7fffff;
    exponent = 1;
  }
  int sign = (u.i >> 31) & 1;
  char *s;
  if (exponent == 0xff)
    {if (asprintf (&s, "%s%s", sign ? "-" : "", "INF") == -1) exit (1);}
  else
    {if (asprintf (&s, "%s%d*2^%d", sign ? "-" : "", fraction, exponent-127-23)) exit (1);}
  push (s);
  return s;
}

char *
pdbl (double x)
{
  if (isnan (x)) {return "NaN";}
  if (x == 0) return "0";
  union {double f; uint64_t i;} u;
  assert (sizeof u == sizeof (double));
  assert (sizeof u == sizeof (uint64_t));
  u.f = x;
  uint64_t fraction = (u.i & 0xfffffffffffff) | 0x10000000000000;
  int exponent = (u.i >> 52) & 0x7ff;
  if (exponent == 0) {
    fraction = u.i & 0xfffffffffffff;
    exponent = 1;
  }
  int sign = (u.i >> 63) & 1;
  char *s;
  if (exponent == 0x7ff)
    {if (asprintf (&s, "%s%s", sign ? "-" : "", "INF") == -1) exit (1);}
  else
    {if (asprintf (&s, "%s%llu*2^%d", sign ? "-" : "", (long long unsigned) fraction, exponent-1023-52) == -1) exit (1);}
  push (s);
  return s;
}

static void
get_xywh (WinData *v, int *xp, int *yp, GLsizei *wp, GLsizei *hp, double *x1ndc, double *y1ndc, double *x2ndc, double *y2ndc)
{
  int     x = nearbyint (fmin (v->p1x, v->p2x) * (v->width - 1));
  GLsizei w = nearbyint (fmax (v->p1x, v->p2x) * (v->width - 1)) - x + 1;
  int     y = nearbyint (fmin (v->p1y, v->p2y) * (v->height - 1));
  GLsizei h = nearbyint (fmax (v->p1y, v->p2y) * (v->height - 1)) - y + 1;

  *x1ndc = -1 + 1. / w;
  *x2ndc =  1 - 1. / w;
  *y1ndc = -1 + 1. / h;
  *y2ndc =  1 - 1. / h;
  if (!v->mapping_distort) {
    double vdc_aspect = fabs ((v->xmax - v->xmin) / (v->ymax - v->ymin));
    double phys_aspect = (double) (w - 1) / (h - 1);
    if (vdc_aspect > phys_aspect) {
      double rh = (w - 1) / vdc_aspect;
      double bottom = v->bottom * (h - 1 - rh);
      double rylo = y + .5 + bottom;
      double ryhi = rylo + rh;
      double ylo = floor (rylo);
      double yhi = ceil (ryhi);
      double new_h = yhi - ylo;
      if (new_h < h) {
        y = ylo;
        h = new_h;
        *y1ndc = -1 + 2 * (rylo - ylo) / h;
        *y2ndc =  1 - 2 * (yhi - ryhi) / h;
      }
    }
    else if (vdc_aspect < phys_aspect) {
      double rw = (h - 1) * vdc_aspect;
      double left = v->left * (w - 1 - rw);
      double rxlo = x + .5 + left;
      double rxhi = rxlo + rw;
      double xlo = floor (rxlo);
      double xhi = ceil (rxhi);
      double new_w = xhi - xlo;
      if (new_w < w) {
        x = xlo;
        w = new_w;         
        *x1ndc = -1 + 2 * (rxlo - xlo) / w;
        *x2ndc =  1 - 2 * (xhi - rxhi) / w;
      }
    }
  }
  *xp = x;
  *yp = y;
  *wp = w;
  *hp = h;
}

static void
set_matrix (long *fildes)
{
  WinData *v = (WinData *) *fildes;
  int x, y;
  GLsizei w, h;
  double x1ndc, y1ndc, x2ndc, y2ndc;
  get_xywh (v, &x, &y, &w, &h, &x1ndc, &y1ndc, &x2ndc, &y2ndc);
  glViewport (x, y, w, h);
  glDepthRange (v->p1z, v->p2z);

  if (0) {
    printf ("x: %d, y: %d, w: %d, h: %d\n", x, y, w, h);
    printf ("x1ndc: %g, y1ndc: %g, x2ndc: %g, y2ndc: %g\n", x1ndc, y1ndc, x2ndc, y2ndc);
    printf ("xmin %g, ymin %g, zmin %g, xmax %g, ymax %g, zmax %g\n", v->xmin, v->ymin, v->zmin, v->xmax, v->ymax, v->zmax);
  }

  /* vdc to ndc matrix */
  v->S[0] = (x1ndc - x2ndc) / (v->xmin - v->xmax);
  v->S[1] = (y1ndc - y2ndc) / (v->ymin - v->ymax);
  v->S[2] = 2 / (v->zmax - v->zmin);
  v->T[0] = (x2ndc * v->xmin - x1ndc * v->xmax) / (v->xmin - v->xmax);
  v->T[1] = (y2ndc * v->ymin - y1ndc * v->ymax) / (v->ymin - v->ymax);
  v->T[2] = (v->zmin + v->zmax) / (v->zmin - v->zmax);

  //  printf ("ndc / vdc Sy: %s = %g\n", pfloat (v->S[1]), v->S[1]);

  if (0) {
    for (int i = 0; i < 3; i++) printf ("S[%d] = %g\n", i, v->S[i]);
    for (int i = 0; i < 3; i++) printf ("T[%d] = %g\n", i, v->T[i]);
  }

  glLoadIdentity ();
  glTranslatef (v->T[0], v->T[1], v->T[2]);
  glScalef (v->S[0], v->S[1], v->S[2]);
}

void
wc_to_vdc (long *fildes, float *wcx, float *wcy, float *wcz, float *vdcx, float *vdcy, float *vdcz)
{
  float *m = wc_to_vdc_matrix;
  float x = *wcx;
  float y = *wcy;
  float z = *wcz;
  float X = x * m[0] + y * m[4] + z * m[ 8] + m[12];
  float Y = x * m[1] + y * m[5] + z * m[ 9] + m[13];
  float Z = x * m[2] + y * m[6] + z * m[10] + m[14];
  *vdcx = X;
  *vdcy = Y;
  *vdcz = Z;
}

void
view_volume (long *fildes, float *vx1, float *vy1, float *vz1, float *vx2, float *vy2, float *vz2)
{
  WinData *w = (WinData *) *fildes;
  /* wc to vdc matrix */
  double Sx = (w->px1 - w->px2) / (*vx1 - *vx2);
  double Sy = (w->py1 - w->py2) / (*vy1 - *vy2);
  double Sz = (w->zmin - w->zmax) / (*vz1 - *vz2);
  double Tx = (w->px2 * *vx1 - w->px1 * *vx2) / (*vx1 - *vx2);
  double Ty = (w->py2 * *vy1 - w->py1 * *vy2) / (*vy1 - *vy2);
  double Tz =  -(*vz2 * w->zmin - *vz1 * w->zmax) / (*vz1 - *vz2);
  //  printf ("vdc / wc Sy: %s = %g\n", pdbl (Sy), Sy);
  glLoadIdentity ();
  glTranslatef (w->T[0], w->T[1], w->T[2]);
  glScalef (w->S[0], w->S[1], w->S[2]);

  glPushMatrix ();
  glLoadIdentity ();
  glTranslatef (Tx, Ty, Tz);
  glScalef (Sx, Sy, Sz);
  glGetFloatv (GL_MODELVIEW_MATRIX, wc_to_vdc_matrix);
  glPopMatrix ();
  glMultMatrixf (wc_to_vdc_matrix);
  
  //  printf ("vv: %g %g %g %g %g %g\n", Tx, Ty, Tz, Sx, Sy, Sz);
  w->vx1 = *vx1;
  w->vy1 = *vy1;
  w->vz1 = *vz1;
  w->vx2 = *vx2;
  w->vy2 = *vy2;
  w->vz2 = *vz2;
  if (0) {
    float m[16];
    glGetFloatv (GL_MODELVIEW_MATRIX, m);
    for (int i = 0; i < 16; i++)
      printf (" %g", m[i]);
    printf ("\n");
  }
}

void
view_port (long *fildes, float *x1, float *y1, float *x2, float *y2)
{
  WinData *w = (WinData *) *fildes;
  w->px1 = *x1;
  w->py1 = *y1;
  w->px2 = *x2;
  w->py2 = *y2;
}

void
vdc_extent (long *fildes, float *xmin, float *ymin, float *zmin, float *xmax, float *ymax, float *zmax)
{
  WinData *w = (WinData *) *fildes;
  w->xmin = *xmin;
  w->ymin = *ymin;
  w->zmin = *zmin;
  w->xmax = *xmax;
  w->ymax = *ymax;
  w->zmax = *zmax;
  view_port (fildes, xmin, ymin, xmax, ymax);
  set_matrix (fildes);
}

void
vdc_justification (long *fildes, float *left, float *bottom)
{
  WinData *w = (WinData *) *fildes;
  w->left = *left;
  w->bottom = *bottom;
}

void
set_p1_p2 (long *fildes, int *units,  float *p1x, float *p1y, float *p1z, float *p2x, float *p2y, float *p2z)
{
  WinData *w = (WinData *) *fildes;
  w->p1x = *p1x;
  w->p1y = *p1y;
  w->p1z = *p1z;
  w->p2x = *p2x;
  w->p2y = *p2y;
  w->p2z = *p2z;
}

void
mapping_mode (long *fildes, int *distort)
{
  WinData *w = (WinData *) *fildes;
  w->mapping_distort = *distort;
}

static void
tr_sc (float *a, double Tx, double Ty, double Tz, double Sx, double Sy, double Sz)
{
  float (*m)[4];
  m = (float (*)[4])a;
  for (int i = 0; i < 4; i++) {
    m[i][0] = m[i][0] * Sx + Tx * m[i][3];
    m[i][1] = m[i][1] * Sy + Ty * m[i][3];
    m[i][2] = m[i][2] * Sz + Tz * m[i][3];
  }
}

typedef int GLint;
typedef int GLsizei;
typedef double GLclampd;
typedef float GLfloat;
float matrix[16];

void
view_window (long *fildes, float *vx1, float *vy1, float *vx2, float *vy2)
{
  float vz1 = 0;
  float vz2 = 1;
  view_volume (fildes, vx1, vy1, &vz1, vx2, vy2, &vz2);
}

void
mul_vdc_dc (long *fildes, float *retmat)
{
  WinData *v = (WinData *) *fildes;
  int x, y;
  GLsizei w, h;
  double x1ndc, y1ndc, x2ndc, y2ndc;
  get_xywh (v, &x, &y, &w, &h, &x1ndc, &y1ndc, &x2ndc, &y2ndc);

  double p1xp = x - .5;
  double p1yp = v->height - y - .5;
  double p1zp = v->p1z * 2147450880;
  double p2xp = x + w - .5;
  double p2yp = v->height - h - y - .5;
  double p2zp = v->p2z * 2147450880;

  /* ndc to dc matrix */
  double Sx = (p2xp - p1xp) / 2;
  double Sy = (p2yp - p1yp) / 2;
  double Sz = (p2zp - p1zp) / 2;
  double Tx = (p2xp + p1xp) / 2;
  double Ty = (p2yp + p1yp) / 2;
  double Tz = (p2zp + p1zp) / 2;

  memcpy (retmat, matrix, sizeof matrix);
  //  printf ("dc / ndc Sy: %s = %g\n", pdbl (Sy), Sy);
  tr_sc (retmat, Tx, Ty, Tz, Sx, Sy, Sz);
}

static void
windata_init (WinData *w)
{
  w->p1x = 0;
  w->p1y = 0;
  w->p1z = 0;
  w->p2x = 1;
  w->p2y = 1;
  w->p2z = 1;
  w->xmin = 0;
  w->ymin = 0;
  w->zmin = 0;
  w->xmax = 1;
  w->ymax = 1;
  w->zmax = 1;
  w->px1 = 0;
  w->py1 = 0;
  w->px2 = 1;
  w->py2 = 1;
  w->mapping_distort = 0;
  w->left = .5;
  w->bottom = .5;
  w->char_height = 1. / 50;
  w->tax = TA_LEFT;
  w->tay = TA_BASE;
  w->text_r = w->text_g = w->text_b = 1;
  w->peri_r = w->peri_g = w->peri_b = 1;
  w->line_r = w->line_g = w->line_b = 1;
  w->font_index = 1;
  w->up = (XYZ){0,1,0};
  w->base = (XYZ){1,0,0};
  free (w->string);
  w->string = 0;
  w->stringlen = 0;
  w->pen_x = 0;
  w->pen_y = 0;
  w->pen_z = 0;
  w->clear_mode = CLEAR_VDC_EXTENT;
  w->interior_style_val = INT_SOLID;
  w->edged_val = 0;
  w->repeat_length = 1. / 32;
  w->line_type = SOLID;
  w->character_expansion_factor = 1;
  w->character_width = 1;
  w->character_width_valid = 0;
}

int gclose (long *fildes)
{
  WinData *w = (WinData *) *fildes;
  if (w == 0) return 0;
  *fildes = 0;

  if (w->font[1]) ftglDestroyFont (w->font[1]);
  if (w->font[4]) ftglDestroyFont (w->font[4]);
  if (w->font[6]) ftglDestroyFont (w->font[6]);
  XDestroyWindow (display, w->win);
  free (w);
}


int *rp;

long
gopen3d (int *width_p, int *height_p, int *x_p, int *y_p, char* title_in, int len)
{
  static bool thread_init_done = true;
  if (!thread_init_done) {
    printf ("init\n");
    if (XInitThreads () == 0)
      printf ("The xlib implementation on this system does not support threads.  Expect problems.\n");
    thread_init_done = true;
  }

  //  printf ("glu version: %s\n", gluGetString (GLU_VERSION));
  //  printf ("glu extensions: %s\n", gluGetString (GLU_EXTENSIONS));
  int x = *x_p, y = *y_p;

  WinData *w = calloc (1, sizeof (WinData));
  windata_init (w);
  w->width = *width_p;
  w->height = *height_p;
  memset (w->font, 0, sizeof w->font);
  w->font[1] = ftglCreatePolygonFont ("/usr/local/share/fonts/hp1.pfb");
  w->font[4] = ftglCreatePolygonFont ("/usr/local/share/fonts/hp4.pfb");
  w->font[6] = ftglCreateTextureFont ("/usr/local/share/fonts/hp6.pfb");

  if (display == 0)
    display = XOpenDisplay (0);
  int wpx = DisplayWidth (display, screen);
  int wmm = DisplayWidthMM (display, screen);
  w->resolution = nearbyint (25.4 * wpx / wmm);
  rp = &w->resolution;
  //  printf ("wpx: %d, wmm: %d, resolution: %d %d, w: %p\n", wpx, wmm, w->resolution, *rp, w);

  int AttributeList[] = {
#if DBLBUF
    GLX_DOUBLEBUFFER,  True,  /* Request double-buffered */
#endif
    GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
    GLX_RENDER_TYPE,   GLX_RGBA_BIT,
    GLX_CONFIG_CAVEAT, GLX_NONE,
    GLX_RED_SIZE,      1,   /* Request a single buffered color buffer */
    GLX_GREEN_SIZE,    1,   /* with the maximum number of color bits  */
    GLX_BLUE_SIZE,     1,   /* for each component                     */
    None
  };
  GLXFBConfig *fbConfigs;
  int numReturned;
  fbConfigs = glXChooseFBConfig (display, DefaultScreen(display), AttributeList, &numReturned);
    // don't know why, but using x2go, the above fails, but when we get a list
  if (!fbConfigs)
  {
     fbConfigs = glXGetFBConfigs (display, DefaultScreen(display), &numReturned);
     printf("Using first available framebuffer from list of, %d.\n",numReturned);
  }
  else
     printf("Using requested framebuffer\n");
  fflush(stdout);
  if (numReturned == 0)
  {
    printf("The graphics required by this program are not supported\nby the current environment, such as over a network.\nExiting. . .");
    exit(1);
  }
  if (Debug){
    printf("%p %x %p\n",display, DefaultScreen(display), fbConfigs);
    fflush(stdout);
  }
  XVisualInfo *vInfo;
if(fbConfigs)
  vInfo = glXGetVisualFromFBConfig (display, fbConfigs[0]);
  static GLXContext context;
  if (context) glXDestroyContext(display, context);
  context = glXCreateNewContext (display, fbConfigs[0], GLX_RGBA_TYPE, NULL, True);
  XFree (fbConfigs);
  
  XSetWindowAttributes attributes = {
    .backing_store = Always, // not if you want Expose events
    .background_pixel = BlackPixel(display, screen),
    .border_pixel = BlackPixel(display, screen),
    .colormap = XCreateColormap (display, RootWindow (display, vInfo->screen), vInfo->visual, AllocNone),
    .event_mask = StructureNotifyMask
  };
  unsigned long valuemask = CWBackingStore | CWBackPixel | CWBorderPixel | CWColormap | CWEventMask;

  unsigned int border_width;
  w->win = XCreateWindow (display, RootWindow (display, screen),
                          x, y, w->width, w->height, border_width = 0,
                          vInfo->depth, InputOutput,
                          vInfo->visual, 
                          valuemask, &attributes);
  XFree (vInfo);
  XSelectInput (display, w->win, ButtonPressMask | ExposureMask | KeyPressMask | StructureNotifyMask);

  
  char *title = strndup (title_in, len);
  char *titlep = title;
  XTextProperty tprop;
  XStringListToTextProperty (&titlep, 1, &tprop);
  free (title);
  XSetWMName (display, w->win, &tprop);

  XSizeHints hints;
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
  hints.x = 100;
  hints.y = 100;
  //  printf ("x: %d, y: %d\n", hints.x, hints.y);
  hints.flags = USPosition | PMinSize | PMaxSize; // no redraw handler,
  hints.min_width = hints.max_width = w->width;   // so no resizing
  hints.min_height = hints.max_height = w->height;

  XSetNormalHints (display, w->win, &hints);

  /* keep window from being deleted when X is clicked */
  static Atom wm_protocols, wm_delete_window;
  wm_protocols = XInternAtom(display, "WM_PROTOCOLS", False);
  wm_delete_window = XInternAtom(display, "WM_DELETE_WINDOW", False);
  XChangeProperty(display, w->win, wm_protocols, XA_ATOM, 32,
                  PropModeAppend, (unsigned char * ) &wm_delete_window, 1);
  XMapWindow (display, w->win);

  XEvent event;
  Bool WaitForNotify (Display *display, XEvent *event, XPointer arg) {
    return (event->type == MapNotify) && (event->xmap.window == (Window) arg);
  }
  XIfEvent (display, &event, WaitForNotify, (XPointer) w->win);

  glXMakeCurrent (display, w->win, context);
  glClearColor (0, 0, 0, 1);
  glClear (GL_COLOR_BUFFER_BIT);
#if DBLBUF == 0
  glDrawBuffer (GL_FRONT);
#endif
  set_matrix ((long *)&w);         /* sets S and T */

  //  printf ("gl vendor: %s\n", glGetString (GL_VENDOR));
  //  printf ("gl renderer: %s\n", glGetString (GL_RENDERER));
  //  printf ("gl version: %s\n", glGetString (GL_VERSION));
  //  printf ("gl extensions: %s\n", glGetString (GL_EXTENSIONS));
//  printf ("glx extensions: %s\n", glXQueryExtensionsString (display, 0));
//  printf ("glx client vendor: %s\n", glXGetClientString (display, GLX_VENDOR));
//  printf ("glx client version: %s\n",  glXGetClientString (display, GLX_VERSION));
//  printf ("glx client extensions: %s\n",  glXGetClientString (display, GLX_EXTENSIONS));
//  printf ("glx server vendor: %s\n", glXQueryServerString (display, 0, GLX_VENDOR));
//  printf ("glx server version: %s\n",  glXQueryServerString (display, 0, GLX_VERSION));
//  printf ("glx server extensions: %s\n",  glXQueryServerString (display, 0, GLX_EXTENSIONS));

  //  printf ("gopen: %p %ld\n", w, (long)w);

  return (long)w;
}

long
gopen (int *width_p, int *height_p, int *x_p, int *y_p, char* title_in, int len)
{
  return gopen3d (width_p, height_p, x_p, y_p, title_in, len);
}
