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

/** Log domain of the shapes code. */
#define G_LOG_DOMAIN "Shapes"

#include "config.h"

#include "rofi-shapes.h"
#include <math.h>
#include <string.h>

/** How the outline of a shape is computed. */
typedef enum {
  /** r = 1 */
  SHAPE_CIRCLE,
  /** Superellipse (squircle) with exponent a. */
  SHAPE_SQUIRCLE,
  /** Half circle on top, squircle at the bottom. */
  SHAPE_ARCH,
  /** Scalloped outline: 1 + a cos(n t), b shifts the lobes (fraction). */
  SHAPE_COOKIE,
  /** Rounded leaves with sharp notches: 1 - a (1 - |cos(n t / 2)|^0.6). */
  SHAPE_CLOVER,
  /** Star with rounded tips, a is the depth, b the sharpness (0..1). */
  SHAPE_BURST,
  /** Regular polygon with n corners rounded by a (fraction of the side). */
  SHAPE_POLYGON,
} ShapeKind;

/** Definition of a shape. */
typedef struct {
  /** Name used in themes. */
  const char *name;
  /** Outline function. */
  ShapeKind kind;
  /** Number of lobes, points or corners. */
  int n;
  /** First parameter, see ShapeKind. */
  double a;
  /** Second parameter, see ShapeKind. */
  double b;
} ShapeDef;

/** The shape library. */
static const ShapeDef shape_defs[] = {
    {"circle", SHAPE_CIRCLE, 0, 0.0, 0.0},
    {"square", SHAPE_SQUIRCLE, 0, 5.0, 0.0},
    {"squircle", SHAPE_SQUIRCLE, 0, 3.2, 0.0},
    {"arch", SHAPE_ARCH, 0, 5.0, 0.0},
    {"cookie-4", SHAPE_COOKIE, 4, 0.12, 0.5},
    {"cookie-6", SHAPE_COOKIE, 6, 0.09, 0.0},
    {"cookie-7", SHAPE_COOKIE, 7, 0.085, 0.0},
    {"cookie-9", SHAPE_COOKIE, 9, 0.07, 0.0},
    {"cookie-12", SHAPE_COOKIE, 12, 0.05, 0.0},
    {"clover-4", SHAPE_CLOVER, 4, 0.30, 0.0},
    {"clover-8", SHAPE_CLOVER, 8, 0.20, 0.0},
    {"flower", SHAPE_COOKIE, 8, 0.13, 0.0},
    {"sunny", SHAPE_BURST, 8, 0.10, 0.35},
    {"very-sunny", SHAPE_BURST, 8, 0.20, 0.55},
    {"soft-burst", SHAPE_BURST, 10, 0.12, 0.25},
    {"burst", SHAPE_BURST, 12, 0.16, 0.80},
    {"triangle", SHAPE_POLYGON, 3, 0.35, 0.0},
    {"pentagon", SHAPE_POLYGON, 5, 0.30, 0.0},
    {"gem", SHAPE_POLYGON, 6, 0.25, 0.0},
};

/** Number of shapes in the library. */
#define NUM_SHAPES (sizeof(shape_defs) / sizeof(shape_defs[0]))

/** Sampled outlines, built on first use. */
static RofiShape *shapes = NULL;
/** NULL terminated list of the shape names. */
static const char *shape_name_list[NUM_SHAPES + 1];

static double shape_squircle(double t, double n) {
  return pow(pow(fabs(cos(t)), n) + pow(fabs(sin(t)), n), -1.0 / n);
}

static double shape_polygon(double t, int n) {
  // Point a corner up: cairo angles grow clockwise, up is -pi/2.
  double seg = 2.0 * G_PI / n;
  double u = fmod(t + G_PI_2 + 2.0 * G_PI, seg) - seg / 2.0;
  return cos(seg / 2.0) / cos(u);
}

static double shape_eval(const ShapeDef *def, double t) {
  switch (def->kind) {
  case SHAPE_SQUIRCLE:
    return shape_squircle(t, def->a);
  case SHAPE_ARCH:
    return (sin(t) < 0.0) ? 1.0 : shape_squircle(t, def->a);
  case SHAPE_COOKIE:
    // Rotate half a lobe so the shape is symmetric around the vertical axis.
    return 1.0 + def->a * cos(def->n * (t + G_PI_2) + def->b * 2.0 * G_PI);
  case SHAPE_CLOVER:
    return 1.0 -
           def->a * (1.0 - pow(fabs(cos(def->n * (t + G_PI_2) / 2.0)), 0.6));
  case SHAPE_BURST: {
    double phase = def->n * (t + G_PI_2);
    double tri = (2.0 / G_PI) * asin(cos(phase));
    double wave = def->b * tri + (1.0 - def->b) * cos(phase);
    return 1.0 + def->a * wave;
  }
  case SHAPE_POLYGON:
    return shape_polygon(t, def->n);
  case SHAPE_CIRCLE:
  default:
    return 1.0;
  }
}

/**
 * Round sharp corners by averaging the outline over a window of angles.
 */
static void shape_smooth(double *radii, int window) {
  if (window < 1) {
    return;
  }
  double tmp[ROFI_SHAPE_SAMPLES];
  for (int i = 0; i < ROFI_SHAPE_SAMPLES; i++) {
    double sum = 0.0;
    for (int j = -window; j <= window; j++) {
      sum += radii[(i + j + ROFI_SHAPE_SAMPLES) % ROFI_SHAPE_SAMPLES];
    }
    tmp[i] = sum / (2 * window + 1);
  }
  memcpy(radii, tmp, sizeof(tmp));
}

/**
 * Scale the outline so it touches the edges of a 2x2 square.
 */
static void shape_fit(double *radii) {
  double extent = 0.0;
  for (int i = 0; i < ROFI_SHAPE_SAMPLES; i++) {
    double t = 2.0 * G_PI * i / ROFI_SHAPE_SAMPLES;
    extent = MAX(extent, radii[i] * fabs(cos(t)));
    extent = MAX(extent, radii[i] * fabs(sin(t)));
  }
  if (extent <= 0.0) {
    return;
  }
  for (int i = 0; i < ROFI_SHAPE_SAMPLES; i++) {
    radii[i] /= extent;
  }
}

static void shapes_init(void) {
  if (shapes != NULL) {
    return;
  }
  shapes = g_malloc0_n(NUM_SHAPES, sizeof(RofiShape));
  for (unsigned int s = 0; s < NUM_SHAPES; s++) {
    const ShapeDef *def = &shape_defs[s];
    double *radii = shapes[s].radii;
    for (int i = 0; i < ROFI_SHAPE_SAMPLES; i++) {
      radii[i] = shape_eval(def, 2.0 * G_PI * i / ROFI_SHAPE_SAMPLES);
    }
    if (def->kind == SHAPE_POLYGON) {
      // Corner radius as a fraction of the angle between two corners.
      int window = (int)(def->a * ROFI_SHAPE_SAMPLES / def->n / 2.0);
      for (int pass = 0; pass < 3; pass++) {
        shape_smooth(radii, window);
      }
    } else if (def->kind == SHAPE_BURST) {
      // Soften the tips a little.
      shape_smooth(radii, 1);
    }
    shape_fit(radii);
    shape_name_list[s] = def->name;
  }
  shape_name_list[NUM_SHAPES] = NULL;
}

const RofiShape *rofi_shape_find(const char *name) {
  if (name == NULL) {
    return NULL;
  }
  shapes_init();
  for (unsigned int s = 0; s < NUM_SHAPES; s++) {
    if (g_ascii_strcasecmp(shape_defs[s].name, name) == 0) {
      return &shapes[s];
    }
  }
  return NULL;
}

const char *const *rofi_shape_names(void) {
  shapes_init();
  return shape_name_list;
}

void rofi_shape_mix(const RofiShape *a, const RofiShape *b, double t,
                    RofiShape *out) {
  for (int i = 0; i < ROFI_SHAPE_SAMPLES; i++) {
    out->radii[i] = MAX(0.0, a->radii[i] + (b->radii[i] - a->radii[i]) * t);
  }
}

void rofi_shape_path(cairo_t *d, const RofiShape *shape, double cx, double cy,
                     double size, double rotation) {
  const double r = size / 2.0;
  const double rot = rotation * G_PI / 180.0;
  cairo_new_sub_path(d);
  for (int i = 0; i < ROFI_SHAPE_SAMPLES; i++) {
    double t = 2.0 * G_PI * i / ROFI_SHAPE_SAMPLES + rot;
    double x = cx + cos(t) * shape->radii[i] * r;
    double y = cy + sin(t) * shape->radii[i] * r;
    if (i == 0) {
      cairo_move_to(d, x, y);
    } else {
      cairo_line_to(d, x, y);
    }
  }
  cairo_close_path(d);
}
