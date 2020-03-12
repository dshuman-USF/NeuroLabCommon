#include <stdio.h>
#include <X11/Xlib.h>

void screensize (int *widthp, int *heightp, int *status)
{
  Display *display;
  Window root;
  int x, y;
  unsigned int width, height;
  unsigned int border_width;
  unsigned int depth;

  if ((display = XOpenDisplay (0)) == 0) {
    *status = 0;
    return;
  }
  
  XGetGeometry (display, DefaultRootWindow (display), &root, &x, &y, &width, &height, &border_width, &depth);

  if (width >= height * 8 / 3)
    width /= 2;

  *widthp = width;
  *heightp = height;
  *status = 1;
  return;
}
