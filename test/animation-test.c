/*
 * rofi
 *
 * MIT/X11 License
 * Copyright © 2013-2023 Qball Cow <qball@gmpclient.org>
 * Copyright © 2026 Kostr2007 (Material You animations)
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

#include "rofi-animation.h"
#include "rofi-shapes.h"
#include "rofi-types.h"
#include "settings.h"
#include <assert.h>
#include <glib.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

unsigned int test = 0;
#define TASSERT(a)                                                             \
  {                                                                            \
    assert(a);                                                                 \
    printf("Test %3u passed (%s)\n", ++test, #a);                              \
  }

#define CLOSE(a, b, eps) (fabs((a) - (b)) < (eps))

/** Step a spring with a fixed time step until it rests, returns the time. */
static double settle(RofiSpring *s, double dt, double *max_value) {
  double t = 0.0;
  *max_value = s->value;
  while (rofi_spring_step(s, dt) && t < 20.0) {
    t += dt;
    *max_value = MAX(*max_value, s->value);
  }
  return t;
}

static void test_tokens(void) {
  RofiSpringSpec e = rofi_spring_token_spec(ROFI_MOTION_EXPRESSIVE,
                                            ROFI_SPRING_SPATIAL_DEFAULT);
  TASSERT(CLOSE(e.stiffness, 380.0, 1e-9) && CLOSE(e.damping, 0.8, 1e-9));
  RofiSpringSpec s =
      rofi_spring_token_spec(ROFI_MOTION_STANDARD, ROFI_SPRING_SPATIAL_DEFAULT);
  TASSERT(CLOSE(s.stiffness, 700.0, 1e-9) && CLOSE(s.damping, 0.9, 1e-9));
  // Effects springs never overshoot.
  for (int i = ROFI_SPRING_EFFECTS_FAST; i <= ROFI_SPRING_EFFECTS_SLOW; i++) {
    TASSERT(rofi_spring_token_spec(ROFI_MOTION_EXPRESSIVE, i).damping >= 1.0);
  }

  RofiMotionScheme scheme;
  TASSERT(rofi_motion_scheme_parse("Standard", &scheme) &&
          scheme == ROFI_MOTION_STANDARD);
  TASSERT(!rofi_motion_scheme_parse("bouncy", &scheme));

  RofiSpringSpec fast =
      rofi_spring_spec_parse("spatial-fast", ROFI_SPRING_SPATIAL_DEFAULT);
  TASSERT(CLOSE(fast.stiffness, 800.0, 1e-9) && CLOSE(fast.damping, 0.6, 1e-9));
  RofiSpringSpec custom =
      rofi_spring_spec_parse("500 0.5", ROFI_SPRING_SPATIAL_DEFAULT);
  TASSERT(CLOSE(custom.stiffness, 500.0, 1e-9) &&
          CLOSE(custom.damping, 0.5, 1e-9));
  RofiSpringSpec fallback =
      rofi_spring_spec_parse(NULL, ROFI_SPRING_EFFECTS_SLOW);
  TASSERT(CLOSE(fallback.stiffness, 800.0, 1e-9) &&
          CLOSE(fallback.damping, 1.0, 1e-9));
}

static void test_spring(void) {
  double max_value;
  RofiSpring s;

  // The first target is snapped to.
  rofi_spring_init(
      &s,
      rofi_spring_token_spec(ROFI_MOTION_EXPRESSIVE, ROFI_SPRING_SPATIAL_FAST),
      0.01);
  rofi_spring_set_target(&s, 10.0);
  TASSERT(s.value == 10.0 && rofi_spring_at_rest(&s));

  // Underdamped: overshoots, then settles on the target.
  rofi_spring_set_target(&s, 110.0);
  TASSERT(!rofi_spring_at_rest(&s));
  double t = settle(&s, 1.0 / 120.0, &max_value);
  TASSERT(s.value == 110.0 && s.velocity == 0.0);
  TASSERT(max_value > 110.0);
  TASSERT(t > 0.1 && t < 2.0);

  // Critically damped: no overshoot.
  rofi_spring_init(&s,
                   rofi_spring_token_spec(ROFI_MOTION_EXPRESSIVE,
                                          ROFI_SPRING_EFFECTS_DEFAULT),
                   0.001);
  rofi_spring_snap(&s, 0.0);
  rofi_spring_set_target(&s, 1.0);
  settle(&s, 1.0 / 60.0, &max_value);
  TASSERT(s.value == 1.0 && max_value <= 1.0 + 1e-9);

  // Overdamped, also stable with huge time steps (exact solution).
  rofi_spring_init(&s, (RofiSpringSpec){.stiffness = 300.0, .damping = 2.0},
                   0.001);
  rofi_spring_snap(&s, 0.0);
  rofi_spring_set_target(&s, 1.0);
  settle(&s, 0.5, &max_value);
  TASSERT(s.value == 1.0 && max_value <= 1.0 + 1e-9);

  // Same end state for different frame rates.
  RofiSpring a, b;
  RofiSpringSpec spec = rofi_spring_token_spec(ROFI_MOTION_EXPRESSIVE,
                                               ROFI_SPRING_SPATIAL_DEFAULT);
  rofi_spring_init(&a, spec, 1e-6);
  rofi_spring_init(&b, spec, 1e-6);
  rofi_spring_snap(&a, 0.0);
  rofi_spring_snap(&b, 0.0);
  rofi_spring_set_target(&a, 1.0);
  rofi_spring_set_target(&b, 1.0);
  for (int i = 0; i < 30; i++) {
    rofi_spring_step(&a, 0.01);
  }
  for (int i = 0; i < 3; i++) {
    rofi_spring_step(&b, 0.1);
  }
  TASSERT(CLOSE(a.value, b.value, 1e-9) && CLOSE(a.velocity, b.velocity, 1e-6));

  // Retargeting keeps the velocity.
  double velocity = a.velocity;
  rofi_spring_set_target(&a, -5.0);
  TASSERT(a.velocity == velocity && a.target == -5.0);
}

static void test_step_response(void) {
  RofiSpringSpec spec =
      rofi_spring_token_spec(ROFI_MOTION_EXPRESSIVE, ROFI_SPRING_SPATIAL_FAST);
  TASSERT(rofi_spring_step_response(spec, 0.0) == 0.0);
  TASSERT(rofi_spring_step_response(spec, -1.0) == 0.0);
  double settle_time = rofi_spring_settle_time(spec);
  TASSERT(settle_time > 0.1 && settle_time < 1.0);
  TASSERT(CLOSE(rofi_spring_step_response(spec, settle_time), 1.0, 0.002));
  // Expressive spatial springs bounce: the response peaks above 1.
  double peak = 0.0;
  for (double t = 0.0; t < settle_time; t += 0.001) {
    peak = MAX(peak, rofi_spring_step_response(spec, t));
  }
  TASSERT(peak > 1.05 && peak < 1.2);
}

static void test_disabled(void) {
  config.animations = FALSE;
  rofi_animation_reload_settings();
  TASSERT(!rofi_animation_enabled());
  RofiSpring s;
  rofi_spring_init(&s, rofi_spring_spec(ROFI_SPRING_SPATIAL_DEFAULT), 0.1);
  rofi_spring_snap(&s, 0.0);
  rofi_spring_set_target(&s, 42.0);
  TASSERT(!rofi_spring_update(&s) && s.value == 42.0);

  config.animations = TRUE;
  g_setenv("ROFI_NO_ANIMATION", "1", TRUE);
  rofi_animation_reload_settings();
  TASSERT(!rofi_animation_enabled());
  g_unsetenv("ROFI_NO_ANIMATION");

  config.motion_scheme = "standard";
  config.animation_speed = "2.5";
  config.animation_fps = 60;
  rofi_animation_reload_settings();
  TASSERT(rofi_animation_enabled());
  TASSERT(rofi_animation_motion_scheme() == ROFI_MOTION_STANDARD);
  TASSERT(CLOSE(rofi_animation_speed(), 2.5, 1e-9));
  TASSERT(rofi_animation_frame_interval() == 16);
}

static void test_color(void) {
  RofiColorTransition ct;
  rofi_color_transition_init(&ct);
  ThemeColor red = {1.0, 0.0, 0.0, 1.0};
  ThemeColor blue = {0.0, 0.0, 1.0, 1.0};
  ThemeColor c = rofi_color_transition_update(&ct, &red);
  TASSERT(c.red == 1.0 && c.blue == 0.0);
  c = rofi_color_transition_update(&ct, &blue);
  // The fade starts from the current color.
  TASSERT(c.red == 1.0);
  for (int i = 0; i < 100; i++) {
    rofi_spring_step(&(ct.progress), 0.01);
  }
  c = rofi_color_transition_update(&ct, &blue);
  TASSERT(CLOSE(c.blue, 1.0, 1e-6) && CLOSE(c.red, 0.0, 1e-6));
}

static void test_shapes(void) {
  const char *const *names = rofi_shape_names();
  int n = 0;
  for (; names[n] != NULL; n++) {
    const RofiShape *shape = rofi_shape_find(names[n]);
    assert(shape != NULL);
    // Every shape fits its box and touches it.
    double extent = 0.0;
    for (int i = 0; i < ROFI_SHAPE_SAMPLES; i++) {
      double t = 2.0 * G_PI * i / ROFI_SHAPE_SAMPLES;
      assert(shape->radii[i] > 0.0);
      extent = MAX(extent, shape->radii[i] * MAX(fabs(cos(t)), fabs(sin(t))));
    }
    assert(CLOSE(extent, 1.0, 1e-9));
  }
  TASSERT(n >= 15);
  TASSERT(rofi_shape_find("Cookie-9") == rofi_shape_find("cookie-9"));
  TASSERT(rofi_shape_find("does-not-exist") == NULL);
  TASSERT(rofi_shape_find(NULL) == NULL);

  // Morphing interpolates the outline.
  const RofiShape *circle = rofi_shape_find("circle");
  const RofiShape *square = rofi_shape_find("square");
  RofiShape mixed;
  rofi_shape_mix(circle, square, 0.0, &mixed);
  TASSERT(mixed.radii[17] == circle->radii[17]);
  rofi_shape_mix(circle, square, 1.0, &mixed);
  TASSERT(mixed.radii[17] == square->radii[17]);
  rofi_shape_mix(circle, square, 0.5, &mixed);
  TASSERT(CLOSE(mixed.radii[32], (circle->radii[32] + square->radii[32]) / 2.0,
                1e-12));
}

int main(G_GNUC_UNUSED int argc, G_GNUC_UNUSED char **argv) {
  test_tokens();
  test_spring();
  test_step_response();
  test_disabled();
  test_color();
  test_shapes();
  return EXIT_SUCCESS;
}
