/* HDMI source state machine - see source.h. */
#include "source.h"
#include "settings.h"
#include "menu.h"
#include "display.h"
#include "hdmi_rx.h"
#include "audio.h"
#include "dp_rx.h"
#include "delay.h"
#include "rtd2556_regs.h"
#include "timebase.h"

/* Durations are in MILLISECONDS and resolved through tb_ms() in source_init():
 * a Timer0 overflow is 29 ms on the crystal but 8.59 ms once the PLL is
 * running, so raw overflow counts would be wrong - see timebase.h. */
#define TICK_MS 116             /* how often the machine looks at the world */
#define SETTLE_TICKS 2          /* consecutive IDENTICAL measurements before the mode set */
#define SETTLE_GIVEUP_MS 3000   /* a locked receiver that never settles */
#define ARM_GRACE_MS 1000       /* let the display settle before trusting a trip */
#define ACTIVE_SETTLED_MS 4000  /* good picture for this long clears the trip counter */
#define TRIP_LIMIT 3            /* consecutive quick trips before we stop arming */
#define SETTLE_FAIL_LIMIT 2     /* give-ups before we mode-set on an unsettled measurement */
#define VALVE_REOPEN_MAX 2      /* times a long-stable picture may re-open the trip valve */
/* TMDS count agreement required before (and ~1 s after) a mode set: 1/256 =
 * 0.4% (21 counts at 1080p), never less than COUNT_TOL_MIN.  The detector
 * jitters by a few counts at ANY clock, so a pure percentage is too tight for
 * slow modes (0.2% would be 4 counts at 1024x768, inside the jitter).  The
 * receiver's +-1/64 tracking window is far too loose for this: the display
 * clock is COMPUTED from the count, and a 1.5% step (1680x1050p60 CVT ->
 * 1080p60) stays inside it while giving a 59.1 Hz display clock against a
 * 60 Hz source. */
#define COUNT_TOL_SHIFT 8
#define COUNT_TOL_MIN 6
#define MISMATCH_LIMIT 3
/* Frame-sync lead trim (closed loop).  About 1 s into ACTIVE, STATUS0 tells
 * us whether the display's reader is overtaking the capture (bit 0, line
 * buffer underflow: add a line of lead) or falling too far behind it (bit 1,
 * FIFO overflow: remove one), and the mode set is redone.  When the picture
 * is scaled vertically the clean window is only 1-2 lines wide, and where it
 * sits differs per mode by about the size of the calculation's own error, so
 * no fixed correction fits.  The trim is kept while the same timing is mode-set again and reset
 * on a new one.  A search that never comes out clean goes back to 0. */
#define TRIM_LIMIT 3            /* lines either way */
/* Pixel clock (htotal x vtotal x refresh, refresh in 0.1 Hz) below which the
 * trim is not searched: the 25-27 MHz modes (640x480, 720x480, 720x576)
 * underflow at every trim from -3 to +3 with the picture correct only at 0,
 * while 800x600 (40 MHz) and up are clean at 0 or +1. */
#define TRIM_PCLK_MIN_X10 300000000UL   /* 30 MHz */
#define TRIM_TRIES 4            /* mode sets spent searching, per timing */
#define TRIM_CLEAR_TICK 4       /* clear STATUS0 here (~0.5 s in: past the lock transient) ... */

/* The above in ticks/overflows once the timebase is calibrated.  Defaults are
 * the crystal-clock figures, used only if source_init() never runs. */
static u16 tick_ovf = 4;
static u8 settle_giveup = 26;
static u8 arm_grace = 9;
static u8 active_settled = 34;

void source_init(void) __banked
{
    u16 per_tick;

    tick_ovf = tb_ms(TICK_MS);
    per_tick = tick_ovf ? tick_ovf : 1;
    /* these three count state-machine iterations, not overflows */
    settle_giveup = (u8)(tb_ms(SETTLE_GIVEUP_MS) / per_tick);
    arm_grace = (u8)(tb_ms(ARM_GRACE_MS) / per_tick);
    active_settled = (u8)(tb_ms(ACTIVE_SETTLED_MS) / per_tick);
    if (!settle_giveup) settle_giveup = 1;
    if (!arm_grace) arm_grace = 1;
    if (!active_settled) active_settled = 1;
}

static u8 state;
static u8 stable;               /* consecutive matching measurements in SETTLING */
static struct hdmi_timing seen;  /* the last measurement, to compare the next against */
static u8 waited;               /* ticks spent in SETTLING, for the give-up */
static u8 held;                 /* ticks survived in ACTIVE: arming and trip accounting */
static u8 armed;                /* the watchdog actions are currently armed */
static u8 reopens;              /* times the trip valve has been given another chance */
static u8 trips;                /* consecutive trips that did not survive active_settled */
static u8 trip_st0, trip_st1;   /* STATUS0/1 captured at the last trip, for diagnosis */
static u8 mismatch;             /* consecutive ACTIVE ticks whose timing disagrees with `seen` */
/* Boundary search results.  [0] is taken in SETTLING just before the mode set,
 * and - when it succeeds - DRIVES the capture geometry
 * (hdmi_timing_from_bound).  [1] is a second reading ~1 s into ACTIVE, kept
 * as a cross-check on the console: the two should always agree. */
static struct hdmi_bound bound[2];
static u8 bound_ok[2];
static u8 remode;               /* source_remode(): redo the mode set next tick */
static s8 trim;                 /* frame-sync lead trim, lines - see TRIM_LIMIT */
static u8 trim_tries;
static u16 trim_htot, trim_vtot; /* the timing `trim` belongs to */
static u8 remeasures;           /* times the ACTIVE-stage search corrected the geometry */
static u8 settle_fails;         /* consecutive SETTLING give-ups - see the valve below */
static u8 arm_ok = 1;           /* safety valve - see below */
static u16 last_tick;
static u16 idle_since;          /* t0ovf when SOURCE_IDLE began (powersave timer) */
static u8 blanked;              /* powersave has the backlight gated off */
#define NOSIGNAL_MS 2000        /* no picture this long: say so ("No Signal") */
static __xdata u8 nosignal_shown;
static __xdata u8 fast_blank;   /* source_tick's fast path blanked: re-settle next tick */
static __xdata u8 fast_blanks;  /* times it fired (debug, read from RAM) */
static __xdata u8 mode_sets;    /* mode_set() calls (debug, read from RAM) */
static __xdata u8 why = 1;      /* what led to the next mode_set (debug) */
static __xdata u8 why_log[4];   /* ... the last four, newest first */

u8 source_state(void) __banked { return state; }
const struct hdmi_timing *source_seen(void) __banked { return &seen; }

#ifdef CONFIG_DEBUG_VCP
s8 source_trim(void) __banked { return trim; }
void source_trim_force(s8 t) __banked
{
    trim = t;
    trim_tries = TRIM_TRIES;        /* manual: the loop keeps its hands off */
    source_remode();
}
#endif

#ifdef CONFIG_DEBUG_CONSOLE
u8 source_trips(void) __banked { return trips; }
u8 source_trip_st0(void) __banked { return trip_st0; }
u8 source_trip_st1(void) __banked { return trip_st1; }
u8 source_remeasures(void) __banked { return remeasures; }

const struct hdmi_bound *source_bound(u8 which, u8 *ok) __banked
{
    *ok = bound_ok[which];
    return &bound[which];
}

/* 0 = the valve is closed, 1 = arming allowed but not armed yet (the picture
 * has not been stable long enough), 2 = armed right now. */
u8 source_armed(void) __banked { return armed ? 2 : (arm_ok ? 1 : 0); }
#endif

/* Safety valve.  If arming the watchdogs makes the picture trip straight back
 * out, arming again just loops - reconfiguring the input every few hundred ms
 * (a mode popup flashing every half second).  After TRIP_LIMIT trips that
 * never held a picture for active_settled, stop arming and run with the
 * actions off: a visible flicker on source loss rather than an unusable
 * display.  A picture that survives clears the counter.
 *
 * This degrades rather than recovering, deliberately.  If it ever fires in the
 * field the arming logic is wrong and wants fixing, not retrying - and
 * source_armed() makes it visible on the debug console instead of silent. */

/* Powersave delay presets in milliseconds; index 0 is off and never reaches
 * here.  5 s / 30 s / 2 min, as on the menu.  2 min does not fit a u16 of milliseconds, hence the 0 and the special case
 * in powersave_tick(). */
static const __code u16 powersave_ms[] = { 0, 5000, 30000, 0 };

static void backlight_wake(u16 t0ovf)
{
    idle_since = t0ovf;
    nosignal_shown = 0;             /* a key press on a dark screen: tell them again */
    if (blanked) {
        blanked = 0;
        settings_backlight_gate(1);
    }
}

/* Leave the active path for any reason.  Order is not negotiable: DISARM
 * FIRST.  The watchdog action forces the background and clears force_en, and
 * that state feeds itself - reprogramming the display while it is latched
 * leaves it stuck, even across an 8051 reset.  display_freerun() also writes
 * WATCH_DOG_CTRL0 = 0, but this must not depend on that. */
static void go_idle(u16 t0ovf, u8 enable)
{
    display_watchdogs_disarm();
    audio_hold();                   /* source gone: mute before the receiver restarts */
    if (enable)
        display_freerun();
    hdmi_stat.result = HDMI_ERR_NO_CLOCK;
    state = SOURCE_IDLE;
    stable = 0;
    waited = 0;
    armed = 0;
    fast_blank = 0;
    idle_since = t0ovf;
}

/* The watchdog ACTION is what we care about, and it is directly observable:
 * it sets force_bg_en, which hdmi_input_mode() had released.  WATCH_DOG_CTRL0
 * bit 0 is NOT used - it is read-only status, only meaningful while its enable
 * is set, and it reads stale-high otherwise.  Watch the effect, not the flag
 * that claims to describe it. */
static u8 watchdog_tripped(void)
{
    return (SR(VIDEO_DISPLAY_CONTROL_REG) & VIDEO_DISPLAY_CONTROL_REG_FORCE_BG_EN) ? 1 : 0;
}

/* Has the live TMDS count moved away from the one the receiver was locked
 * (and the display clock computed) with? */
static u8 count_drifted(void)
{
    u16 c = hdmi_freq_count(), s0 = hdmi_stat.tmds_count;
    u16 d = (c > s0) ? c - s0 : s0 - c;
    u16 tol = s0 >> COUNT_TOL_SHIFT;

    if (tol < COUNT_TOL_MIN)
        tol = COUNT_TOL_MIN;
    return (d > tol) ? 1 : 0;
}

static void mode_set(u16 t0ovf)
{
    fast_blank = 0;
    remode = 0;                     /* a pending source_remode() is satisfied by this one
                                     * (settings_apply_all() at boot requests one before
                                     * there is a picture) */
    mode_sets++;
    why_log[3] = why_log[2]; why_log[2] = why_log[1]; why_log[1] = why_log[0];
    why_log[0] = why;
    why = 1;
    /* `seen` holds the settled measurement - the timing SETTLING saw twice
     * running - so the capture window comes from what the input actually is
     * rather than from the table's fixed 1080p geometry. */
    if (seen.htotal != trim_htot || seen.vtotal != trim_vtot) {
        trim = 0;                   /* a new timing starts from the calculation */
        /* No search below TRIM_PCLK_MIN: there the underflow flag is set at
         * every trim, and each step it would try shows ~1 s of skewed picture. */
        trim_tries = (u32)seen.htotal * seen.vtotal * seen.refresh_x10 < TRIM_PCLK_MIN_X10
                   ? TRIM_TRIES : 0;
        trim_htot = seen.htotal;
        trim_vtot = seen.vtotal;
    }
    hdmi_fs_extra_lines = trim;
    if (seen.active_ok)
        hdmi_input_mode(&seen);
    else
        hdmi_input_mode(0);         /* unrecognized timing: leave the table's geometry */
    settings_apply_levels();        /* hdmi_input_mode() resets the color blocks */
    settings_apply_saturation();
    /* ... and reloads the scale-up coefficients and input dither at their
     * DEFAULTS (pipeline_tables_load): re-apply the user's sharpness and
     * input dither. */
    settings_apply_sharpness();
    settings_apply_idither();
    /* The receiver's audio side (PLL, FIFO, tracking) restarts with every
     * mode set: the receiver bring-up before it resets it.  Its automatic
     * output flow turns the sound on once it detects audio.  The outputs stay
     * muted through it (audio_tick() unmutes): the restart pops.  NOT when
     * audio is already running: the redos ~1 s in (lead trim, aspect, the
     * geometry safety net) re-mode the display only, and restarting the
     * receiver's audio there would pop just after the sound came on. */
    if (rx_port_dp) {
        audio_hold();                   /* DP: D1's audio FSM runs with the stream */
        audio_route(1);
    } else {
        audio_route(0);
        if (!hdmi_audio_rate()) {
            audio_hold();
            hdmi_audio_start();
        }
    }
    menu_popup_mode(t0ovf);         /* a setting; a no-op when off or when the menu is open */
    backlight_wake(t0ovf);
    held = 0;
    armed = 0;                      /* armed later, once the picture proves stable */
    mismatch = 0;
    if (!rx_port_dp)
        hdmi_clock_watch();         /* the fast path watches from the picture on */
    state = SOURCE_ACTIVE;
}

/* Entering SETTLING has to start the measure block: the mode set is gated on a
 * measurement, so the measurement has to be running before the gate is read. */
static void enter_settling(void)
{
    hdmi_measure_start();
    state = SOURCE_SETTLING;
    stable = 0;
    waited = 0;
    armed = 0;
}

void source_attach(void) __banked
{
    /* If the receiver is already locked (the usual case coming off the splash
     * screen) skip straight to settling - re-running the bring-up would only
     * add latency to first picture. */
    armed = 0;
    if (hdmi_stat.result == HDMI_OK) {
        enter_settling();
        return;
    }
    state = SOURCE_IDLE;
    stable = 0;
    waited = 0;
}

void source_detach(void) __banked
{
    display_watchdogs_disarm();
    stable = 0;
    waited = 0;
    armed = 0;
    /* `state` is deliberately left alone.  The debug console is the only place
     * this is visible, and switching to it detaches - resetting the state here
     * would make the console only ever report IDLE. */
}

void source_remode(void) __banked
{
    remode = 1;
}

void source_wake(u16 t0ovf) __banked
{
    backlight_wake(t0ovf);
}

u8 source_asleep(void) __banked
{
    return blanked;
}


/* Called every tick with whether a picture is up, whichever input owns the
 * display, so an input with nothing plugged in still lets the backlight go
 * off. */
static void powersave_tick(u16 t0ovf, u8 up)
{
    u16 limit;

    if (up) {
        if (nosignal_shown)
            menu_popup_nosignal_off();
        nosignal_shown = 0;
        return;                     /* a picture is up: nothing to save */
    }
    if (!nosignal_shown && !blanked && (u16)(t0ovf - idle_since) >= tb_ms(NOSIGNAL_MS)) {
        nosignal_shown = 1;
        menu_popup_nosignal(t0ovf);
    }
    if (ob_settings.powersave == 0 || blanked)
        return;
    if (ob_settings.powersave == 3)
        limit = (u16)(tb_ms(60000) * 2);        /* 2 min, in two u16-safe steps */
    else
        limit = tb_ms(powersave_ms[ob_settings.powersave < SETTINGS_POWERSAVE_COUNT
                                   ? ob_settings.powersave : SETTINGS_POWERSAVE_DEFAULT]);
    if ((u16)(t0ovf - idle_since) < limit)
        return;
    blanked = 1;
    settings_backlight_gate(0);
}

/* DisplayPort (USB-C) mode set (also the FFh = 7 debug stage): take the
 * display from the HDMI state machine and mode-set on the D1 stream.  The timing
 * comes from the page-30 measure block on D1's syncs, the pixel clock from
 * the D1 frame-time measurement (dp_pixel_count).  0 = shown. */
static u8 dp_owns;

u8 source_dp_show(u16 t0ovf) __banked
{
    struct hdmi_timing t;
    u16 c = dp_pixel_count();

    if (!c)
        return 1;
    dp_owns = 1;
    display_watchdogs_disarm();
    rx_port_dp = 1;
    hdmi_stat.tmds_count = c;
    hdmi_stat.f_tmds = (u16)(((u32)c * 27UL + 50UL) / 100UL);
    hdmi_measure_start();
    delay_ms(150);
    if (!hdmi_measure(&t))
        return 2;
    /* Geometry as the HDMI path gets it (the boundary search on DE, which
     * D1's timing generator regenerates cleanly); failing that, the DPF's
     * own: active from HTT-HWD-8 / VTT-VHT-4, sized by the MSA.  Not the
     * standard-mode table, which does not cover every DP source mode (a
     * phone's 1280x960, total 1800x1000). */
    bound_ok[0] = hdmi_boundary_search(&bound[0]);
    if (!bound_ok[0]) {
        bound[0].hs = dp_log.msa_htt - dp_log.msa_hwd - 8;
        bound[0].he = bound[0].hs + dp_log.msa_hwd - 1;
        bound[0].vs = dp_log.msa_vtt - dp_log.msa_vht - 4;
        bound[0].ve = bound[0].vs + dp_log.msa_vht - 1;
    }
    hdmi_timing_from_bound(&t, &bound[0]);
    t.refresh_x10 = (u16)(270000000UL / dp_log.frame27);    /* 27 MHz x 10 / cycles per frame */
    seen = t;
    why = 10;
    mode_set(t0ovf);
    return 0;
}

/* DisplayPort (USB-C) path (armed at boot once the port is started, or by
 * the FFh = 8 debug stage): the stream comes up whenever the link has steady video AND the
 * input selection shows DP; it is torn down when it goes.  The HDMI state
 * machine stays off while DP owns the display. */
#define DPS_OFF   0
#define DPS_WAIT  1     /* link / video / a clean stream */
#define DPS_SHOWN 2
static u8 dp_st;
static u8 dp_miss;
static u8 dp_backoff;
static u8 dp_steady;            /* ticks of video present before the setup */
static u8 dp_gen;              /* dp_log.lt_gen the stream was set up for */
static u8 dp_avail;            /* the port has steady video (shown or not) */
static u8 dp_avail_prev;
static u8 cur_in;              /* SOURCE_IN_*: the input on the panel */

void source_dp_arm(void) __banked
{
    dp_st = DPS_WAIT;
    dp_miss = 0;
}

u8 source_input(void) __banked { return cur_in; }

u8 source_dp_video(void) __banked { return dp_avail; }

/* enable: the input view owns the display - only then is the stream shown */
static void dp_tick(u16 t0ovf, u8 enable)
{
    u8 r;

    if (dp_st == DPS_WAIT) {
        /* Not shown.  The link clock is the only live unplug sign (link
         * state, BE, VB-ID and the MSA all hold): a trained link whose
         * clock free-runs is a pulled cable - forget the training so the
         * re-plugged source retrains. */
        r = dp_link_clk_nominal();
        if (r == 0 && dp_link_up()) {
            dp_link_reset();
            dp_log.losses++;
        }
        if (r != 1 || dp_stream_present() != DP_ALIVE) {
            dp_steady = 0;
            dp_avail = 0;
            return;
        }
        /* video steadily present first: a stream set up the moment training
         * ends locks to the source's start-up and comes up black or
         * vertically shifted */
        if (dp_steady < 3) {
            dp_steady++;
            return;
        }
        dp_avail = 1;
        if (cur_in != SOURCE_IN_DP || !enable)
            return;
        /* the stream setup blocks for ~0.7 s; a failing one must not
         * monopolise the main loop (DDC/CI, keys): one attempt per ~2 s */
        if (dp_backoff) {
            dp_backoff--;
            return;
        }
        r = dp_stream_start();
        dp_log.stage3 = r;
        if (r) {
            dp_backoff = 16;            /* ticks of 116 ms */
            return;
        }
        dp_gen = dp_log.lt_gen;
        r = source_dp_show(t0ovf);
        dp_log.stage3 = 0x10 | r;
        if (!r) {
            dp_st = DPS_SHOWN;
            dp_miss = 0;
        }
    } else if (dp_st == DPS_SHOWN) {
        if (dp_log.lt_gen != dp_gen) {
            /* retrained since the stream was set up (a quick re-plug, or
             * the source retraining): the stream PLL / DPF / frame sync
             * are stale even if the timing is the same (the picture comes
             * back shifted).  Redo it. */
            display_watchdogs_disarm();
            display_freerun();
            dp_log.losses++;
            dp_st = DPS_WAIT;
            return;
        }
        r = dp_stream_alive();
        dp_log.alive = r;
        if (r == DP_ALIVE) {
            dp_miss = 0;
            return;
        }
        if (++dp_miss < 3)
            return;                     /* ~350 ms of loss before acting */
        display_watchdogs_disarm();
        display_freerun();              /* black, display self-timed */
        if (r == DP_LINK_LOST)
            dp_link_reset();            /* a re-plugged source must retrain */
        dp_log.losses++;
        dp_avail = 0;
        dp_steady = 0;
        dp_st = DPS_WAIT;
    }
}

/* Input selection (setting Input, MCCS 60h).  "Prefer X" shows X whenever it
 * has video, else the other; HDMI / USB-C are fixed.  Auto
 * keeps the input on the panel while it has a signal, moves when it loses
 * it and the other has one, and moves to USB-C when a phone newly appears
 * there - a plug-in is intent, the PC's HDMI waking from DPMS is not. */
static void input_select(u16 t0ovf)
{
    u8 want = cur_in, hdmi = hdmi_clock_present();
    u8 sel = dp_st == DPS_OFF ? SETTINGS_INPUT_HDMI : ob_settings.input;

    if (sel == SETTINGS_INPUT_HDMI)
        want = SOURCE_IN_HDMI;
    else if (sel == SETTINGS_INPUT_USBC)
        want = SOURCE_IN_DP;
    else if (sel == SETTINGS_INPUT_PREF_HDMI)
        want = hdmi ? SOURCE_IN_HDMI : dp_avail ? SOURCE_IN_DP : cur_in;
    else if (sel == SETTINGS_INPUT_PREF_USBC)
        want = dp_avail ? SOURCE_IN_DP : hdmi ? SOURCE_IN_HDMI : cur_in;
    else if (dp_avail && !dp_avail_prev)
        want = SOURCE_IN_DP;
    else if (cur_in == SOURCE_IN_HDMI && !hdmi && dp_avail)
        want = SOURCE_IN_DP;
    else if (cur_in == SOURCE_IN_DP && !dp_avail && hdmi)
        want = SOURCE_IN_HDMI;
    dp_avail_prev = dp_avail;
    if (want == cur_in)
        return;
    cur_in = want;
    display_watchdogs_disarm();
    display_freerun();
    if (want == SOURCE_IN_DP) {
        dp_owns = 1;                    /* HDMI state machine off; dp_tick shows it */
    } else {
        if (dp_st == DPS_SHOWN)
            dp_st = DPS_WAIT;           /* keep the link, stop showing it */
        dp_owns = 0;
        rx_port_dp = 0;
        go_idle(t0ovf, 1);              /* the HDMI state machine brings it up again */
    }
}

void source_tick(u16 t0ovf, u8 enable) __banked
{
    struct hdmi_timing t;
    u8 clk;

    /* Fast path, every call: the TMDS clock leaving its window means the
     * source is changing mode or going away.  Black NOW - the tick-paced
     * timing check below takes MISMATCH_LIMIT ticks, and the picture tears into
     * noise meanwhile - and let the next tick re-settle (on a false alarm that
     * is a re-mode-set, not a dark screen). */
    if (state == SOURCE_ACTIVE && enable && !dp_owns && !fast_blank && hdmi_clock_moved()) {
        display_force_bg(1);
        audio_hold();
        fast_blank = 1;
        fast_blanks++;
    }
    if ((u16)(t0ovf - last_tick) < tick_ovf)
        return;
    last_tick = t0ovf;
    if (dp_st != DPS_OFF) {
        dp_tick(t0ovf, enable);         /* link upkeep and dp_avail even while not shown */
        if (enable)
            input_select(t0ovf);        /* reprograms the display: only when the input view owns it */
    }
    if (!enable) {
        /* Someone else owns the display (splash, debug console).  Keep the
         * powersave timer honest but touch nothing. */
        idle_since = t0ovf;
        return;
    }
    if (dp_owns) {                  /* DP owns the display: dp_tick shows it */
        powersave_tick(t0ovf, dp_st == DPS_SHOWN);
        return;
    }

    clk = hdmi_clock_present();

    switch (state) {
    case SOURCE_IDLE:
        if (clk)
            state = SOURCE_LOCKING;
        break;

    case SOURCE_LOCKING:
        if (!clk) {
            go_idle(t0ovf, enable);
            break;
        }
        display_watchdogs_disarm();     /* never reprogram with an action latched */
        if (hdmi_rx_start() == HDMI_OK)
            enter_settling();
        break;                          /* otherwise retry on the next tick */

    case SOURCE_SETTLING:
        if (!clk) {
            go_idle(t0ovf, enable);
            break;
        }
        /* "Settled" means the SAME timing twice running, not merely a valid
         * reading twice running - a source still changing mode produces a
         * sequence of individually-plausible frames, and mode-setting in the
         * middle of that is how you get a picture built on timing that has
         * already moved on. */
        if (hdmi_measure(&t)) {
            if (stable && t.htotal == seen.htotal && t.vtotal == seen.vtotal) {
                if (++stable > SETTLE_TICKS) {
                    /* A source can change mode WITHOUT dropping its clock, and
                     * then this state is reached by the ACTIVE timing-mismatch
                     * path, not via LOCKING - so the receiver is still banded,
                     * and hdmi_stat.tmds_count still measured, for the OLD
                     * frequency, and a display clock computed from that stale
                     * count underflows the FIFO.  If the live count has moved
                     * beyond COUNT_TOL, re-lock. */
                    if (count_drifted()) {
                        state = SOURCE_LOCKING;
                        break;
                    }
                    bound_ok[0] = hdmi_boundary_search(&bound[0]);
                    if (bound_ok[0])
                        hdmi_timing_from_bound(&seen, &bound[0]);
                    mode_set(t0ovf);
                    break;
                }
            } else {
                stable = 1;
            }
            seen = t;
        } else {
            stable = 0;
        }
        if (++waited >= settle_giveup) {
            /* Second valve, same principle as arm_ok: a locked receiver whose
             * measurement never settles must not leave the panel dark forever.
             * After a couple of give-ups, mode-set on the fixed table anyway:
             * the floor is "some picture", never "no picture".  Garbage
             * timing then shows as garbage, which is at least diagnosable. */
            if (++settle_fails >= SETTLE_FAIL_LIMIT) {
                why = 2;
                mode_set(t0ovf);
            }
            else
                go_idle(t0ovf, enable);
        }
        break;

    default:                            /* SOURCE_ACTIVE */
        if (!clk) {
            go_idle(t0ovf, enable);     /* a real source loss, not a watchdog fault */
            break;
        }
        if (fast_blank) {               /* the clock moved: re-measure, re-mode-set */
            fast_blank = 0;
            display_watchdogs_disarm();
            display_freerun();
            armed = 0;
            why = 9;
            state = SOURCE_LOCKING;
            break;
        }
        if (remode) {                   /* same teardown as the geometry safety net below */
            remode = 0;
            display_watchdogs_disarm();
            display_freerun();
            armed = 0;
            why = 3;
            mode_set(t0ovf);
            break;
        }
        if (held < 0xFF)
            held++;
        /* The source may change its picture aspect without changing timing (a
         * player switching between 4:3 and 16:9 material): the AVI InfoFrame
         * is the only sign.  Only matters when Fit is pillarboxing by it. */
        if (held > TRIM_CLEAR_TICK && hdmi_avi_aspect() != hdmi_ar_used && hdmi_window_stale(&seen)) {
            display_watchdogs_disarm();
            display_freerun();
            armed = 0;
            why = 4;
            mode_set(t0ovf);
            break;
        }
        if (held == TRIM_CLEAR_TICK)
            SR(STATUS0) = STATUS0_BUFFER_OVF1 | STATUS0_BUFFER_UDF1;   /* W1C */
        if (held == arm_grace && trim_tries < TRIM_TRIES) {
            /* ... and read it here: the FIFO's verdict on the lead. */
            u8 st = SR(STATUS0) & (STATUS0_BUFFER_OVF1 | STATUS0_BUFFER_UDF1);
            s8 t = trim;

            if (st == STATUS0_BUFFER_UDF1 && trim < TRIM_LIMIT)
                t++;
            else if (st == STATUS0_BUFFER_OVF1 && trim > -TRIM_LIMIT)
                t--;
            /* The last try is kept for judging the final step, not for another one. */
            if (t != trim && trim_tries < TRIM_TRIES - 1) {
                trim_tries++;
            } else {
                trim_tries = TRIM_TRIES;            /* the search ends here */
                /* Only a trim the FIFO came out clean at is worth keeping.  Still
                 * flagged at the end means the flag says nothing about the lead
                 * for this mode (some underflow at every trim, -3..+3, picture
                 * correct at 0), so go back to the calculation rather than
                 * keep a skewed end-of-search value. */
                t = st ? 0 : trim;
            }
            if (t != trim) {
                trim = t;
                display_watchdogs_disarm();
                display_freerun();
                armed = 0;
                why = 5;
                mode_set(t0ovf);
                break;
            }
        }
        if (held == arm_grace) {        /* ~1 s into ACTIVE: capture certainly running */
            /* The count the display clock came from, checked once more now
             * the source has certainly finished changing mode. */
            if (count_drifted()) {
                display_watchdogs_disarm();
                display_freerun();
                armed = 0;
                why = 8;
                state = SOURCE_LOCKING;
                break;
            }
            bound_ok[1] = hdmi_boundary_search(&bound[1]);
            /* Safety net: if what the hardware measures now disagrees with the
             * geometry that was programmed - because the pre-mode-set search
             * failed and the table was used, or for any other reason - redo
             * the mode set from the measurement.  Geometry-based, so it cannot
             * loop: after the redo the two agree and this does nothing. */
            if (bound_ok[1]) {
                struct hdmi_timing m = seen;

                hdmi_timing_from_bound(&m, &bound[1]);
                if (m.hactive != seen.hactive || m.vactive != seen.vactive
                    || m.hstart != seen.hstart || m.ihs != seen.ihs || m.vsta != seen.vsta) {
                    seen = m;
                    remeasures++;
                    display_watchdogs_disarm();
                    display_freerun();
                    armed = 0;
                    why = 6;
                    mode_set(t0ovf);
                    break;
                }
            }
        }
        if (held == active_settled) {   /* a genuinely stable picture */
            trips = 0;                  /* whatever tripped before is forgiven */
            settle_fails = 0;
            if (!arm_ok && reopens < VALVE_REOPEN_MAX) {
                arm_ok = 1;             /* and the hardware has earned another chance */
                reopens++;
            }
        }
        /* Watch for the input timing moving under us.
         *
         * With the display watchdogs disarmed - the default, because they trip
         * during normal operation here - NOTHING ELSE NOTICES a source that
         * changes mode without dropping its clock.  The capture window would
         * keep the old geometry and the picture is simply lost, with no path
         * back.  This is the software half of what the watchdog was supposed to
         * do, and it is what makes a live resolution change recoverable.
         *
         * Debounced: a single odd measurement mid-frame must not tear down a
         * working picture. */
        if (hdmi_measure(&t)) {
            /* Totals only.  The active region deliberately does NOT take part:
             * it is the less trustworthy measurement, and must not tear down a
             * working picture. */
            if (t.htotal != seen.htotal || t.vtotal != seen.vtotal)
                mismatch++;
            else
                mismatch = 0;
        } else {
            mismatch++;                 /* measure block lost the signal entirely */
        }
        if (mismatch >= MISMATCH_LIMIT) {
            display_watchdogs_disarm();
            display_freerun();          /* blank rather than show the wrong geometry */
            armed = 0;
            why = 7;
            enter_settling();           /* re-measure, then re-run the mode set */
            break;
        }

        if (!armed) {
            /* Arm only once the picture has been stable for the grace period,
             * and only from a CLEAN force_bg_en.
             *
             * Do NOT arm at the mode set and ignore trips during the grace:
             * the forced-background state is self-sustaining, so a trip inside
             * the grace window is still latched when the window expires and
             * gets counted as a trip every single time.  Waiting until the picture is demonstrably stable means force_bg_en
             * is genuinely clear at the moment we hand over to the hardware, so
             * a later set really is the watchdog acting. */
            if (ob_settings.wd_blank && held >= arm_grace && arm_ok && !watchdog_tripped()) {
                display_watchdogs_arm();
                armed = 1;
            }
            break;
        }
        if (watchdog_tripped()) {
            trip_st0 = SR(STATUS0);         /* what the input path was complaining about */
            trip_st1 = SR(STATUS1);
            /* The clock is still there, so this is the watchdog reacting to
             * something transient rather than the source going away.  Drop
             * back to SETTLING and re-run the mode set - NOT all the way to
             * IDLE, which would re-initialize the whole receiver PHY and turn
             * a hiccup into a second of blank screen. */
            display_watchdogs_disarm();
            display_freerun();
            armed = 0;
            if (held < active_settled && ++trips >= TRIP_LIMIT)
                arm_ok = 0;
            why = 11;
            enter_settling();
        }
        break;
    }

    powersave_tick(t0ovf, state == SOURCE_ACTIVE);
}

/* A picture is up on the input that owns the display (rx_port_dp says which). */
u8 source_picture(void) __banked
{
    return dp_owns ? (dp_st == DPS_SHOWN) : (state == SOURCE_ACTIVE);
}
