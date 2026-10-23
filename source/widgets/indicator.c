/*
 * rofi
 *
 * MIT/X11 License
 * Copyright © 2013-2023 Qball Cow <qball@gmpclient.org>
 * Copyright © 2026 Kostr2007 (Material You selection indicator)
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

/** Log domain of the indicator. */
#define G_LOG_DOMAIN "Widgets.Indicator"

#include "config.h"

#include "theme.h"
#include "widgets/indicator.h"
#include "widgets/widget-internal.h"
#include <math.h>
#include <string.h>

/** Edges, in the order of RofiIndicator.edge. */
enum { EDGE_LEFT = 0, EDGE_TOP, EDGE_RIGHT, EDGE_BOTTOM };

void rofi_indicator_init(RofiIndicator *ind, widget *owner) {
  memset(ind, 0, sizeof(*ind));
  ind->enabled = rofi_theme_get_boolean(owner, "selection-indicator", FALSE);
  ind->stretch =
      rofi_theme_get_boolean(owner, "selection-indicator-stretch", TRUE);
  ind->spec = rofi_spring_spec_parse(
      rofi_theme_get_string(owner, "selection-indicator-spring", NULL),
      ROFI_SPRING_SPATIAL_DEFAULT);
  ind->lead_spec = rofi_spring_spec(ROFI_SPRING_SPATIAL_FAST);
  if (!ind->stretch) {
    ind->lead_spec = ind->spec;
  }
  for (int i = 0; i < 4; i++) {
    rofi_spring_init(&(ind->edge[i]), ind->spec, 0.2);
  }
  rofi_spring_init(&(ind->radius), ind->spec, 0.2);
  rofi_spring_init(&(ind->alpha), rofi_spring_spec(ROFI_SPRING_EFFECTS_SLOW),
                   0.002);
  rofi_spring_snap(&(ind->alpha), 0.0);
  rofi_color_transition_init(&(ind->color));
}

void rofi_indicator_set_selected(RofiIndicator *ind, widget *selected) {
  if (!ind->enabled) {
    return;
  }
  if (ind->selected != selected) {
    widget_set_background_suppressed(ind->selected, FALSE);
    ind->selected = NULL;
  }
  double rect[4], radius;
  if (selected == NULL ||
      !widget_get_background_rect(selected, &rect[0], &rect[1], &rect[2],
                                  &rect[3], &radius)) {
    rofi_spring_set_target(&(ind->alpha), 0.0);
    return;
  }
  ind->selected = selected;
  widget_set_background_suppressed(selected, TRUE);

  // Convert to edges.
  rect[2] += rect[0];
  rect[3] += rect[1];

  ThemeColor color = {0, 0, 0, 0};
  rofi_theme_get_color_value(selected, "background-color", &color);
  // Retarget the fade only, the color is advanced while drawing.
  if (!ind->color.initialized) {
    rofi_color_transition_update(&(ind->color), &color);
  } else if (memcmp(&(ind->color.to), &color, sizeof(color)) != 0) {
    rofi_color_transition_update(&(ind->color), &color);
  }

  if (!ind->placed) {
    for (int i = 0; i < 4; i++) {
      rofi_spring_snap(&(ind->edge[i]), rect[i]);
      ind->target[i] = rect[i];
    }
    rofi_spring_snap(&(ind->radius), radius);
    ind->placed = TRUE;
  } else if (memcmp(ind->target, rect, sizeof(rect)) != 0) {
    // Per axis: the edge in the direction of movement leads.
    for (int axis = 0; axis < 2; axis++) {
      int lo = axis, hi = axis + 2;
      double delta =
          (rect[lo] + rect[hi]) - (ind->target[lo] + ind->target[hi]);
      if (delta > 0.5) {
        ind->edge[hi].spec = ind->lead_spec;
        ind->edge[lo].spec = ind->spec;
      } else if (delta < -0.5) {
        ind->edge[lo].spec = ind->lead_spec;
        ind->edge[hi].spec = ind->spec;
      } else {
        ind->edge[lo].spec = ind->spec;
        ind->edge[hi].spec = ind->spec;
      }
    }
    for (int i = 0; i < 4; i++) {
      rofi_spring_set_target(&(ind->edge[i]), rect[i]);
      ind->target[i] = rect[i];
    }
  }
  rofi_spring_set_target(&(ind->radius), radius);
  rofi_spring_set_target(&(ind->alpha), 1.0);
}

void rofi_indicator_forget(RofiIndicator *ind, widget *wid) {
  if (ind->selected == wid) {
    ind->selected = NULL;
  }
}

gboolean rofi_indicator_get_rect(const RofiIndicator *ind, double *x, double *y,
                                 double *w, double *h, double *r) {
  if (!ind->enabled || !ind->placed || ind->alpha.value <= 0.0) {
    return FALSE;
  }
  double l = ind->edge[EDGE_LEFT].value;
  double t = ind->edge[EDGE_TOP].value;
  double rr = ind->edge[EDGE_RIGHT].value;
  double b = ind->edge[EDGE_BOTTOM].value;
  // An overshooting spring may cross the other edge, never draw inverted.
  *x = MIN(l, rr);
  *y = MIN(t, b);
  *w = fabs(rr - l);
  *h = fabs(b - t);
  *r = MAX(0.0, ind->radius.value);
  return TRUE;
}

void rofi_indicator_draw(RofiIndicator *ind, cairo_t *d) {
  if (!ind->enabled || !ind->placed) {
    return;
  }
  for (int i = 0; i < 4; i++) {
    rofi_spring_update(&(ind->edge[i]));
  }
  rofi_spring_update(&(ind->radius));
  rofi_spring_update(&(ind->alpha));
  ThemeColor c = rofi_color_transition_update(&(ind->color), &(ind->color.to));

  double x, y, w, h, r;
  if (!rofi_indicator_get_rect(ind, &x, &y, &w, &h, &r)) {
    return;
  }
  double alpha = c.alpha * CLAMP(ind->alpha.value, 0.0, 1.0);
  if (alpha <= 0.0) {
    return;
  }
  cairo_save(d);
  cairo_set_source_rgba(d, c.red, c.green, c.blue, alpha);
  rofi_rounded_rectangle(d, x, y, w, h, r);
  cairo_fill(d);
  cairo_restore(d);
}
