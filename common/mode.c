#include <curses.h>
#include <term.h>

// THIS SUBROUTINE WILL ERASE THE SCREEN AND SEND THE CURSOR TO THE 
// HOME POSITION (UPPER LEFT CORNER OF THE SCREEN)

void
mode (int *ivt)
{
  static int setup_done;
  static char *clear;
  if (!setup_done) {
    setupterm(0, 1, 0);
    clear = tigetstr ("clear");
    //    printf ("clear = %p\n", clear);
  }
//  printf ("clear");
//  for (int i = 0; clear && clear[i]; i++)
//    printf (" %d", clear[i]);
//  printf ("\n");
  
  putp (clear);
  
}
