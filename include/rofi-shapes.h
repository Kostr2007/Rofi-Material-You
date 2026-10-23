/*
 * rofi
 *
 * MIT/X11 License
 * Copyright © 2013-2023 Qball Cow <qball@gmpclient.org>
 * Copyright © 2026 Kostr2007 (Material You shapes)
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

#ifndef ROFI_SHAPES_H
#define ROFI_SHAPES_H

#include <cairo.h>
#include <glib.h>

/**
 * @defgroup SHAPES Shapes
 * @ingroup HELPERS
 *
 * The Material 3 Expressive shape library (circle, cookies, clovers,
 * bursts, ...). Every shape is stored as a polar outline, the distance from
 * the center sampled at ROFI_SHAPE_SAMPLES evenly spaced angles. Because all
 * shapes share the same sampling, morphing between two shapes is a plain
 * interpolation of the samples.
 *
 * @{
 */

/** Number of samples in a shape outline. */
#define ROFI_SHAPE_SAMPLES 256

/**
 * A shape outline, radii are normalized so the shape fits in a square of
 * size 2 centered on the origin.
 */
typedef struct {
  /** Distance from the center per angle, starting at 3 o'clock, clockwise
   * (cairo coordinates). */
  double radii[ROFI_SHAPE_SAMPLES];
} RofiShape;

/**
 * @param name The shape name, e.g. "cookie-9".
 *
 * @returns the shape, or NULL if there is no shape with this name.
 */
const RofiShape *rofi_shape_find(const char *name);

/**
 * @returns NULL terminated list of all shape names.
 */
const char *const *rofi_shape_names(void);

/**
 * @param a   Shape to morph from.
 * @param b   Shape to morph to.
 * @param t   Progress, 0 is a, 1 is b. Values outside [0,1] overshoot.
 * @param out The morphed shape.
 */
void rofi_shape_mix(const RofiShape *a, const RofiShape *b, double t,
                    RofiShape *out);

/**
 * @param d        The cairo context.
 * @param shape    The shape.
 * @param cx       Center x.
 * @param cy       Center y.
 * @param size     Width and height of the square the shape fills.
 * @param rotation Rotation in degrees, clockwise.
 *
 * Add the outline of the shape as a new closed sub path.
 */
void rofi_shape_path(cairo_t *d, const RofiShape *shape, double cx, double cy,
                     double size, double rotation);

/**@}*/
#endif // ROFI_SHAPES_H
