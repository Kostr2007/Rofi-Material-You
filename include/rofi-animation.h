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

#ifndef ROFI_ANIMATION_H
#define ROFI_ANIMATION_H

#include "rofi-types.h"
#include <glib.h>

/**
 * @defgroup ANIMATION Animation
 * @ingroup HELPERS
 *
 * Spring based motion, modelled after the Material 3 Expressive motion
 * physics system. Every animated value is a damped harmonic oscillator
 * (mass 1) that is moved towards a target. Springs are advanced lazily from
 * the draw code: a widget calls rofi_spring_update() while painting and, if
 * the spring has not settled yet, a new frame is requested automatically.
 *
 * @{
 */

/**
 * Material 3 motion schemes.
 */
typedef enum {
  /** Bouncy springs with visible overshoot (M3 Expressive). */
  ROFI_MOTION_EXPRESSIVE = 0,
  /** Calmer, well damped springs (M3 standard). */
  ROFI_MOTION_STANDARD = 1,
} RofiMotionScheme;

/**
 * Motion tokens. Spatial springs move things (position, size, shape),
 * effects springs change looks (opacity, color) and never overshoot.
 */
typedef enum {
  ROFI_SPRING_SPATIAL_FAST = 0,
  ROFI_SPRING_SPATIAL_DEFAULT,
  ROFI_SPRING_SPATIAL_SLOW,
  ROFI_SPRING_EFFECTS_FAST,
  ROFI_SPRING_EFFECTS_DEFAULT,
  ROFI_SPRING_EFFECTS_SLOW,
  /** Number of tokens. */
  ROFI_SPRING_TOKEN_NUM,
} RofiSpringToken;

/**
 * Physical description of a spring.
 */
typedef struct {
  /** Stiffness (k), mass is fixed to 1. */
  double stiffness;
  /** Damping ratio (zeta), 1.0 is critically damped. */
  double damping;
} RofiSpringSpec;

/**
 * An animated value.
 */
typedef struct {
  /** Current value. */
  double value;
  /** Current velocity in units per second. */
  double velocity;
  /** Value the spring moves towards. */
  double target;
  /** The spring constants. */
  RofiSpringSpec spec;
  /** Distance to target (and velocity / 10) below which the spring rests. */
  double epsilon;
  /** Monotonic time (us) the spring was last advanced to. */
  gint64 last_time;
  /** If the spring has a value yet, the first target is snapped to. */
  gboolean initialized;
} RofiSpring;

/**
 * @param scheme The motion scheme to use.
 * @param token  The motion token.
 *
 * @returns the spring constants for the token in the given scheme.
 */
RofiSpringSpec rofi_spring_token_spec(RofiMotionScheme scheme,
                                      RofiSpringToken token);

/**
 * @param token The motion token.
 *
 * @returns the spring constants for the token in the configured scheme.
 */
RofiSpringSpec rofi_spring_spec(RofiSpringToken token);

/**
 * @param name     Token name ("spatial-fast", "effects-default", ...) or two
 *                 numbers "<stiffness> <damping>", e.g. "380 0.8".
 * @param fallback Token used when name is NULL or invalid.
 *
 * @returns the parsed spring constants.
 */
RofiSpringSpec rofi_spring_spec_parse(const char *name,
                                      RofiSpringToken fallback);

/**
 * @param name   The name of the scheme ("expressive" or "standard").
 * @param scheme Set to the parsed scheme.
 *
 * @returns TRUE if the name is valid.
 */
gboolean rofi_motion_scheme_parse(const char *name, RofiMotionScheme *scheme);

/**
 * Re-read the animation settings from the configuration and the
 * environment (ROFI_NO_ANIMATION). Called lazily on first use.
 */
void rofi_animation_reload_settings(void);

/**
 * @returns TRUE if animations are enabled.
 */
gboolean rofi_animation_enabled(void);

/**
 * @returns the configured motion scheme.
 */
RofiMotionScheme rofi_animation_motion_scheme(void);

/**
 * @returns the configured animation speed factor (1.0 is normal).
 */
double rofi_animation_speed(void);

/**
 * @returns the configured frame interval in milliseconds.
 */
guint rofi_animation_frame_interval(void);

/**
 * Mark the start of a new frame, all springs updated in this frame use the
 * same timestamp.
 */
void rofi_animation_begin_frame(void);

/**
 * @returns the timestamp (us) of the current frame.
 */
gint64 rofi_animation_frame_time(void);

/**
 * @param epoch Monotonic time (us) the view became visible, 0 to reset.
 *
 * Entrance animations count from this moment, so they are not spent on
 * frames drawn before the window is on screen.
 */
void rofi_animation_set_epoch(gint64 epoch);

/**
 * @returns the time the view became visible, 0 if it is not visible yet.
 */
gint64 rofi_animation_get_epoch(void);

/**
 * @param hook Function that schedules a redraw of the active view.
 *
 * The view code installs the hook, so the animation code does not depend
 * on a display backend.
 */
void rofi_animation_set_frame_hook(void (*hook)(void));

/**
 * Request a new frame to be drawn.
 */
void rofi_animation_request_frame(void);

/**
 * @param s       The spring.
 * @param spec    The spring constants.
 * @param epsilon Rest threshold.
 *
 * Initialize a spring without a value, the first target is snapped to.
 */
void rofi_spring_init(RofiSpring *s, RofiSpringSpec spec, double epsilon);

/**
 * @param s      The spring.
 * @param target New target.
 *
 * Move the spring to a new target, keeping the current velocity.
 */
void rofi_spring_set_target(RofiSpring *s, double target);

/**
 * @param s     The spring.
 * @param value The value.
 *
 * Jump to value and stop moving.
 */
void rofi_spring_snap(RofiSpring *s, double value);

/**
 * @param s  The spring.
 * @param dt Time step in seconds.
 *
 * Advance the spring by dt using the exact solution of the damped harmonic
 * oscillator, so it is stable for any time step.
 *
 * @returns TRUE if the spring is still moving.
 */
gboolean rofi_spring_step(RofiSpring *s, double dt);

/**
 * @param s The spring.
 *
 * Advance the spring to the current frame time, requesting a new frame
 * when it did not settle. When animations are disabled it snaps.
 *
 * @returns TRUE if the spring is still moving.
 */
gboolean rofi_spring_update(RofiSpring *s);

/**
 * @param s The spring.
 *
 * @returns TRUE if the spring is at rest on its target.
 */
gboolean rofi_spring_at_rest(const RofiSpring *s);

/**
 * @param spec The spring constants.
 * @param t    Time in seconds since the start.
 *
 * Position of a spring released at 0 towards 1 at time t. Used for
 * stateless (staggered) entrance animations.
 *
 * @returns the position, can overshoot 1.0.
 */
double rofi_spring_step_response(RofiSpringSpec spec, double t);

/**
 * @param spec The spring constants.
 *
 * @returns an estimate of the time in seconds the step response needs to
 * settle within 0.1% of the target.
 */
double rofi_spring_settle_time(RofiSpringSpec spec);

/**
 * A color that cross-fades to a new value when its target changes.
 */
typedef struct {
  /** Color the fade started from. */
  ThemeColor from;
  /** Color the fade goes to. */
  ThemeColor to;
  /** Progress of the fade, 0 to 1. */
  RofiSpring progress;
  /** If a color was set already. */
  gboolean initialized;
} RofiColorTransition;

/**
 * @param ct The color transition.
 *
 * Initialize, using the effects-default spring.
 */
void rofi_color_transition_init(RofiColorTransition *ct);

/**
 * @param ct     The color transition.
 * @param target The color to fade to.
 *
 * Retarget (only when the color changed) and advance the fade.
 *
 * @returns the color to draw with in this frame.
 */
ThemeColor rofi_color_transition_update(RofiColorTransition *ct,
                                        const ThemeColor *target);

/**@}*/
#endif // ROFI_ANIMATION_H
