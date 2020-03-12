#define _GNU_SOURCE
#include <string.h>
#include <math.h>
#include <fenv.h>
#include <float.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include "windata.h"
#include "util.h"

extern float matrix[16];

float wc_to_vdc_matrix[16];

static float
flt (double x)
{
  if (x > -FLT_MIN && x < FLT_MIN)
    return 0;
  return x;
}

static void
unproject (int winx, int winy, int winz, double *x, double *y, double *z)
{

  GLint viewport[4];
  glGetIntegerv (GL_VIEWPORT, viewport);

  GLdouble projection[16];
  glGetDoublev (GL_PROJECTION_MATRIX, projection);
  winy = viewport[3] - winy;
  //  glReadPixels (winx, winy, 1, 1, GL_DEPTH_COMPONENT, GL_INT, &winz);
  GLdouble modelview[16];
  glGetDoublev (GL_MODELVIEW_MATRIX, modelview);
  gluUnProject (winx, winy, winz, modelview, projection, viewport, x, y, z);
}

static double *
corners (void)
{
  static double c[12];
  GLint viewport[4];
  glGetIntegerv (GL_VIEWPORT, viewport);
  int llxp = viewport[0];
  int llyp = viewport[1];
  int urxp = viewport[0] + viewport[2];
  int uryp = viewport[1] + viewport[3];
  double x, y, z;
  unproject (llxp, llyp, 0, &x, &y, &z); printf ("ll 0: %g %g %g\n", x, y, z);
  unproject (llxp, llyp, 1, &x, &y, &z); printf ("ll 1: %g %g %g\n", x, y, z);
  unproject (urxp, uryp, 0, &x, &y, &z); printf ("ur 0: %g %g %g\n", x, y, z);
  unproject (urxp, uryp, 1, &x, &y, &z); printf ("ur 1: %g %g %g\n", x, y, z);
}

void
view_camera (long *fildes, camera_arg *camera)
{
  WinData *wd = (WinData *) *fildes;
  XYZ cam = {camera->camx, camera->camy, camera->camz};
  XYZ ref = {camera->refx, camera->refy, camera->refz};
  XYZ up = {camera->upx, camera->upy, camera->upz};
  double fov = camera->field_of_view / 180. * M_PI;
  double frontclip = camera->front;
  double backclip = camera->back;
  if (frontclip == backclip) {
    frontclip = 0;
    backclip = FLT_MAX;
  }

  XYZ look = sub (ref, cam);
  double looklen = len (look);
  XYZ looknorm = norm (look);
  XYZ upp = sub (up, smul (dot (up, look), look));
  XYZ rightnorm = norm (cross (up, look));
  double rightlen = looklen * tan (fov / 2);
  XYZ right = smul (rightlen, rightnorm);
  XYZ frontpos = add (ref, smul (frontclip, looknorm));
  XYZ backpos = add (ref, smul (backclip, looknorm));
  XYZ rightpos = add (frontpos, right);
  XYZ uppnorm = norm (upp);
  XYZ toppos = add (frontpos, smul (rightlen, uppnorm));

  double a = frontpos.x;
  double b = frontpos.y;
  double c = frontpos.z;
  double d = backpos.x;
  double e = backpos.y;
  double f = backpos.z;
  double g = rightpos.x;
  double h = rightpos.y;
  double i = rightpos.z;
  double j = toppos.x;
  double k = toppos.y;
  double l = toppos.z;

  double mind = fmin (fabs (wd->px2 - wd->px1), fabs (wd->py2 - wd->py1)) / 2;
  int sgn (double x) { return x < 0 ? -1 : (x > 0 ? 1 : 0); }

  double A = (wd->px1 + wd->px2) / 2;
  double B = (wd->py1 + wd->py2) / 2;            /* front */
  double C = wd->zmin;

  double D = A;
  double E = B;			                 /* back */
  double F = wd->zmax;

  double sign = 1;
  if (sgn (wd->px2 - wd->px1) < 0 || sgn (wd->py2 - wd->py1) < 0)
    sign = -1;

  //  double G = A + sgn (wd->px2 - wd->px1) * mind;
  double G = A + sign * mind;
  double H = B;			                 /* right */
  double I = C;

  double J = A;
  //  double K = B + sgn (wd->py2 - wd->py1) * mind; /* top */
  double K = B + sign * mind; /* top */
  double L = C;
  
  /*
    solve for transformation matrix with maxima
    MI:matrix([a,b,c,1],[d,e,f,1],[g,h,i,1],[j,k,l,1]);
    MT:matrix([m,n,o,0],[p,q,r,0],[s,t,u,0],[v,w,x,1]);
    MO:matrix([A,B,C,1],[D,E,F,1],[G,H,I,1],[J,K,L,1]);
    EQ:MI.MT-MO;
    solve(makelist(EQ[i][1],i,1,4), makelist(MT[i][1],i,1,4));
    solve(makelist(EQ[i][2],i,1,4), makelist(MT[i][2],i,1,4));
    solve(makelist(EQ[i][3],i,1,4), makelist(MT[i][3],i,1,4));
  */
  double m = ((b*(f*(J-G)-i*J+l*G+(i-l)*D)+e*(i*J-l*G)+c*(h*J+e*(G-J)-k*G+(k-h)*D)
               +f*(k*G-h*J)+(h*l-i*k)*D+(e*(l-i)-h*l+i*k+f*(h-k))*A)
              /(a*(e*(l-i)-h*l+i*k+f*(h-k))+d*(h*l-i*k)+b*(g*l+d*(i-l)+f*(j-g)-i*j)
                +e*(i*j-g*l)+c*(d*(k-h)-g*k+h*j+e*(g-j))+f*(g*k-h*j)));
  double p = -((a*(f*(J-G)-i*J+l*G+(i-l)*D)+d*(i*J-l*G)+c*(g*J+d*(G-J)-j*G+(j-g)*D)
                +f*(j*G-g*J)+(g*l-i*j)*D+(d*(l-i)-g*l+i*j+f*(g-j))*A)
               /(a*(e*(l-i)-h*l+i*k+f*(h-k))+d*(h*l-i*k)+b*(g*l+d*(i-l)+f*(j-g)-i*j)
                 +e*(i*j-g*l)+c*(d*(k-h)-g*k+h*j+e*(g-j))+f*(g*k-h*j)));
  double s = ((a*(e*(J-G)-h*J+k*G+(h-k)*D)+d*(h*J-k*G)+b*(g*J+d*(G-J)-j*G+(j-g)*D)
               +e*(j*G-g*J)+(g*k-h*j)*D+(d*(k-h)-g*k+h*j+e*(g-j))*A)
              /(a*(e*(l-i)-h*l+i*k+f*(h-k))+d*(h*l-i*k)+b*(g*l+d*(i-l)+f*(j-g)-i*j)
                +e*(i*j-g*l)+c*(d*(k-h)-g*k+h*j+e*(g-j))+f*(g*k-h*j)));
  double v = -((a*(e*(i*J-l*G)+f*(k*G-h*J)+(h*l-i*k)*D)+b*(d*(l*G-i*J)+f*(g*J-j*G)+(i*j-g*l)*D)
                +c*(d*(h*J-k*G)+e*(j*G-g*J)+(g*k-h*j)*D)+(d*(i*k-h*l)+e*(g*l-i*j)+f*(h*j-g*k))*A)
               /(a*(e*(l-i)-h*l+i*k+f*(h-k))+d*(h*l-i*k)+b*(g*l+d*(i-l)+f*(j-g)-i*j)
                 +e*(i*j-g*l)+c*(d*(k-h)-g*k+h*j+e*(g-j))+f*(g*k-h*j)));
  double n = ((b*(f*(K-H)-i*K+l*H+(i-l)*E)+e*(i*K-l*H)+c*(h*K+e*(H-K)-k*H+(k-h)*E)
               +f*(k*H-h*K)+(h*l-i*k)*E+(e*(l-i)-h*l+i*k+f*(h-k))*B)
              /(a*(e*(l-i)-h*l+i*k+f*(h-k))+d*(h*l-i*k)+b*(g*l+d*(i-l)+f*(j-g)-i*j)
                +e*(i*j-g*l)+c*(d*(k-h)-g*k+h*j+e*(g-j))+f*(g*k-h*j)));
  double q = -((a*(f*(K-H)-i*K+l*H+(i-l)*E)+d*(i*K-l*H)+c*(g*K+d*(H-K)-j*H+(j-g)*E)
                +f*(j*H-g*K)+(g*l-i*j)*E+(d*(l-i)-g*l+i*j+f*(g-j))*B)
               /(a*(e*(l-i)-h*l+i*k+f*(h-k))+d*(h*l-i*k)+b*(g*l+d*(i-l)+f*(j-g)-i*j)
                 +e*(i*j-g*l)+c*(d*(k-h)-g*k+h*j+e*(g-j))+f*(g*k-h*j)));
  double t = ((a*(e*(K-H)-h*K+k*H+(h-k)*E)+d*(h*K-k*H)+b*(g*K+d*(H-K)-j*H+(j-g)*E)
               +e*(j*H-g*K)+(g*k-h*j)*E+(d*(k-h)-g*k+h*j+e*(g-j))*B)
              /(a*(e*(l-i)-h*l+i*k+f*(h-k))+d*(h*l-i*k)+b*(g*l+d*(i-l)+f*(j-g)-i*j)
                +e*(i*j-g*l)+c*(d*(k-h)-g*k+h*j+e*(g-j))+f*(g*k-h*j)));
  double w = -((a*(e*(i*K-l*H)+f*(k*H-h*K)+(h*l-i*k)*E)+b*(d*(l*H-i*K)+f*(g*K-j*H)+(i*j-g*l)*E)
                +c*(d*(h*K-k*H)+e*(j*H-g*K)+(g*k-h*j)*E)+(d*(i*k-h*l)+e*(g*l-i*j)+f*(h*j-g*k))*B)
               /(a*(e*(l-i)-h*l+i*k+f*(h-k))+d*(h*l-i*k)+b*(g*l+d*(i-l)+f*(j-g)-i*j)
                 +e*(i*j-g*l)+c*(d*(k-h)-g*k+h*j+e*(g-j))+f*(g*k-h*j)));
  double o = ((b*(f*(L-I)-i*L+l*I+(i-l)*F)+e*(i*L-l*I)+c*(h*L+e*(I-L)-k*I+(k-h)*F)
               +f*(k*I-h*L)+(h*l-i*k)*F+(e*(l-i)-h*l+i*k+f*(h-k))*C)
              /(a*(e*(l-i)-h*l+i*k+f*(h-k))+d*(h*l-i*k)+b*(g*l+d*(i-l)+f*(j-g)-i*j)
                +e*(i*j-g*l)+c*(d*(k-h)-g*k+h*j+e*(g-j))+f*(g*k-h*j)));
  double r = -((a*(f*(L-I)-i*L+l*I+(i-l)*F)+d*(i*L-l*I)+c*(g*L+d*(I-L)-j*I+(j-g)*F)
                +f*(j*I-g*L)+(g*l-i*j)*F+(d*(l-i)-g*l+i*j+f*(g-j))*C)
               /(a*(e*(l-i)-h*l+i*k+f*(h-k))+d*(h*l-i*k)+b*(g*l+d*(i-l)+f*(j-g)-i*j)
                 +e*(i*j-g*l)+c*(d*(k-h)-g*k+h*j+e*(g-j))+f*(g*k-h*j)));
  double u = ((a*(e*(L-I)-h*L+k*I+(h-k)*F)+d*(h*L-k*I)+b*(g*L+d*(I-L)-j*I+(j-g)*F)
               +e*(j*I-g*L)+(g*k-h*j)*F+(d*(k-h)-g*k+h*j+e*(g-j))*C)
              /(a*(e*(l-i)-h*l+i*k+f*(h-k))+d*(h*l-i*k)+b*(g*l+d*(i-l)+f*(j-g)-i*j)
                +e*(i*j-g*l)+c*(d*(k-h)-g*k+h*j+e*(g-j))+f*(g*k-h*j)));
  double x = -((a*(e*(i*L-l*I)+f*(k*I-h*L)+(h*l-i*k)*F)+b*(d*(l*I-i*L)+f*(g*L-j*I)+(i*j-g*l)*F)
                +c*(d*(h*L-k*I)+e*(j*I-g*L)+(g*k-h*j)*F)+(d*(i*k-h*l)+e*(g*l-i*j)+f*(h*j-g*k))*C)
               /(a*(e*(l-i)-h*l+i*k+f*(h-k))+d*(h*l-i*k)+b*(g*l+d*(i-l)+f*(j-g)-i*j)
                 +e*(i*j-g*l)+c*(d*(k-h)-g*k+h*j+e*(g-j))+f*(g*k-h*j)));
  #include <stdio.h>
  //    MT:matrix([m,n,o,0],[p,q,r,0],[s,t,u,0],[v,w,x,1]);
  glLoadIdentity ();
  glTranslatef (wd->T[0], wd->T[1], wd->T[2]);
  glScalef (wd->S[0], wd->S[1], wd->S[2]);
  float MT[16] = {flt(m),flt(n),flt(o),0,
                  flt(p),flt(q),flt(r),0,
                  flt(s),flt(t),flt(u),0,
                  flt(v),flt(w),flt(x),1};
  memcpy (wc_to_vdc_matrix, MT, sizeof MT);
  glMultMatrixf (MT);

  //  corners ();
  
}
