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

/** Log domain of the animation code. */
#define G_LOG_DOMAIN "Animation"

#include "config.h"

#include "rofi-animation.h"
#include "rofi-types.h"
#include "settings.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/** Largest time step taken in one frame, avoids jumps after a stall. */
#define ANIMATION_MAX_STEP 0.05

/**
 * Material 3 motion tokens (stiffness, damping ratio).
 * Spatial springs differ per scheme, effects springs are shared.
 */
static const RofiSpringSpec spring_tokens[2][ROFI_SPRING_TOKEN_NUM] = {
    [ROFI_MOTION_EXPRESSIVE] =
        {
            [ROFI_SPRING_SPATIAL_FAST] = {800.0, 0.6},
            [ROFI_SPRING_SPATIAL_DEFAULT] = {380.0, 0.8},
            [ROFI_SPRING_SPATIAL_SLOW] = {200.0, 0.8},
            [ROFI_SPRING_EFFECTS_FAST] = {3800.0, 1.0},
            [ROFI_SPRING_EFFECTS_DEFAULT] = {1600.0, 1.0},
            [ROFI_SPRING_EFFECTS_SLOW] = {800.0, 1.0},
        },
    [ROFI_MOTION_STANDARD] =
        {
            [ROFI_SPRING_SPATIAL_FAST] = {1400.0, 0.9},
            [ROFI_SPRING_SPATIAL_DEFAULT] = {700.0, 0.9},
            [ROFI_SPRING_SPATIAL_SLOW] = {300.0, 0.9},
            [ROFI_SPRING_EFFECTS_FAST] = {3800.0, 1.0},
            [ROFI_SPRING_EFFECTS_DEFAULT] = {1600.0, 1.0},
            [ROFI_SPRING_EFFECTS_SLOW] = {800.0, 1.0},
        },
};

/** Names of the motion tokens as used in themes. */
static const char *const spring_token_names[ROFI_SPRING_TOKEN_NUM] = {
    [ROFI_SPRING_SPATIAL_FAST] = "spatial-fast",
    [ROFI_SPRING_SPATIAL_DEFAULT] = "spatial-default",
    [ROFI_SPRING_SPATIAL_SLOW] = "spatial-slow",
    [ROFI_SPRING_EFFECTS_FAST] = "effects-fast",
    [ROFI_SPRING_EFFECTS_DEFAULT] = "effects-default",
    [ROFI_SPRING_EFFECTS_SLOW] = "effects-slow",
};

/** Global animation state. */
static struct {
  /** Settings were read. */
  gboolean loaded;
  /** Animations are on. */
  gboolean enabled;
  /** Configured motion scheme. */
  RofiMotionScheme scheme;
  /** Speed factor. */
  double speed;
  /** Frame interval in milliseconds. */
  guint interval;
  /** Timestamp of the current frame. */
  gint64 frame_time;
  /** Time the view became visible. */
  gint64 epoch;
  /** Schedules a redraw of the view. */
  void (*hook)(void);
  /** Pending frame timeout. */
  guint frame_source;
} AnimationState = {
    .loaded = FALSE,
    .enabled = TRUE,
    .scheme = ROFI_MOTION_EXPRESSIVE,
    .speed = 1.0,
    .interval = 16,
    .frame_time = 0,
    .epoch = 0,
    .hook = NULL,
    .frame_source = 0,
};

gboolean rofi_motion_scheme_parse(const char *name, RofiMotionScheme *scheme) {
  if (name == NULL) {
    return FALSE;
  }
  if (g_ascii_strcasecmp(name, "expressive") == 0) {
    *scheme = ROFI_MOTION_EXPRESSIVE;
    return TRUE;
  }
  if (g_ascii_strcasecmp(name, "standard") == 0) {
    *scheme = ROFI_MOTION_STANDARD;
    return TRUE;
  }
  return FALSE;
}

void rofi_animation_reload_settings(void) {
  AnimationState.loaded = TRUE;
  AnimationState.enabled = config.animations;
  const char *env = g_getenv("ROFI_NO_ANIMATION");
  if (env != NULL && env[0] != '\0' && g_strcmp0(env, "0") != 0) {
    AnimationState.enabled = FALSE;
  }

  AnimationState.scheme = ROFI_MOTION_EXPRESSIVE;
  if (config.motion_scheme != NULL &&
      !rofi_motion_scheme_parse(config.motion_scheme, &AnimationState.scheme)) {
    g_warning("Unknown motion scheme '%s', using 'expressive'.",
              config.motion_scheme);
  }

  AnimationState.speed = 1.0;
  if (config.animation_speed != NULL) {
    char *end = NULL;
    double speed = g_ascii_strtod(config.animation_speed, &end);
    if (end != config.animation_speed && speed > 0.0) {
      AnimationState.speed = CLAMP(speed, 0.05, 20.0);
    } else {
      g_warning("Invalid animation speed '%s', using 1.0.",
                config.animation_speed);
    }
  }

  guint fps = CLAMP(config.animation_fps, 10u, 360u);
  AnimationState.interval = MAX(1u, 1000u / fps);
}

static inline void rofi_animation_ensure_loaded(void) {
  if (!AnimationState.loaded) {
    rofi_animation_reload_settings();
  }
}

gboolean rofi_animation_enabled(void) {
  rofi_animation_ensure_loaded();
  return AnimationState.enabled;
}

RofiMotionScheme rofi_animation_motion_scheme(void) {
  rofi_animation_ensure_loaded();
  return AnimationState.scheme;
}

double rofi_animation_speed(void) {
  rofi_animation_ensure_loaded();
  return AnimationState.speed;
}

guint rofi_animation_frame_interval(void) {
  rofi_animation_ensure_loaded();
  return AnimationState.interval;
}

RofiSpringSpec rofi_spring_token_spec(RofiMotionScheme scheme,
                                      RofiSpringToken token) {
  if ((unsigned)scheme > ROFI_MOTION_STANDARD) {
    scheme = ROFI_MOTION_EXPRESSIVE;
  }
  if ((unsigned)token >= ROFI_SPRING_TOKEN_NUM) {
    token = ROFI_SPRING_SPATIAL_DEFAULT;
  }
  return spring_tokens[scheme][token];
}

RofiSpringSpec rofi_spring_spec(RofiSpringToken token) {
  return rofi_spring_token_spec(rofi_animation_motion_scheme(), token);
}

RofiSpringSpec rofi_spring_spec_parse(const char *name,
                                      RofiSpringToken fallback) {
  if (name == NULL) {
    return rofi_spring_spec(fallback);
  }
  for (int i = 0; i < ROFI_SPRING_TOKEN_NUM; i++) {
    if (g_ascii_strcasecmp(name, spring_token_names[i]) == 0) {
      return rofi_spring_spec((RofiSpringToken)i);
    }
  }
  // Custom spring: "<stiffness> <damping>".
  char *end = NULL;
  double stiffness = g_ascii_strtod(name, &end);
  if (end != name && stiffness > 0.0) {
    const char *rest = end;
    double damping = g_ascii_strtod(rest, &end);
    if (end != rest && damping > 0.0) {
      return (RofiSpringSpec){.stiffness = MIN(stiffness, 100000.0),
                              .damping = MIN(damping, 10.0)};
    }
  }
  g_warning("Invalid spring '%s', expected a motion token or "
            "'<stiffness> <damping>'.",
            name);
  return rofi_spring_spec(fallback);
}

void rofi_animation_begin_frame(void) {
  AnimationState.frame_time = g_get_monotonic_time();
}

gint64 rofi_animation_frame_time(void) {
  if (AnimationState.frame_time == 0) {
    return g_get_monotonic_time();
  }
  return AnimationState.frame_time;
}

void rofi_animation_set_epoch(gint64 epoch) { AnimationState.epoch = epoch; }

gint64 rofi_animation_get_epoch(void) { return AnimationState.epoch; }

void rofi_animation_set_frame_hook(void (*hook)(void)) {
  AnimationState.hook = hook;
}

static gboolean rofi_animation_frame_cb(G_GNUC_UNUSED gpointer data) {
  AnimationState.frame_source = 0;
  if (AnimationState.hook != NULL) {
    AnimationState.hook();
  }
  return G_SOURCE_REMOVE;
}

void rofi_animation_request_frame(void) {
  if (AnimationState.hook == NULL || AnimationState.frame_source != 0) {
    return;
  }
  AnimationState.frame_source =
      g_timeout_add_full(G_PRIORITY_HIGH_IDLE, rofi_animation_frame_interval(),
                         rofi_animation_frame_cb, NULL, NULL);
}

void rofi_spring_init(RofiSpring *s, RofiSpringSpec spec, double epsilon) {
  s->value = 0.0;
  s->velocity = 0.0;
  s->target = 0.0;
  s->spec = spec;
  s->epsilon = epsilon > 0.0 ? epsilon : 0.001;
  s->last_time = 0;
  s->initialized = FALSE;
}

void rofi_spring_snap(RofiSpring *s, double value) {
  s->value = value;
  s->target = value;
  s->velocity = 0.0;
  s->initialized = TRUE;
  s->last_time = 0;
}

gboolean rofi_spring_at_rest(const RofiSpring *s) {
  return s->value == s->target && s->velocity == 0.0;
}

void rofi_spring_set_target(RofiSpring *s, double target) {
  if (!s->initialized) {
    rofi_spring_snap(s, target);
    return;
  }
  if (rofi_spring_at_rest(s)) {
    // Start counting time from now, not from the last time it moved.
    s->last_time = 0;
  }
  s->target = target;
}

/**
 * Exact solution of x'' + 2 zeta w x' + w^2 x = 0 after dt,
 * x is the displacement from the target.
 */
static void spring_solve(RofiSpringSpec spec, double x0, double v0, double dt,
                         double *x, double *v) {
  const double w = sqrt(spec.stiffness);
  const double zeta = spec.damping;
  if (fabs(zeta - 1.0) < 1e-6) {
    const double c = v0 + w * x0;
    const double e = exp(-w * dt);
    *x = e * (x0 + c * dt);
    *v = e * (c - w * (x0 + c * dt));
  } else if (zeta < 1.0) {
    const double sigma = zeta * w;
    const double wd = w * sqrt(1.0 - zeta * zeta);
    const double b = (v0 + sigma * x0) / wd;
    const double e = exp(-sigma * dt);
    const double cs = cos(wd * dt);
    const double sn = sin(wd * dt);
    *x = e * (x0 * cs + b * sn);
    *v = e * ((b * wd - sigma * x0) * cs - (x0 * wd + sigma * b) * sn);
  } else {
    const double root = w * sqrt(zeta * zeta - 1.0);
    const double r1 = -zeta * w + root;
    const double r2 = -zeta * w - root;
    const double c2 = (v0 - r1 * x0) / (r2 - r1);
    const double c1 = x0 - c2;
    const double e1 = exp(r1 * dt);
    const double e2 = exp(r2 * dt);
    *x = c1 * e1 + c2 * e2;
    *v = c1 * r1 * e1 + c2 * r2 * e2;
  }
}

gboolean rofi_spring_step(RofiSpring *s, double dt) {
  if (rofi_spring_at_rest(s)) {
    return FALSE;
  }
  if (dt > 0.0) {
    double x = 0.0, v = 0.0;
    spring_solve(s->spec, s->value - s->target, s->velocity, dt, &x, &v);
    s->value = s->target + x;
    s->velocity = v;
  }
  if (fabs(s->value - s->target) < s->epsilon &&
      fabs(s->velocity) < s->epsilon * 10.0) {
    s->value = s->target;
    s->velocity = 0.0;
    return FALSE;
  }
  return TRUE;
}

gboolean rofi_spring_update(RofiSpring *s) {
  if (rofi_spring_at_rest(s)) {
    s->last_time = 0;
    return FALSE;
  }
  if (!rofi_animation_enabled()) {
    rofi_spring_snap(s, s->target);
    return FALSE;
  }
  gint64 now = rofi_animation_frame_time();
  if (s->last_time == 0) {
    // First frame of a movement: draw the start position.
    s->last_time = now;
    rofi_animation_request_frame();
    return TRUE;
  }
  double dt = (now - s->last_time) / (double)G_USEC_PER_SEC;
  s->last_time = now;
  dt = MIN(MAX(dt, 0.0), ANIMATION_MAX_STEP) * rofi_animation_speed();
  gboolean moving = rofi_spring_step(s, dt);
  if (moving) {
    rofi_animation_request_frame();
  } else {
    s->last_time = 0;
  }
  return moving;
}

double rofi_spring_step_response(RofiSpringSpec spec, double t) {
  if (t <= 0.0) {
    return 0.0;
  }
  double x = 0.0, v = 0.0;
  spring_solve(spec, -1.0, 0.0, t, &x, &v);
  return 1.0 + x;
}

double rofi_spring_settle_time(RofiSpringSpec spec) {
  const double w = sqrt(spec.stiffness);
  const double zeta = spec.damping;
  double decay = 0.0;
  if (zeta <= 1.0) {
    decay = zeta * w;
  } else {
    decay = w * (zeta - sqrt(zeta * zeta - 1.0));
  }
  if (decay <= 0.0) {
    return 10.0;
  }
  // ln(1000) ~ 6.9, the polynomial term of a critically damped spring
  // needs a bit more.
  return (zeta >= 1.0 ? 9.2 : 6.9) / decay;
}

void rofi_color_transition_init(RofiColorTransition *ct) {
  memset(ct, 0, sizeof(*ct));
  rofi_spring_init(&(ct->progress),
                   rofi_spring_spec(ROFI_SPRING_EFFECTS_DEFAULT), 0.002);
  rofi_spring_snap(&(ct->progress), 1.0);
}

static ThemeColor color_mix(const ThemeColor *a, const ThemeColor *b,
                            double t) {
  t = CLAMP(t, 0.0, 1.0);
  ThemeColor c = {
      .red = a->red + (b->red - a->red) * t,
      .green = a->green + (b->green - a->green) * t,
      .blue = a->blue + (b->blue - a->blue) * t,
      .alpha = a->alpha + (b->alpha - a->alpha) * t,
  };
  return c;
}

static gboolean color_equal(const ThemeColor *a, const ThemeColor *b) {
  return fabs(a->red - b->red) < 1e-4 && fabs(a->green - b->green) < 1e-4 &&
         fabs(a->blue - b->blue) < 1e-4 && fabs(a->alpha - b->alpha) < 1e-4;
}

ThemeColor rofi_color_transition_update(RofiColorTransition *ct,
                                        const ThemeColor *target) {
  if (!ct->initialized) {
    ct->from = *target;
    ct->to = *target;
    ct->initialized = TRUE;
    rofi_spring_snap(&(ct->progress), 1.0);
    return *target;
  }
  if (!color_equal(&(ct->to), target)) {
    // Start from what is visible now.
    ct->from = color_mix(&(ct->from), &(ct->to), ct->progress.value);
    ct->to = *target;
    rofi_spring_snap(&(ct->progress), 0.0);
    rofi_spring_set_target(&(ct->progress), 1.0);
  }
  rofi_spring_update(&(ct->progress));
  return color_mix(&(ct->from), &(ct->to), ct->progress.value);
}
