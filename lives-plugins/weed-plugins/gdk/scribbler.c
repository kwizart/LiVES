// scribbler.c
// weed plugin
// (c) A. Penkov (salsaman) 2010 - 2019
// cloned and modified from livetext.c (author G. Finch aka salsaman)
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

///////////////////////////////////////////////////////////////////

static int package_version = 2; // version of this package

//////////////////////////////////////////////////////////////////

#define NEED_PALETTE_CONVERSIONS

#ifndef NEED_LOCAL_WEED_PLUGIN
#include <weed/weed-plugin.h>
#include <weed/weed-plugin-utils.h> // optional
#else
#include "../../../libweed/weed-plugin.h"
#include "../../../libweed/weed-plugin-utils.h" // optional
#endif

#include "../weed-plugin-utils.c" // optional

/////////////////////////////////////////////////////////////

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <pango/pangocairo.h>
#include <gdk/gdk.h>

// defines for configure dialog elements
enum DlgControls {
  P_TEXT = 0,
  P_MODE,
  P_FONT,
  P_FOREGROUND,
  P_BACKGROUND,
  P_FGALPHA,
  P_BGALPHA,
  P_FONTSIZE,
  P_CENTER,
  P_RISE,
  P_TOP,
  P_END
};

typedef struct {
  int red;
  int green;
  int blue;
} rgb_t;

/////////////////////////////////////////////

static cairo_t *channel_to_cairo(weed_plant_t *channel) {
  // convert a weed channel to cairo
  // the channel shares pixel_data with cairo
  // so it should be copied before the cairo is destroyed

  // WEED_LEAF_WIDTH,WEED_LEAF_ROWSTRIDES and WEED_LEAF_CURRENT_PALETTE of channel may all change

  int irowstride, orowstride;
  int width, widthx;
  int height;
  int pal;
  int error;

  register int i;

  guchar *src, *dst, *pixel_data;

  cairo_surface_t *surf;
  cairo_t *cairo;
  cairo_format_t cform = CAIRO_FORMAT_ARGB32;

  width = weed_get_int_value(channel, WEED_LEAF_WIDTH, &error);
  height = weed_get_int_value(channel, WEED_LEAF_HEIGHT, &error);
  pal = weed_get_int_value(channel, WEED_LEAF_CURRENT_PALETTE, &error);
  irowstride = weed_get_int_value(channel, WEED_LEAF_ROWSTRIDES, &error);

  widthx = width * 4;

  orowstride = cairo_format_stride_for_width(cform, width);

  src = (guchar *)weed_get_voidptr_value(channel, WEED_LEAF_PIXEL_DATA, &error);

  pixel_data = (guchar *)weed_malloc(height * orowstride);

  if (pixel_data == NULL) return NULL;

  if (irowstride == orowstride) {
    weed_memcpy((void *)pixel_data, (void *)src, irowstride * height);
  } else {
    dst = pixel_data;
    for (i = 0; i < height; i++) {
      weed_memcpy((void *)dst, (void *)src, widthx);
      dst += orowstride;
      src += irowstride;
    }
  }

  // pre-multiply alpha for cairo
  if (weed_get_boolean_value(channel, WEED_LEAF_ALPHA_PREMULTIPLIED, NULL) == WEED_FALSE)
    alpha_premult(pixel_data, widthx, height, orowstride, pal, WEED_FALSE);

  surf = cairo_image_surface_create_for_data(pixel_data,
         cform,
         width, height,
         orowstride);

  if (surf == NULL) {
    weed_free(pixel_data);
    return NULL;
  }

  cairo = cairo_create(surf);
  return cairo;
}


static void cairo_to_channel(cairo_t *cairo, weed_plant_t *channel) {
  // updates a weed_channel from a cairo_t
  cairo_surface_t *surface = cairo_get_target(cairo);
  guchar *osrc, *src, *dst, *pixel_data = (guchar *)weed_get_voidptr_value(channel, WEED_LEAF_PIXEL_DATA, NULL);

  int height = weed_get_int_value(channel, WEED_LEAF_HEIGHT, NULL), cheight;
  int irowstride, orowstride = weed_get_int_value(channel, WEED_LEAF_ROWSTRIDES, NULL);
  int width = weed_get_int_value(channel, WEED_LEAF_WIDTH, NULL), widthx = width * 4;

  register int i;

  // flush to ensure all writing to the image was done
  cairo_surface_flush(surface);

  osrc = src = cairo_image_surface_get_data(surface);
  irowstride = cairo_image_surface_get_stride(surface);
  cheight = cairo_image_surface_get_height(surface);

  if (cheight < height) height = cheight;

  if (irowstride == orowstride) {
    weed_memcpy((void *)pixel_data, (void *)src, irowstride * height);
  } else {
    dst = pixel_data;
    for (i = 0; i < height; i++) {
      weed_memcpy((void *)dst, (void *)src, widthx);
      dst += orowstride;
      src += irowstride;
    }
  }

  if (weed_get_boolean_value(channel, WEED_LEAF_ALPHA_PREMULTIPLIED, NULL) == WEED_FALSE) {
    int pal = weed_channel_get_palette(channel);
    // un-premultiply the alpha
    alpha_premult(pixel_data, widthx, height, orowstride, pal, TRUE);
  }
  cairo_surface_flush(surface);
  weed_free(osrc);
  cairo_surface_destroy(surface);
}


static const char **fonts_available = NULL;
static int num_fonts_available = 0;

static weed_error_t scribbler_init(weed_plant_t *inst) {
  weed_plant_t **in_params = weed_get_in_params(inst, NULL);
  weed_plant_t *pgui;
  int mode = weed_param_get_value_int(in_params[P_MODE]);

  pgui = weed_param_get_gui(in_params[P_BGALPHA]);
  if (mode == 0) weed_set_boolean_value(pgui, WEED_LEAF_HIDDEN, WEED_TRUE);
  else weed_set_boolean_value(pgui, WEED_LEAF_HIDDEN, WEED_FALSE);

  pgui = weed_param_get_gui(in_params[P_BACKGROUND]);
  if (mode == 0) weed_set_boolean_value(pgui, WEED_LEAF_HIDDEN, WEED_TRUE);
  else weed_set_boolean_value(pgui, WEED_LEAF_HIDDEN, WEED_FALSE);

  pgui = weed_param_get_gui(in_params[P_FGALPHA]);
  if (mode == 2) weed_set_boolean_value(pgui, WEED_LEAF_HIDDEN, WEED_TRUE);
  else weed_set_boolean_value(pgui, WEED_LEAF_HIDDEN, WEED_FALSE);

  pgui = weed_param_get_gui(in_params[P_FOREGROUND]);
  if (mode == 2) weed_set_boolean_value(pgui, WEED_LEAF_HIDDEN, WEED_TRUE);
  else weed_set_boolean_value(pgui, WEED_LEAF_HIDDEN, WEED_FALSE);

  weed_free(in_params);

  return WEED_SUCCESS;
}


static void getxypos(PangoLayout *layout, double *px, double *py, int width, int height, int cent, double *pw, double *ph) {
  int w_, h_;
  double d;
  pango_layout_get_size(layout, &w_, &h_);
  if (pw)
    *pw = ((double)w_) / PANGO_SCALE;
  if (ph)
    *ph = ((double)h_) / PANGO_SCALE;

  if (cent) {
    d = ((double)w_) / PANGO_SCALE;
    d /= 2.0;
    d = (width >> 1) - d;
  } else
    d = 0.0;
  if (px)
    *px = d;

  d = ((double)h_) / PANGO_SCALE;
  d = height - d;
  if (py)
    *py = d;
}


static void fill_bckg(cairo_t *cr, double x, double y, double dx, double dy) {
  cairo_rectangle(cr, x, y, dx, dy);
  cairo_fill(cr);
}

//
//
// now text is drawn with pixbuf/pango
//
//

static weed_error_t scribbler_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  weed_plant_t **in_params = weed_get_plantptr_array(inst, WEED_LEAF_IN_PARAMETERS, NULL);
  weed_plant_t *out_channel = weed_get_plantptr_value(inst, WEED_LEAF_OUT_CHANNELS, NULL);
  weed_plant_t *in_channel = NULL;

  rgb_t *fg, *bg;

  cairo_t *cairo;

  double f_alpha, b_alpha;
  double dwidth, dheight;
  double font_size, top;

  char *text;

  int cent, rise;
  //int alpha_threshold = 0;
  int fontnum;
  int mode;

  int width = weed_get_int_value(out_channel, WEED_LEAF_WIDTH, NULL);
  int height = weed_get_int_value(out_channel, WEED_LEAF_HEIGHT, NULL);

  if (weed_plant_has_leaf(inst, WEED_LEAF_IN_CHANNELS)) {
    in_channel = weed_get_plantptr_value(inst, WEED_LEAF_IN_CHANNELS, NULL);
  }

  text = weed_get_string_value(in_params[P_TEXT], WEED_LEAF_VALUE, NULL);
  mode = weed_get_int_value(in_params[P_MODE], WEED_LEAF_VALUE, NULL);
  fontnum = weed_get_int_value(in_params[P_FONT], WEED_LEAF_VALUE, NULL);
  fg = (rgb_t *)weed_get_int_array(in_params[P_FOREGROUND], WEED_LEAF_VALUE, NULL);
  bg = (rgb_t *)weed_get_int_array(in_params[P_BACKGROUND], WEED_LEAF_VALUE, NULL);

  f_alpha = weed_get_double_value(in_params[P_FGALPHA], WEED_LEAF_VALUE, NULL);
  b_alpha = weed_get_double_value(in_params[P_BGALPHA], WEED_LEAF_VALUE, NULL);
  font_size = weed_get_double_value(in_params[P_FONTSIZE], WEED_LEAF_VALUE, NULL);

  cent = weed_get_boolean_value(in_params[P_CENTER], WEED_LEAF_VALUE, NULL);
  rise = weed_get_boolean_value(in_params[P_RISE], WEED_LEAF_VALUE, NULL);
  top = weed_get_double_value(in_params[P_TOP], WEED_LEAF_VALUE, NULL);

  weed_free(in_params); // must weed free because we got an array

  // THINGS TO TO WITH TEXTS AND PANGO
  if ((!in_channel) || (in_channel == out_channel))
    cairo = channel_to_cairo(out_channel);
  else
    cairo = channel_to_cairo(in_channel);

  if (cairo) {
    if (text && strlen(text)) {
      // do cairo and pango things
      PangoLayout *layout = pango_cairo_create_layout(cairo);
      if (layout) {
        PangoFontDescription *font;
        double x_pos, y_pos;
        double x_text, y_text;

        font = pango_font_description_new();
        if ((num_fonts_available) && (fontnum >= 0) && (fontnum < num_fonts_available) && (fonts_available[fontnum]))
          pango_font_description_set_family(font, fonts_available[fontnum]);

        pango_font_description_set_size(font, font_size * PANGO_SCALE);

        pango_layout_set_font_description(layout, font);
        pango_layout_set_text(layout, text, -1);
        getxypos(layout, &x_pos, &y_pos, width, height, cent, &dwidth, &dheight);

        if (!rise)
          y_pos = y_text = height * top;

        x_text = x_pos;
        y_text = y_pos;
        if (cent) pango_layout_set_alignment(layout, PANGO_ALIGN_CENTER);
        else pango_layout_set_alignment(layout, PANGO_ALIGN_LEFT);

        cairo_move_to(cairo, x_text, y_text);

        switch (mode) {
        case 1:
          cairo_set_source_rgba(cairo, (double)bg->red / 255.0, (double)bg->green / 255.0, (double)bg->blue / 255.0, b_alpha);
          fill_bckg(cairo, x_pos, y_pos, dwidth, dheight);
          cairo_move_to(cairo, x_text, y_text);
          cairo_set_source_rgba(cairo, (double)fg->red / 255.0, (double)fg->green / 255.0, (double)fg->blue / 255.0, f_alpha);
          pango_layout_set_text(layout, text, -1);
          break;
        case 2:
          cairo_set_source_rgba(cairo, (double)bg->red / 255.0, (double)bg->green / 255.0, (double)bg->blue / 255.0, b_alpha);
          fill_bckg(cairo, x_pos, y_pos, dwidth, dheight);
          cairo_move_to(cairo, x_pos, y_pos);
          cairo_set_source_rgba(cairo, (double)fg->red / 255.0, (double)fg->green / 255.0, (double)fg->blue / 255.0, f_alpha);
          pango_layout_set_text(layout, "", -1);
          break;
        case 0:
        default:
          cairo_set_source_rgba(cairo, (double)fg->red / 255.0, (double)fg->green / 255.0, (double)fg->blue / 255.0, f_alpha);
          break;
        }

        pango_cairo_show_layout(cairo, layout);
        g_object_unref(layout);
        pango_font_description_free(font);
      }
    }
    cairo_to_channel(cairo, out_channel);
    cairo_destroy(cairo);
  }

  weed_free(text);
  weed_free(fg);
  weed_free(bg);

  return WEED_SUCCESS;
}


static int font_compare(const void *p1, const void *p2) {
  const char *s1 = (const char *)(*(char **)p1);
  const char *s2 = (const char *)(*(char **)p2);
  char *u1 = g_utf8_casefold(s1, -1);
  char *u2 = g_utf8_casefold(s2, -1);
  int ret = strcmp(u1, u2);
  g_free(u1);
  g_free(u2);
  return ret;
}


WEED_SETUP_START(200, 200) {
  weed_plant_t **clone1, **clone2;

  const char *def_fonts[] = {"serif", NULL};
  const char *modes[] = {"foreground only", "foreground and background", "background only", NULL};
  // removed palettes with alpha channel
  int palette_list[2];
  weed_plant_t *in_chantmpls[2];
  weed_plant_t *out_chantmpls[2];
  weed_plant_t *in_params[P_END + 1], *pgui;
  weed_plant_t *filter_class, *gui;
  weed_plant_t *host_info = weed_get_host_info(plugin_info);
  PangoContext *ctx;
  int filter_flags = weed_host_supports_premultiplied_alpha(host_info);
  int param_flags = 0;

  if (is_big_endian())
    palette_list[0] = WEED_PALETTE_ARGB32;
  else
    palette_list[0] = WEED_PALETTE_BGRA32;

  palette_list[1] = WEED_PALETTE_END;

  in_chantmpls[0] = weed_channel_template_init("in channel 0", 0);
  in_chantmpls[1] = NULL;

  out_chantmpls[0] = weed_channel_template_init("out channel 0", WEED_CHANNEL_CAN_DO_INPLACE);
  out_chantmpls[1] = NULL;

  init_unal();

  // this section contains code
  // for configure fonts available
  num_fonts_available = 0;
  fonts_available = NULL;

  ctx = gdk_pango_context_get();
  if (ctx) {
    PangoFontMap *pfm = pango_context_get_font_map(ctx);
    if (pfm) {
      int num = 0;
      PangoFontFamily **pff = NULL;
      pango_font_map_list_families(pfm, &pff, &num);
      if (num > 0) {
        // we should reserve num+1 for a final NULL pointer
        fonts_available = (const char **)weed_malloc((num + 1) * sizeof(char *));
        if (fonts_available) {
          register int i;
          num_fonts_available = num;
          for (i = 0; i < num; ++i) {
            fonts_available[i] = strdup(pango_font_family_get_name(pff[i]));
          }
          // don't forget this thing
          fonts_available[num] = NULL;
          // also we sort fonts in alphabetical order
          qsort(fonts_available, num, sizeof(char *), font_compare);
        }
      }
      g_free(pff);
    }
    g_object_unref(ctx);
  }

  in_params[P_TEXT] = weed_text_init("text", "_Text", "");
  in_params[P_MODE] = weed_string_list_init("mode", "Colour _mode", 0, modes);
  param_flags = weed_get_int_value(in_params[P_MODE], WEED_LEAF_FLAGS, NULL);
  param_flags |= WEED_PARAMETER_REINIT_ON_VALUE_CHANGE;
  weed_set_int_value(in_params[P_MODE], WEED_LEAF_FLAGS, param_flags);
  gui = weed_paramtmpl_get_gui(in_params[P_MODE]);
  weed_gui_set_flags(gui, WEED_GUI_REINIT_ON_VALUE_CHANGE);

  if (fonts_available)
    in_params[P_FONT] = weed_string_list_init("font", "_Font", 0, fonts_available);
  else
    in_params[P_FONT] = weed_string_list_init("font", "_Font", 0, def_fonts);
  in_params[P_FOREGROUND] = weed_colRGBi_init("foreground", "_Foreground", 255, 255, 255);
  in_params[P_BACKGROUND] = weed_colRGBi_init("background", "_Background", 0, 0, 0);
  in_params[P_FGALPHA] = weed_float_init("fr_alpha", "_Alpha _Foreground", 1.0, 0.0, 1.0);
  in_params[P_BGALPHA] = weed_float_init("bg_alpha", "_Alpha _Background", 1.0, 0.0, 1.0);
  in_params[P_FONTSIZE] = weed_float_init("fontsize", "_Font Size", 20.0, 10.0, 128.0);
  in_params[P_CENTER] = weed_switch_init("center", "_Center text", WEED_TRUE);
  in_params[P_RISE] = weed_switch_init("rising", "_Rising text", WEED_TRUE);
  in_params[P_TOP] = weed_float_init("top", "_Top", 0.0, 0.0, 1.0);
  in_params[P_END] = NULL;

  pgui = weed_paramtmpl_get_gui(in_params[P_TEXT]);
  weed_set_int_value(pgui, WEED_LEAF_MAXCHARS, 65536);

  weed_set_int_value(in_params[P_FGALPHA], WEED_LEAF_COPY_VALUE_TO, P_BGALPHA);

  filter_class = weed_filter_class_init("scribbler", "Aleksej Penkov", 1, filter_flags, palette_list,
                                        scribbler_init, scribbler_process, NULL,
                                        in_chantmpls,
                                        out_chantmpls,
                                        in_params, NULL);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);

  filter_class = weed_filter_class_init("scribbler_generator", "Aleksej Penkov", 1, filter_flags, palette_list,
                                        scribbler_init, scribbler_process, NULL,
                                        NULL,
                                        (clone1 = weed_clone_plants(out_chantmpls)),
                                        (clone2 = weed_clone_plants(in_params)), NULL);
  weed_free(clone1);
  weed_free(clone2);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);
  weed_set_double_value(filter_class, WEED_LEAF_TARGET_FPS, 25.); // set reasonable default fps
  weed_set_int_value(plugin_info, WEED_LEAF_VERSION, package_version);
}
WEED_SETUP_END;


WEED_DESETUP_START {
  // clean up what we reserve for font family names
  if (num_fonts_available && fonts_available) {
    int i;
    for (i = 0; i < num_fonts_available; ++i) {
      free((void *)fonts_available[i]);
    }
    weed_free((void *)fonts_available);
  }
  num_fonts_available = 0;
  fonts_available = NULL;
}
WEED_DESETUP_END;
