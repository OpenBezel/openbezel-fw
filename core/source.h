/* HDMI source state machine.
 *
 * A state machine is what makes it safe to ARM THE DISPLAY WATCHDOGS.
 *
 * The watchdog action blanks the panel in hardware the instant the input
 * misbehaves, so a source dropping out shows no flicker.  But
 * the forced-background state it leaves behind is self-sustaining (see
 * display.c): with force_en clear the timing generator waits for an input
 * trigger that the forced state itself prevents, so nothing short of software
 * intervention recovers, and an 8051 reset walks straight back into it (HDMI
 * then needs a power cycle).
 *
 * Arming is therefore only safe when a trip is a STATE TRANSITION: something
 * notices, disarms, and re-runs the bring-up.  That is this file.  The two go
 * together - do not arm the watchdogs anywhere else.
 *
 * Powersave rides on the same transitions: SOURCE_IDLE past a timeout
 * gates the backlight off, and anything that brings a source back - or any key
 * press - turns it on again.  The user's menu setting stays master; powersave
 * is only a gate on top of it (settings_backlight_gate).
 */
#ifndef OB_SOURCE_H
#define OB_SOURCE_H

#include "ob_types.h"
#include "hdmi_rx.h"

#define SOURCE_IDLE 0       /* no TMDS clock: free-run background, watchdogs disarmed */
#define SOURCE_LOCKING 1    /* clock seen: run the receiver bring-up */
#define SOURCE_SETTLING 2   /* receiver locked: wait for the measure block to agree */
#define SOURCE_ACTIVE 3     /* mode set done, picture up, watchdogs ARMED */

/* Resolve the state machine's timings against the calibrated timebase.  Call
 * after tb_calibrate(); until then the crystal-clock defaults apply. */
void source_init(void) __banked;

u8 source_state(void) __banked;
/* 0 = the trip valve has shut arming off, 1 = arming allowed but the picture
 * has not been stable long enough yet, 2 = armed right now.  Surfaced on the
 * debug console so a degraded mode is never silent. */
u8 source_armed(void) __banked;
/* Consecutive watchdog trips that did not hold a picture; reaching TRIP_LIMIT
 * is what closes source_armed().  On the debug console so a valve that fires
 * is diagnosable rather than just observable. */
u8 source_trips(void) __banked;
/* The settled measurement: the timing the last mode set was built from. */
const struct hdmi_timing *source_seen(void) __banked;

/* Boundary search taken before the mode set (which = 0) or ~1 s into ACTIVE
 * (which = 1); *ok = whether it completed plausibly. */
const struct hdmi_bound *source_bound(u8 which, u8 *ok) __banked;

/* How many times the ACTIVE-stage boundary search had to correct the geometry
 * a mode set used.  Should stay 0: non-zero means the pre-mode-set search is
 * failing somewhere and only the safety net is saving the picture. */
u8 source_remeasures(void) __banked;

/* STATUS0 / STATUS1 as they read at the last watchdog trip, to show which
 * armed condition fired.  STATUS0 bit 6 is IVS_ERROR and bit 5 IHS_ERROR,
 * which is what hv_err_wden watches; all-zero points at dvs_timeout instead. */
u8 source_trip_st0(void) __banked;
u8 source_trip_st1(void) __banked;

/* Hand the display to the state machine (entering UI_INPUT) or take it back
 * (leaving it).  source_detach() always disarms - the caller's display_freerun()
 * would too, but the order must never depend on that. */
void source_attach(void) __banked;
void source_detach(void) __banked;

/* Call from the main loop.  `enable` is "the state machine owns the display
 * right now", i.e. display_up && ui_mode == UI_INPUT; when it is 0 the machine
 * idles without touching the display, so the splash and debug screens keep
 * free-running deliberately. */
void source_tick(u16 t0ovf, u8 enable) __banked;

/* Any front-panel key: wakes the backlight out of powersave and restarts the
 * timer.  Without this, a menu opened over a blanked panel is invisible. */
void source_wake(u16 t0ovf) __banked;
u8 source_asleep(void) __banked;       /* powersave has the backlight off */
u8 source_picture(void) __banked;      /* a picture is up (rx_port_dp: which input) */
/* Redo the mode set on the next tick if ACTIVE (a setting that changes the
 * geometry); otherwise the next mode set picks it up anyway. */
void source_remode(void) __banked;
/* Frame-sync lead trim (lines) currently applied, and a manual override that
 * also stops the closed loop for this timing (DDC/CI FAh). */
s8 source_trim(void) __banked;
void source_trim_force(s8 t) __banked;

#define SOURCE_IN_HDMI 0
#define SOURCE_IN_DP   1
u8 source_input(void) __banked;        /* SOURCE_IN_*: the input on the panel */
void source_dp_arm(void) __banked;     /* DP (USB-C) automatic stream/mode set from now on */
u8 source_dp_show(u16 t0ovf) __banked; /* mode-set on the DP (USB-C) stream; 0 = ok */
u8 source_dp_video(void) __banked;     /* the USB-C port has steady video (shown or not) */

#endif /* OB_SOURCE_H */
