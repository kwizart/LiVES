/* LIBGIW - The GIW Library
   Copyright (C) 1995-1997 Peter Mattis and Spencer Kimball

   This library is free software: you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library.  If not, see
   <http://www.gnu.org/licenses/>.
*/

// giwtimeline.h (c) 2013 - 2019 G. Finch salsaman@gmail.com

#ifndef __GIW_TIMELINE_H__
#define __GIW_TIMELINE_H__

#include <gdk/gdk.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef enum {
  GIW_TIME_UNIT_SECONDS,
  GIW_TIME_UNIT_SMH
} GiwTimeUnit;

#define GIW_TYPE_TIMELINE            (giw_timeline_get_type ())
#define GIW_TIMELINE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIW_TYPE_TIMELINE, GiwTimeline))
#define GIW_TIMELINE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GIW_TYPE_TIMELINE, GiwTimelineClass))
#define GIW_IS_TIMELINE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIW_TYPE_TIMELINE))
#define GIW_IS_TIMELINE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GIW_TYPE_TIMELINE))
#define GIW_TIMELINE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GIW_TYPE_TIMELINE, GiwTimelineClass))

typedef enum {
  GIW_TIMELINE_MOUSE_DISABLED,
  GIW_TIMELINE_MOUSE_AUTOMATIC
  //GIW_TIMELINE_MOUSE_DELAYED
} GiwTimelineMousePolicy;

typedef struct _GiwTimeline   GiwTimeline;
typedef struct _GiwTimelineClass   GiwTimelineClass;

struct _GiwTimeline {
  GtkWidget parent_instance;
  GtkWidget widget;

  GtkAdjustment *adjustment;
  GtkOrientation   orientation;
  GiwTimeUnit      unit;
  gdouble          max_size;

  cairo_surface_t *backing_store;
  PangoLayout     *layout;
  gdouble          font_scale;

  gint             xsrc;
  gint             ysrc;

  gint button;

  gulong chsig;
  gulong vchsig;

  // Policy of mouse (GIW_TIMELINE_MOUSE_DISABLED, GIW_TIMELINE_MOUSE_AUTOMATIC, GIW_TIMELINE_MOUSE_DELAYED)
  gint mouse_policy;

  GList           *track_widgets;
};

struct _GiwTimelineClass {
  GtkScaleClass  parent_class;
};

GType       giw_timeline_get_type(void) G_GNUC_CONST;
GtkWidget *giw_timeline_new(GtkOrientation  orientation, GtkAdjustment *adjustment);
GtkWidget *giw_timeline_new_with_adjustment(GtkOrientation orientation, gdouble value, gdouble lower, gdouble upper,
    gdouble max_size);
void        giw_timeline_add_track_widget(GiwTimeline      *timeline,
    GtkWidget      *widget);
void        giw_timeline_remove_track_widget(GiwTimeline      *timeline,
    GtkWidget      *widget);
void        giw_timeline_set_max_size(GiwTimeline      *timeline,
                                      gdouble         max_size);
gdouble     giw_timeline_get_max_size(GiwTimeline    *timeline);
void        giw_timeline_set_unit(GiwTimeline    *timeline,
                                  GiwTimeUnit    unit);
GiwTimeUnit giw_timeline_get_unit(GiwTimeline    *timeline);

void           giw_timeline_set_value(GiwTimeline *timeline, gdouble value);
gdouble        giw_timeline_get_value(GiwTimeline *timeline);
GtkAdjustment *giw_timeline_get_adjustment(GiwTimeline *timeline);
void           giw_timeline_set_adjustment(GiwTimeline *timeline, GtkAdjustment *adjustment);
void giw_timeline_set_range(GiwTimeline *timeline, gdouble lower, gdouble upper, gdouble max_size);
void           giw_timeline_set_mouse_policy(GiwTimeline *timeline, GiwTimelineMousePolicy policy);

G_END_DECLS

#endif /* __GIW_TIMELINE_H__ */
