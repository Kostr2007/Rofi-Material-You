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

#ifndef ROFI_INDICATOR_H
#define ROFI_INDICATOR_H

#include "rofi-animation.h"
#include "widgets/widget.h"
#include <cairo.h>

/**
 * @defgroup indicator Indicator
 * @ingroup widget
 *
 * The Material You selection indicator: instead of every element painting
 * its own "selected" background, the container paints one rounded shape
 * that springs from the old selection to the new one. The leading edge
 * moves on a faster spring than the trailing edge, so the indicator
 * stretches on the way and settles with a little bounce.
 *
 * Enabled with `selection-indicator: true;` on a listview or box.
 *
 * @{
 */

/**
 * State of a selection indicator.
 */
typedef struct {
  /** Left, top, right and bottom edge. */
  RofiSpring edge[4];
  /** Corner radius. */
  RofiSpring radius;
  /** Opacity, fades in on first selection and out when nothing is
   * selected. */
  RofiSpring alpha;
  /** Background color. */
  RofiColorTransition color;
  /** Last target, in the owner coordinates. */
  double target[4];
  /** If the indicator has a position yet. */
  gboolean placed;
  /** Feature enabled. */
  gboolean enabled;
  /** Leading edge moves faster. */
  gboolean stretch;
  /** Spring of the trailing edge. */
  RofiSpringSpec spec;
  /** Spring of the leading edge. */
  RofiSpringSpec lead_spec;
  /** Widget whose background was taken over. */
  widget *selected;
} RofiIndicator;

/**
 * @param ind   The indicator.
 * @param owner The widget (listview or box) the indicator is drawn in.
 *
 * Read the theme properties of the owner and reset the state.
 */
void rofi_indicator_init(RofiIndicator *ind, widget *owner);

/**
 * @param ind      The indicator.
 * @param selected The selected child of the owner, or NULL.
 *
 * Move the indicator to the background of the selected widget and take
 * over painting its background.
 */
void rofi_indicator_set_selected(RofiIndicator *ind, widget *selected);

/**
 * @param ind The indicator.
 * @param wid A widget that is about to be freed.
 *
 * Drop the reference to wid if it is the selected widget.
 */
void rofi_indicator_forget(RofiIndicator *ind, widget *wid);

/**
 * @param ind The indicator.
 * @param d   The cairo context, in the coordinates of the owner.
 *
 * Advance the springs and paint the indicator.
 */
void rofi_indicator_draw(RofiIndicator *ind, cairo_t *d);

/**
 * @param ind The indicator.
 * @param x   Set to the x position.
 * @param y   Set to the y position.
 * @param w   Set to the width.
 * @param h   Set to the height.
 * @param r   Set to the corner radius.
 *
 * @returns FALSE if the indicator is not visible.
 */
gboolean rofi_indicator_get_rect(const RofiIndicator *ind, double *x, double *y,
                                 double *w, double *h, double *r);

/**@}*/
#endif // ROFI_INDICATOR_H
