/*
 * rofi
 *
 * MIT/X11 License
 * Copyright © 2013-2018 Qball Cow <qball@gmpclient.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

/** The log domain of this widget. */
#include "cairo.h"
#define G_LOG_DOMAIN "Widgets.Icon"
#include "config.h"

#include "theme.h"
#include "widgets/icon.h"
#include "widgets/widget-internal.h"
#include "widgets/widget.h"
#include <stdio.h>

#include "rofi-animation.h"
#include "rofi-icon-fetcher.h"
#include "rofi-shapes.h"
#include <math.h>
#include <string.h>

struct _icon {
  widget widget;

  // Size of the icon.
  int size;

  int squared;

  uint32_t icon_fetch_id;

  double yalign, xalign;

  // Source surface.
  cairo_surface_t *icon;

  /** Material You: shape behind (or around) the icon. */
  struct {
    /** If any state of this icon has a shape. */
    gboolean used;
    /** Shape the morph goes to. */
    const RofiShape *to;
    /** Outline the morph started from. */
    RofiShape from;
    /** Morph progress, 0 (from) to 1 (to). */
    RofiSpring morph;
    /** Rotation in degrees. */
    RofiSpring rotation;
    /** Shape fill color. */
    RofiColorTransition color;
  } shape;
  /** Material You: size of the image relative to the widget. */
  RofiSpring scale;
};

static int icon_get_desired_height(widget *wid, G_GNUC_UNUSED const int width) {
  icon *b = (icon *)wid;
  int height = b->size;
  if (b->squared == FALSE) {
    if (b->icon) {
      int iconh = cairo_image_surface_get_height(b->icon);
      int iconw = cairo_image_surface_get_width(b->icon);
      int icons = MAX(iconh, iconw);
      double scale = (double)b->size / icons;
      height = iconh * scale;
    }
  }
  height += widget_padding_get_padding_height(wid);
  return height;
}
static int icon_get_desired_width(widget *wid, G_GNUC_UNUSED const int height) {
  icon *b = (icon *)wid;
  int width = b->size;
  if (b->squared == FALSE) {
    if (b->icon) {
      int iconh = cairo_image_surface_get_height(b->icon);
      int iconw = cairo_image_surface_get_width(b->icon);
      int icons = MAX(iconh, iconw);
      double scale = (double)b->size / icons;
      width = iconw * scale;
    }
  }
  width += widget_padding_get_padding_width(wid);
  return width;
}

/**
 * Update the shape springs to the values of the current state.
 *
 * @returns the shape to draw, or NULL.
 */
static const RofiShape *icon_shape_update(icon *b, RofiShape *buffer,
                                          ThemeColor *color) {
  widget *wid = WIDGET(b);
  const char *name = rofi_theme_get_string(wid, "shape", NULL);
  const RofiShape *target = rofi_shape_find(name);
  if (name != NULL && target == NULL) {
    g_warning("Unknown shape '%s'.", name);
  }
  if (target == NULL && !b->shape.used) {
    return NULL;
  }
  if (target == NULL) {
    // A state without shape: morph to a circle and fade out.
    target = rofi_shape_find("circle");
  }
  RofiSpringSpec spec =
      rofi_spring_spec_parse(rofi_theme_get_string(wid, "shape-spring", NULL),
                             ROFI_SPRING_SPATIAL_FAST);
  if (!b->shape.used) {
    b->shape.used = TRUE;
    b->shape.to = target;
    b->shape.from = *target;
    rofi_spring_init(&(b->shape.morph), spec, 0.001);
    rofi_spring_snap(&(b->shape.morph), 1.0);
    rofi_spring_init(&(b->shape.rotation), spec, 0.05);
    rofi_color_transition_init(&(b->shape.color));
  } else if (target != b->shape.to) {
    // Morph from whatever is visible now.
    RofiShape current;
    rofi_shape_mix(&(b->shape.from), b->shape.to, b->shape.morph.value,
                   &current);
    b->shape.from = current;
    b->shape.to = target;
    b->shape.morph.spec = spec;
    rofi_spring_snap(&(b->shape.morph), 0.0);
    rofi_spring_set_target(&(b->shape.morph), 1.0);
  }
  b->shape.rotation.spec = spec;
  rofi_spring_set_target(&(b->shape.rotation),
                         rofi_theme_get_double(wid, "shape-rotation", 0.0));
  rofi_spring_update(&(b->shape.morph));
  rofi_spring_update(&(b->shape.rotation));

  ThemeColor target_color = {0, 0, 0, 0};
  if (name != NULL) {
    rofi_theme_get_color_value(wid, "shape-color", &target_color);
  }
  *color = rofi_color_transition_update(&(b->shape.color), &target_color);

  rofi_shape_mix(&(b->shape.from), b->shape.to, b->shape.morph.value, buffer);
  return buffer;
}

static void icon_draw(widget *wid, cairo_t *draw) {
  icon *b = (icon *)wid;

  // Material You: shape and image scale.
  RofiShape shape_buffer;
  ThemeColor shape_color = {0, 0, 0, 0};
  const RofiShape *shape = icon_shape_update(b, &shape_buffer, &shape_color);
  rofi_spring_set_target(&(b->scale),
                         rofi_theme_get_double(wid, "icon-scale", 1.0));
  rofi_spring_update(&(b->scale));
  double content_w = b->widget.w - widget_padding_get_padding_width(WIDGET(b));
  double content_h = b->widget.h - widget_padding_get_padding_height(WIDGET(b));
  double shape_size = MIN(content_w, content_h);
  double shape_cx = widget_padding_get_left(WIDGET(b)) +
                    (content_w - shape_size) * b->xalign + shape_size / 2.0;
  double shape_cy = widget_padding_get_top(WIDGET(b)) +
                    (content_h - shape_size) * b->yalign + shape_size / 2.0;
  // No shape for rows without an icon (run, ssh, ...), it would be an empty
  // blob.
  gboolean has_image = (b->icon != NULL || b->icon_fetch_id > 0);
  if (shape != NULL && shape_color.alpha > 0.0 && has_image) {
    cairo_save(draw);
    cairo_set_source_rgba(draw, shape_color.red, shape_color.green,
                          shape_color.blue, shape_color.alpha);
    rofi_shape_path(draw, shape, shape_cx, shape_cy, shape_size,
                    b->shape.rotation.value);
    cairo_fill(draw);
    cairo_restore(draw);
  }

  // If no icon is loaded. quit.
  if (b->icon == NULL && b->icon_fetch_id > 0) {
    b->icon = rofi_icon_fetcher_get(b->icon_fetch_id);
    if (b->icon) {
      cairo_surface_reference(b->icon);
    }
  }
  if (b->icon == NULL) {
    return;
  }
  int iconh = cairo_image_surface_get_height(b->icon);
  int iconw = cairo_image_surface_get_width(b->icon);
  int lpad = widget_padding_get_left(WIDGET(b));
  int rpad = widget_padding_get_right(WIDGET(b));
  int tpad = widget_padding_get_top(WIDGET(b));
  int bpad = widget_padding_get_bottom(WIDGET(b));
  double scalex = (double)(b->widget.w - lpad - rpad) / iconw;
  double scaley = (double)(b->widget.h - tpad - bpad) / iconh;
  double scale = MIN(scalex, scaley);

  cairo_save(draw);
  if (shape != NULL && rofi_theme_get_boolean(WIDGET(b), "shape-clip", FALSE)) {
    rofi_shape_path(draw, shape, shape_cx, shape_cy, shape_size,
                    b->shape.rotation.value);
    cairo_clip(draw);
  }
  // Scale the image around the center of its box.
  double image_scale = MAX(0.0, b->scale.value);
  double box_x = lpad + (b->widget.w - iconw * scale - lpad - rpad) * b->xalign;
  double box_y = tpad + (b->widget.h - iconh * scale - tpad - bpad) * b->yalign;
  cairo_translate(draw, box_x + iconw * scale * (1.0 - image_scale) / 2.0,
                  box_y + iconh * scale * (1.0 - image_scale) / 2.0);
  scale *= image_scale;
  if (scale <= 0.0) {
    cairo_restore(draw);
    return;
  }
  cairo_scale(draw, scale, scale);
  if (rofi_theme_has_property(WIDGET(wid), P_COLOR, "tint")) {
    cairo_pattern_t *pat = cairo_pattern_create_for_surface(b->icon);
    cairo_set_source_rgb(draw, 0, 0, 0);
    rofi_theme_get_color(WIDGET(wid), "tint", draw);
    cairo_mask(draw, pat);
    cairo_set_operator(draw, CAIRO_OPERATOR_HSL_LUMINOSITY);
    cairo_pattern_destroy(pat);
  }
  cairo_set_source_surface(draw, b->icon, 0, 0);
  cairo_paint(draw);
  cairo_restore(draw);
}

static void icon_free(widget *wid) {
  icon *b = (icon *)wid;

  if (b->icon) {
    cairo_surface_destroy(b->icon);
  }

  g_free(b);
}

static void icon_resize(widget *wid, short w, short h) {
  icon *b = (icon *)wid;
  if (b->widget.w != w || b->widget.h != h) {
    b->widget.w = w;
    b->widget.h = h;
    widget_update(wid);
  }
}

void icon_set_surface(icon *icon_widget, cairo_surface_t *surf) {
  icon_widget->icon_fetch_id = 0;
  if (icon_widget->icon) {
    cairo_surface_destroy(icon_widget->icon);
    icon_widget->icon = NULL;
  }
  if (surf) {
    cairo_surface_reference(surf);
    icon_widget->icon = surf;
  }
  widget_queue_redraw(WIDGET(icon_widget));
}

icon *icon_create(widget *parent, const char *name) {
  icon *b = g_malloc0(sizeof(icon));

  b->size = 16;
  // Initialize widget.
  widget_init(WIDGET(b), parent, WIDGET_TYPE_UNKNOWN, name);
  b->widget.draw = icon_draw;
  b->widget.free = icon_free;
  b->widget.resize = icon_resize;
  b->widget.get_desired_height = icon_get_desired_height;
  b->widget.get_desired_width = icon_get_desired_width;

  RofiDistance d = rofi_theme_get_distance(WIDGET(b), "size", b->size);
  b->size = distance_get_pixel(d, ROFI_ORIENTATION_VERTICAL);

  b->squared = rofi_theme_get_boolean(WIDGET(b), "squared", TRUE);

  const char *filename = rofi_theme_get_string(WIDGET(b), "filename", NULL);
  if (filename) {
    b->icon_fetch_id = rofi_icon_fetcher_query(filename, b->size);
  }
  b->yalign = rofi_theme_get_double(WIDGET(b), "vertical-align", 0.5);
  b->yalign = MAX(0, MIN(1.0, b->yalign));
  b->xalign = rofi_theme_get_double(WIDGET(b), "horizontal-align", 0.5);
  b->xalign = MAX(0, MIN(1.0, b->xalign));

  rofi_spring_init(&(b->scale),
                   rofi_spring_spec_parse(
                       rofi_theme_get_string(WIDGET(b), "shape-spring", NULL),
                       ROFI_SPRING_SPATIAL_FAST),
                   0.001);
  rofi_spring_snap(&(b->scale),
                   rofi_theme_get_double(WIDGET(b), "icon-scale", 1.0));

  return b;
}
