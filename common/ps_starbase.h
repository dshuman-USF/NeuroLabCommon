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


void ps_fopen (char *filename);

void ps_fclose (void);
 
void ps_geometry (float *llx, float *lly, float *urx, float *ury);

void ps_vdc_extent (float *xminp, float *yminp, float *zminp, float *xmaxp, float *ymaxp, float *zmaxp);

void ps_mapping_mode (int *modep);

void ps_view_port (float *x1p, float *y1p, float *x2p, float *y2p);

void ps_view_window (float *x1p, float *y1p, float *x2p, float *y2p);

void ps_character_height (float *heightp);

void ps_character_width (float *widthp);

void ps_line_type (int *stylep);

void ps_draw2d (float *xp, float *yp);

void ps_rectangle (float *x1p, float *y1p, float *x2p, float *y2p);

void ps_move2d (float *xp, float *yp);

void ps_text2d (float *xp, float *yp, char *string, int *xformp, int slen);

void ps_append_text (char *string, int *xformp);

void ps_direct (char *string);

void ps_text_alignment (int *tax_in, int *tay_in, int *h, int *v);

void ps_text_orientation2d (float *up_xp, float *up_yp, float *base_xp, float *base_yp);

void ps_clip_indicator (int *clip_level);

void ps_text_path (int *path);

void ps_text_line_path (int *path);

void ps_line_color (float *red, float *green, float *blue);

void ps_text_color (float *red, float *green, float *blue);

void ps_fill_color (float *red, float *green, float *blue);

void ps_interior_style (int *style, int *edged);

void ps_perimeter_color (float *red, float *green, float *blue);

void ps_color_set (int *val);


#define TA_LEFT 0
#define TA_RIGHT 2
#define TA_NORMAL_HORIZONTAL 4
#define TA_TOP 0
#define TA_BOTTOM 4
#define TA_NORMAL_VERTICAL 6
