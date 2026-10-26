/* Backlight dimming: PWM5:
 *   duty       PWM5H_DUT = 0xFF - level  - 8-bit, INVERTED
 *   frequency  360 Hz, f = clk / ((N+1) << M) / 256
 * Below 1 kHz we also enable PWM5's reset on display VSYNC, so the PWM is
 * frame-locked (exactly 6 periods per frame at 60 Hz) rather than beating
 * against the refresh.
 *
 * The inversion is why the backlight comes up at full brightness with PWM5
 * untouched: a disabled PWM idles like zero duty, which is full on.
 *
 * The on/off enable GPIO (BOARD_BACKLIGHT_OFF) is separate and stays master -
 * settings.c combines the two. */
#ifndef RTD2556_BACKLIGHT_H
#define RTD2556_BACKLIGHT_H

#include "ob_types.h"

void backlight_pwm_init(void);          /* PWM5 on M2PLL, 360 Hz, 8-bit duty, VSYNC reset */
void backlight_pwm_level(u8 pct);       /* 0..100 %; 100 = full */

#endif
