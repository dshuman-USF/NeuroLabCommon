#include <GL/gl.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "windata.h"
#include "sbparam.h"

extern void print_mat (float *m, char *name);

int width[7][128] = {[1] = {[32 ... 127] = 64},
                     [4] = {[32] = 59,18,27,59,59,59,59,18,23,23,59,59,18,49,18,33,54,51,59,59,59,59,59,59,59,59,18,18,59,59,59,49,54,59,59,59,59,59,59,59,59,59,51,59,59,59,59,59,59,59,59,59,59,59,59,59,59,59,59,23,33,23,33,59,18,50,50,51,51,53,51,51,51,39,34,51,39,51,51,52,51,52,41,51,51,51,51,51,51,51,51,33,13,33,59,0},
                     [6] = {[32] = 63,21,37,68,57,75,60,23,29,29,63,63,21,53,21,38,59,35,55,58,57,56,56,53,59,56,21,21,63,63,63,48,67,65,60,66,61,57,55,68,61,21,48,64,52,73,60,71,58,71,61,61,59,60,65,87,63,65,60,29,38,29,37,87,23,52,52,53,52,53,34,52,51,20,27,52,20,70,48,54,52,52,35,51,34,48,51,73,54,54,49,38,19,38,54,0},
};

int   tax[7][128] = {[1] = {[33 ... 126] = 48},
                     [4] = {[33] = 5,14,46,46,46,46,5,10,10,46,46,5,36,5,20,41,38,46,46,46,46,46,46,46,46,5,5,46,46,46,36,41,46,46,46,46,46,46,46,46,46,38,46,46,46,46,46,46,46,46,46,46,46,46,46,46,46,46,10,20,10,20,46,5,37,37,38,38,40,38,38,38,26,21,38,26,38,38,39,38,39,28,38,38,38,38,38,38,38,38,20,0,20,46},
                     [6] = {[33] = 8,24,55,44,62,47,10,16,16,50,50,8,40,8,25,46,22,42,45,44,43,43,40,46,43,8,8,50,50,50,35,54,52,47,53,48,44,42,55,48,8,35,51,39,60,47,58,45,58,48,48,46,47,52,74,50,52,47,16,25,16,24,74,10,39,39,40,39,40,21,39,38,7,14,39,7,57,35,41,39,39,22,38,21,35,38,60,41,41,36,25,6,25,41},
};

typedef struct
{
  int cell_width;
  int base;
  int right;
} FontInfo;

FontInfo font_info[7] = {[1] = {64, 32, 48},
                         [4] = {59, 30, 46},
                         [6] = {63, 30, 50}};


extern void (*glyph_func[5][127])(void);

static void
renderfont (WinData *w, char *string)
{
  int fi = w->font_index;
  if (fi > 4) {
    ftglRenderFont (w->font[fi], string, FTGL_RENDER_ALL);
    return;
  }
  glPushMatrix ();
  float dx = fi == 1 ? -8 : 0;
  float dy = fi == 1 ? -32 : -30;
  glTranslatef (dx, dy, 0);
  for (int i = 0; string[i]; i++) {
    int c = string[i];
    if (c > 32)
      (*glyph_func[fi][string[i]])();
    glTranslatef (width[fi][c], 0, 0);
  }
  glPopMatrix ();
}

void
text3d (long *fildes, float *x_in, float *y_in, float *z_in, char *string_in, int *xform, int *more, int slen)
{
  WinData *w = (WinData *) *fildes;

  if (0) {
    glColor3f (w->text_r, w->text_g, w->text_b);
    ftglSetFontFaceSize (w->font[w->font_index], 1, 0);
    printf ("string: \"%s\"\n", w->string);
    printf ("font: %p\n", w->font[w->font_index]);
    ftglRenderFont (w->font[w->font_index], "B", FTGL_RENDER_ALL);
    return;
  }

  float x, y, z;
  if (w->stringlen == 0) {
    w->string = realloc (w->string, slen + 1);
    w->string[0] = 0;
    x = w->text_x = *x_in;
    y = w->text_y = *y_in;
    z = w->text_z = *z_in;
  }
  else {
    w->string = realloc (w->string, w->stringlen + slen + 1);
    x = w->text_x;
    y = w->text_y;
    z = w->text_z;
  }
  memcpy (w->string + w->stringlen, string_in, slen);
  w->string[w->stringlen += slen] = 0;
  if (0)
    while (w->stringlen > 0 && (w->string[w->stringlen - 1] == ' ' || w->string[w->stringlen - 1] == 0))
      w->string[--w->stringlen] = 0;
  int sl = strlen (w->string);
  if (sl < w->stringlen)
    w->stringlen = sl;
  
  if (*more)
    return;

  if (0) {
    glColor3f (w->text_r, w->text_g, w->text_b);
    ftglSetFontFaceSize (w->font[w->font_index], 1, 0);
    printf ("string: \"%s\"\n", w->string);
    printf ("font: %p\n", w->font[w->font_index]);
    ftglRenderFont (w->font[w->font_index], w->string, FTGL_RENDER_ALL);
    /*
      FTGLfont *font = ftglCreatePolygonFont ("/raid/docs/russtemp/fonts/hp1.pfb");
      ftglSetFontFaceSize (font, 1, 0);
      ftglRenderFont (font, "B", FTGL_RENDER_ALL);
    */
    return;
  }

  glPushMatrix ();
  if (*xform == VDC_TEXT) {
    //    printf ("VDC TEXT %d\n", *xform);
    glLoadIdentity ();
    glTranslatef (w->T[0], w->T[1], w->T[2]);
    glScalef (w->S[0], w->S[1], w->S[2]);
  }

  /* position */
  glTranslatef (x, y, z);

  /* orientation */
  XYZ in = norm (cross (w->base, w->up));
  float M[16] = {w->base.x, w->base.y, w->base.z, 0,
                   w->up.x,   w->up.y,   w->up.z, 0,
                      in.x,      in.y,      in.z, 0,
                         0,         0,         0, 1};
  //  print_mat (M, "Orientation");

  glMultMatrixf (M);

  /* height */
  int base = font_info[w->font_index].base;
  double f[] = {[TA_BOTTOM] = -base / 120.,
                [TA_BASE]   =  0,
                [TA_HALF]   = (60 - base) / 120.,
                [TA_TOP]    = (120 - base) / 120.};
  double frac = f[w->tay];
  double char_pixel_height = 0;
  enum {VPX, VPY, VPW, VPH};
  float vp[4];
  glGetFloatv (GL_VIEWPORT, vp);
  XYZ pb = {-w->char_height, 0, 0};
  float m[16];
  glGetFloatv (GL_MODELVIEW_MATRIX, m);
  pb = vmmul (pb, m);
  glGetFloatv (GL_PROJECTION_MATRIX, m);
  pb = vmmul (pb, m);
  pb.x *= vp[VPW] / 2;
  pb.y *= vp[VPH] / 2;
  char_pixel_height = sqrt (pow (pb.x, 2) + pow (pb.y, 2));
  int face_size = nearbyint (char_pixel_height / w->resolution * 72);
  if (w->font_index <= 4)
    face_size = 120;
  else
    ftglSetFontFaceSize (w->font[w->font_index], face_size, w->resolution);
  //  printf ("char_pixel_height: %g\n", char_pixel_height);
  //  printf ("face_size: %d\n", face_size);
  double hu = w->char_height / sin (acos (dot (w->up, w->base)));
  double factor = 1;
  if (w->character_width_valid)
    factor = w->character_width / (w->char_height * font_info[w->font_index].cell_width / 120);
  else
    factor = w->character_expansion_factor;
  glScaled (factor * w->char_height / face_size, hu / face_size, 1);
  
  /* alignment */
  double dy = - frac * face_size;
  double dx = 0;
  if (w->tax != TA_LEFT) {
    int txtwidth = 0;
    int len = strlen (w->string);
    for (int i = 0; i + 1 < len; i++)
      txtwidth += width[w->font_index][w->string[i]];
    txtwidth += tax[w->font_index][w->string[len - 1]];

    //    printf ("txtwidth: %d, B width: %d\n", txtwidth, width[w->font_index]['B']);
    dx = -txtwidth / (1. + (w->tax == TA_CENTER)) / 120 * face_size;
  }
  //  printf ("translate %g\n", dx);
  glTranslated (dx, dy, 0);
  glColor3f (w->text_r, w->text_g, w->text_b);
  
  renderfont (w, w->string);
              
  glPopMatrix ();
  w->string[0] = 0;
  w->stringlen = 0;
}

void
text2d (long *fildes, float *x_in, float *y_in, char *string_in, int *xform, int *more, int slen)
{
  float z_in = 0;
  text3d (fildes, x_in, y_in, &z_in, string_in, xform, more, slen);
}

void
append_text (long *fildes, char *string_in, int *xform, int *more, int slen)
{
  WinData *w = (WinData *) *fildes;
  text3d (fildes, &w->pen_x, &w->pen_y, &w->pen_z, string_in, xform, more, slen);
}
