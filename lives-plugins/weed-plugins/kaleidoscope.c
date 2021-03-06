// kaleidoscope.c
// weed plugin
// (c) G. Finch (salsaman) 2013
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

///////////////////////////////////////////////////////////////////

static int package_version = 1; // version of this package

//////////////////////////////////////////////////////////////////

#define NEED_PALETTE_UTILS

#ifndef NEED_LOCAL_WEED_PLUGIN
#include <weed/weed-plugin.h>
#include <weed/weed-plugin-utils.h> // optional
#else
#include "../../libweed/weed-plugin.h"
#include "../../libweed/weed-plugin-utils.h" // optional
#endif

#include "weed-plugin-utils.c" // optional

/////////////////////////////////////////////////////////////

#include <math.h>

#define FIVE_PI3 5.23598775598f
#define FOUR_PI3 4.18879020479f

#define THREE_PI2 4.71238898038f

#define TWO_PI 6.28318530718f
#define TWO_PI3 2.09439510239f

#define ONE_PI2 1.57079632679f
#define ONE_PI3 1.0471975512f

#define RT3  1.73205080757f //sqrt(3)
#define RT32 0.86602540378f //sqrt(3)/2

#define RT322 0.43301270189f

static float calc_angle(float y, float x) {
  if (x > 0.) {
    if (y >= 0.) return atanf(y / x);
    return TWO_PI + atanf(y / x);
  }
  if (x < -0.) {
    return atanf(y / x) + M_PI;
  }
  if (y > 0.) return ONE_PI2;
  return THREE_PI2;
}

static float calc_dist(float x, float y) {
  return sqrtf(x * x + y * y);
}

typedef struct {
  float angle;
  weed_timecode_t old_tc;
  int revrot;
  int owidth;
  int oheight;
} sdata;

static void calc_center(float side, float j, float i, float *x, float *y) {
  // find nearest hex center
  int gridx, gridy;

  float secx, secy;

  float sidex = side * RT3; // 2 * side * cos(30)
  float sidey = side * 1.5; // side + sin(30)

  float hsidex = sidex / 2., hsidey = sidey / 2.;

  i -= side / 5.3;

  if (i > 0.) i += hsidey;
  else i -= hsidey;
  if (j > 0.) j += hsidex;
  else j -= hsidex;

  // find the square first
  gridy = i / sidey;
  gridx = j / sidex;

  // center
  *y = gridy * sidey;
  *x = gridx * sidex;

  secy = i - *y;
  secx = j - *x;

  if (secy < 0.) secy += sidey;
  if (secx < 0.) secx += sidex;

  if (!(gridy & 1)) {
    // even row (inverted Y)
    if (secy > (sidey - (hsidex - secx)*RT322)) {
      *y += sidey;
      *x -= hsidex;
    } else if (secy > sidey - (secx - hsidex)*RT322) {
      *y += sidey;
      *x += hsidex;
    }
  } else {
    // odd row, center is left or right (Y)
    if (secx <= hsidex) {
      if (secy < (sidey - secx * RT322)) {
        *x -= hsidex;
      } else *y += sidey;
    } else {
      if (secy < sidey - (sidex - secx)*RT322) {
        *x += hsidex;
      } else *y += sidey;
    }
  }
}


static void rotate(float r, float theta, float angle, float *x, float *y) {
  theta += angle;
  if (theta < 0.) theta += TWO_PI;
  else if (theta >= TWO_PI) theta -= TWO_PI;
  *x = r * cosf(theta);
  *y = r * sinf(theta);
}


static int put_pixel(void *src, void *dst, int psize, float angle, float theta, float r, int irowstride, int hheight,
                     int hwidth) {
  // dest point is at i,j; r tells us which point to copy, and theta related to angle gives us the transform

  // return 0 if src is oob

  float adif = theta - angle;
  float stheta;

  int sx, sy;

  if (adif < 0.) adif += TWO_PI;
  else if (adif >= TWO_PI) adif -= TWO_PI;

  theta -= angle;

  if (theta < 0.) theta += TWO_PI;
  else if (theta > TWO_PI) theta -= TWO_PI;

  if (adif < ONE_PI3) {
    stheta = theta;
  }

  else if (adif < TWO_PI3) {
    // get coords of src point
    stheta = TWO_PI3 - theta;
  }

  else if (adif < M_PI) {
    // get coords of src point
    stheta = theta - TWO_PI3;
  }

  else if (adif < FOUR_PI3) {
    // get coords of src point
    stheta = FOUR_PI3 - theta;
  }

  else if (adif < FIVE_PI3) {
    // get coords of src point
    stheta = theta - FOUR_PI3;
  } else {
    // get coords of src point
    stheta = TWO_PI - theta;
  }

  stheta += angle;

  sx = r * cosf(stheta) + .5;
  sy = r * sinf(stheta) + .5;

  if (sy < -hheight || sy >= hheight || sx < -hwidth || sx >= hwidth) {
    return 0;
  }

  weed_memcpy(dst, src - sy * irowstride + sx * psize, psize);
  return 1;
}


static weed_error_t kal_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  weed_plant_t *in_channel = weed_get_plantptr_value(inst, WEED_LEAF_IN_CHANNELS, NULL),
                *out_channel = weed_get_plantptr_value(inst, WEED_LEAF_OUT_CHANNELS, NULL);
  weed_plant_t **in_params = weed_get_plantptr_array(inst, WEED_LEAF_IN_PARAMETERS, NULL);
  unsigned char *src = weed_get_voidptr_value(in_channel, WEED_LEAF_PIXEL_DATA, NULL);
  unsigned char *dst = weed_get_voidptr_value(out_channel, WEED_LEAF_PIXEL_DATA, NULL);

  sdata *sdata = weed_get_voidptr_value(inst, "plugin_internal", NULL);

  float theta, r, xangle;

  float x, y, a, b;

  float side, fi, fj;

  float anglerot = 0.;
  double dtime, sfac, angleoffs;

  int width = weed_get_int_value(in_channel, WEED_LEAF_WIDTH, NULL), hwidth = width >> 1;
  int height = weed_get_int_value(in_channel, WEED_LEAF_HEIGHT, NULL), hheight = height >> 1;
  int palette = weed_get_int_value(in_channel, WEED_LEAF_CURRENT_PALETTE, NULL);
  int irowstride = weed_get_int_value(in_channel, WEED_LEAF_ROWSTRIDES, NULL);
  int orowstride = weed_get_int_value(out_channel, WEED_LEAF_ROWSTRIDES, NULL);
  int psize = 4;

  int sizerev;

  int start, end;

  int upd = 1;

  register int i, j;

  if (width < height) side = width / 2. / RT32;
  else side = height / 2.;
  sfac = log(weed_get_double_value(in_params[0], WEED_LEAF_VALUE, NULL)) / 2.;

  angleoffs = weed_get_double_value(in_params[1], WEED_LEAF_VALUE, NULL);

  if (sdata->old_tc != 0 && timestamp > sdata->old_tc) {
    anglerot = (float)weed_get_double_value(in_params[2], WEED_LEAF_VALUE, NULL);
    dtime = (double)(timestamp - sdata->old_tc) / 100000000.;
    anglerot *= (float)dtime;
    while (anglerot >= TWO_PI) anglerot -= TWO_PI;
  }

  if (weed_get_boolean_value(in_params[4], WEED_LEAF_VALUE, NULL) == WEED_TRUE) anglerot = -anglerot;

  sizerev = weed_get_boolean_value(in_params[5], WEED_LEAF_VALUE, NULL);

  weed_free(in_params);

  xangle = sdata->angle + (float)angleoffs / 360.*TWO_PI;
  if (xangle >= TWO_PI) xangle -= TWO_PI;

  if (sdata->owidth != width || sdata->oheight != height) {
    if (sizerev && sdata->owidth != 0 && sdata->oheight != 0) sdata->revrot = 1 - sdata->revrot;
    sdata->owidth = width;
    sdata->oheight = height;
  }

  if (sdata->revrot) anglerot = -anglerot;

  side *= (float)sfac;

  if (palette == WEED_PALETTE_RGB24 || palette == WEED_PALETTE_BGR24) psize = 3;

  src += hheight * irowstride + hwidth * psize;

  start = hheight;
  end = -hheight;

  // new threading arch
  if (weed_plant_has_leaf(out_channel, WEED_LEAF_OFFSET)) {
    int offset = weed_get_int_value(out_channel, WEED_LEAF_OFFSET, NULL);
    int dheight = weed_get_int_value(out_channel, WEED_LEAF_HEIGHT, NULL);

    if (offset > 0) upd = 0;

    start -= offset;
    dst += offset * orowstride;
    end = start - dheight;
  }

  orowstride -= psize * (hwidth << 1);

  for (i = start; i > end; i--) {
    for (j = -hwidth; j < hwidth; j++) {
      // rotate point to line up with hex grid
      theta = calc_angle((fi = (float)i), (fj = (float)j)); // get angle of this point from origin
      r = calc_dist(fi, fj); // get dist of point from origin
      rotate(r, theta, -xangle + ONE_PI2, &a, &b); // since our central hex has rotated by xangle, so has the hex grid - so compensate

      // find nearest hex center and angle to it
      calc_center(side, a, b, &x, &y);

      // rotate hex center about itself
      theta = calc_angle(y, x);
      r = calc_dist(x, y);
      rotate(r, theta, xangle - ONE_PI2, &a, &b);

      theta = calc_angle(fi - b, fj - a);
      r = calc_dist(b - fi, a - fj);

      if (r < 10.) r = 10.;

      if (!put_pixel(src, dst, psize, xangle, theta, r, irowstride, hheight, hwidth)) {
        if (palette == WEED_PALETTE_RGB24 || palette == WEED_PALETTE_BGR24) {
          weed_memset(dst, 0, 3);
        } else if (palette == WEED_PALETTE_RGBA32 || palette == WEED_PALETTE_BGRA32) {
          weed_memset(dst, 0, 3);
          dst[3] = 255;
        } else if (palette == WEED_PALETTE_ARGB32) {
          weed_memset(dst + 1, 0, 3);
          dst[0] = 255;
        }
      }

      dst += psize;
    }
    dst += orowstride;
  }

  if (upd) {
    sdata->angle += anglerot * TWO_PI;
    if (sdata->angle >= TWO_PI) sdata->angle -= TWO_PI;
    else if (sdata->angle < 0.) sdata->angle += TWO_PI;
    sdata->old_tc = timestamp;
  }

  return WEED_SUCCESS;
}


static weed_error_t kal_init(weed_plant_t *inst) {
  sdata *sd = (sdata *)weed_malloc(sizeof(sdata));
  if (sd == NULL) return WEED_ERROR_MEMORY_ALLOCATION;

  sd->angle = 0.;
  sd->old_tc = 0;
  sd->revrot = 0;
  sd->owidth = sd->oheight = 0;

  weed_set_voidptr_value(inst, "plugin_internal", sd);

  return WEED_SUCCESS;
}


static weed_error_t kal_deinit(weed_plant_t *inst) {
  sdata *sd = weed_get_voidptr_value(inst, "plugin_internal", NULL);
  if (sd) {
    weed_free(sd);
    weed_set_voidptr_value(inst, "plugin_internal", NULL);
  }
  return WEED_SUCCESS;
}


WEED_SETUP_START(200, 200) {
  int palette_list[] = ALL_RGB_PALETTES;

  weed_plant_t *in_chantmpls[] = {weed_channel_template_init("in channel 0", 0), NULL};
  weed_plant_t *out_chantmpls[] = {weed_channel_template_init("out channel 0", 0), NULL};
  weed_plant_t *in_params[] = {weed_float_init("szln", "_Size (log)", 5.62, 2., 10.),
                               weed_float_init(WEED_LEAF_OFFSET, "_Offset angle", 0., 0., 359.),
                               weed_float_init("rotsec", "_Rotations per second", 0.2, 0., 4.),
                               weed_radio_init("acw", "_Anti-clockwise", WEED_TRUE, 1),
                               weed_radio_init("cw", "_Clockwise", WEED_FALSE, 1),
                               weed_switch_init("szc", "_Switch direction on frame size change", WEED_FALSE),
                               NULL
                              };

  weed_plant_t *filter_class = weed_filter_class_init("kaleidoscope", "salsaman", 1, WEED_FILTER_HINT_MAY_THREAD, palette_list,
                               kal_init, kal_process, kal_deinit, in_chantmpls, out_chantmpls, in_params, NULL);

  weed_plant_t *gui = weed_paramtmpl_get_gui(in_params[2]);
  weed_set_double_value(gui, WEED_LEAF_STEP_SIZE, .1);

  gui = weed_paramtmpl_get_gui(in_params[1]);
  weed_set_boolean_value(gui, WEED_LEAF_WRAP, WEED_TRUE);

  gui = weed_paramtmpl_get_gui(in_params[0]);
  weed_set_double_value(gui, WEED_LEAF_STEP_SIZE, .1);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);
  weed_set_int_value(plugin_info, WEED_LEAF_VERSION, package_version);
}
WEED_SETUP_END

