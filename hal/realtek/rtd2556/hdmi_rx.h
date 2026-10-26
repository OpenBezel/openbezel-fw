/* RTD2556 HDMI receiver, port D3 (GDI PHY page 0x7C, HDMI system block page 0x72),
 * feeding the M1 input capture / scaler (page 0, 05, 16, 30, 31).
 *
 * Bring-up sequence:
 *   hdmi_rx_phy_on      GDI PHY power-on: freq-detector clock ungate, terminations, HDMI mode
 *   hdmi_rx_sys_init    TMDS decoder / HDCP block init, arm the TMDS clock frequency detector
 *   hdmi_rx_detect      wait for a stable TMDS clock, read the count -> hdmi_stat.f_tmds (0.1 MHz)
 *   hdmi_rx_band_config CDR/CMU/DFE configuration for the measured frequency band
 *   hdmi_rx_lock        TMDS decoders locked (TMDS_CTRL bcd/gcd/rcd), video output enable
 *   hdmi_input_mode     M1 capture, scaler and measure blocks for the measured input
 *                       timing, frame-synced display
 *
 * Only 24-bit RGB input is supported: the EDID we serve does not advertise deep
 * color (the RX's deep-color packing path is not yet ported).
 */
#ifndef RTD2556_HDMI_RX_H
#define RTD2556_HDMI_RX_H

#include "ob_types.h"

#define HDMI_OK 0
#define HDMI_ERR_NO_CLOCK 1     /* frequency detector never reported a stable TMDS clock */
#define HDMI_ERR_NO_LOCK 2      /* TMDS decoders did not lock */
#define HDMI_ERR_BAND 3         /* TMDS clock >= 200.1 MHz: analog-cdR path not ported */

struct hdmi_status {
    u16 tmds_count;     /* HDMI_FREQDET_RESULT: TMDS clocks per 1000 reference (27 MHz) cycles */
    u16 f_tmds;         /* TMDS clock, 0.1 MHz units (1485 = 148.5 MHz) */
    u8 lock;            /* TMDS_CTRL lock flags at hdmi_rx_lock (F8 = all channels) */
    u8 mode_hdmi;       /* HDMI_SR mode bit: 1 = HDMI, 0 = DVI */
    u8 result;          /* HDMI_* of the last hdmi_rx_start */
    u8 detect_polls;
    u8 dclk_ok;         /* 0: this mode needs a display clock the DDS cannot reach (e.g. 50 Hz) */
    u8 dclk_path;       /* how the last PLL multiplier change went: DCLK_PATH_* */
};
#define DCLK_PATH_NONE 0    /* no multiplier change needed */
#define DCLK_PATH_SOFT 1    /* M changed live, MVID confirmed it */
#define DCLK_PATH_HARD 2    /* live change missed; the full power-up sequence worked */
#define DCLK_PATH_FAILED 3  /* neither produced the requested clock */
extern struct hdmi_status hdmi_stat;
extern u8 rx_port_dp;                   /* capture input: 0 HDMI (D3), 1 DP (D1) */

/* Display-clock calculation, captured at the last mode set (debug console). */
struct dclk_dbg {
    u16 count;          /* TMDS frequency-detector count used */
    u32 target;         /* display frame, dclk (1024x768 DMT: 2528670 = 0x26959E) */
    u32 hv;             /* input Htot * Vtot (1024x768: 1083264 = 0x108780) */
    u32 k2;             /* TMDS clock, kHz (count x 27) */
    u32 w2;             /* required display clock, kHz */
};
extern struct dclk_dbg hdmi_dclk_dbg;

/* Input timing as the off-line measure block (page 30) actually counts it. */
struct hdmi_timing {
    u16 htotal;         /* pixels per line  (HS_PERIOD_D, stored as value - 1) */
    u16 vtotal;         /* lines per frame  (VS_PERIOD_A, stored as value - 1) */
    u16 refresh_x10;    /* vertical refresh in 0.1 Hz */
    /* Active size, looked up from the totals against a table of standard modes
     * (see hdmi_rx.c).  `active_ok` is 0 for an unrecognized timing; the totals
     * above stay valid either way, so the mode set is never gated on this. */
    u8 active_ok;
    u16 hactive, vactive;
    u16 hstart;                 /* capture window H start for this mode */
    u16 ihs;                    /* IHS delay (001D + 001E bits 0/2): where the I-domain line begins */
    u8 vsta;                    /* input VSYNC -> first active line; feeds frame_sync_set() */
};

/* 1 if the measure block reports a clean, plausible frame; 0 otherwise (and
 * *t is then untouched).  Rejects on measure errors set, zero periods, or a
 * refresh outside a sane range. */
u8 hdmi_measure(struct hdmi_timing *t) __banked;

/* Active-region boundary search, raw: the first/last active pixel and line as
 * the hardware measures them off DE.  1080p reads H 271..2190, V 43..1122. */
struct hdmi_bound {
    u16 hs, he;         /* HX_STA / HX_END, input pixels */
    u16 vs, ve;         /* VX_STA / VX_END, input lines */
};

/* Run the digital boundary search (active region measured off DE).
 * Turns input sampling on, and blocks ~3 frames.  1 = a plausible region,
 * 0 = nothing found (*b is filled either way, for display). */
u8 hdmi_boundary_search(struct hdmi_bound *b) __banked;

/* Replace the mode table's geometry in *t with the measured one: active size,
 * capture H start and vsta.  The table stays the fallback when the search
 * fails. */
void hdmi_timing_from_bound(struct hdmi_timing *t, const struct hdmi_bound *b) __banked;

/* Start the off-line measure block on the input syncs.  hdmi_input_mode() calls
 * it, but the source state machine must also call it BEFORE the mode set - the
 * measurement is what gates the mode set, so it has to be running first. */
void hdmi_measure_start(void) __banked;


void hdmi_rx_phy_on(void) __banked;
void hdmi_rx_sys_init(void) __banked;
u8 hdmi_rx_detect(void) __banked;
void hdmi_rx_band_config(void) __banked;
u8 hdmi_rx_lock(void) __banked;
u8 hdmi_rx_start(void) __banked;          /* phy_on .. lock; result in hdmi_stat.result */

/* Live frequency-detector count; hdmi_stat.tmds_count is only refreshed by a
 * full hdmi_rx_start(). */
u16 hdmi_freq_count(void) __banked;
/* Capture/scaler/measure, display frame-synced.  Pass the settled timing to
 * drive the capture window from what the input actually is; pass 0 for an
 * unrecognized timing, which is set up as 1080p60. */
void hdmi_input_mode(const struct hdmi_timing *t) __banked;

/* Scaling mode, applied by the next hdmi_input_mode().
 *
 * Only the modes this pipeline can actually build.  The display is
 * frame-locked to the input through a line buffer (no frame memory), so the
 * display's ACTIVE window has to take the same share of its frame as the
 * captured lines take of the input's.  That leaves the horizontal window free
 * (Fit's pillarbox) but not the vertical one: 1:1 "no scaling" needs fewer
 * display lines per frame than the panel has, and Fill's vertical crop needs
 * ~1.4x the display line rate. */
#define HDMI_SCALE_STRETCH 0    /* fill the panel, ignore aspect */
#define HDMI_SCALE_FIT 1        /* keep aspect: pillarbox anything narrower than 16:9 */
#define HDMI_SCALE_COUNT 2
void hdmi_scaling_set(u8 mode) __banked;

/* AVI InfoFrame picture aspect (PB2 M1:M0): HDMI_AR_NONE when there is no
 * valid AVI InfoFrame or it says "no data".  Read from the packet storage
 * SRAM (72CD/72CE; index 1 version, 2 length, 3 checksum, 4.. PB1..PB13),
 * validated by length and checksum. */
#define HDMI_AR_NONE 0
#define HDMI_AR_4_3 1
#define HDMI_AR_16_9 2
u8 hdmi_window_stale(const struct hdmi_timing *t) __banked;   /* new AVI aspect moves the output window */
u8 hdmi_avi_aspect(void) __banked;

/* HDMI audio, receiver side: audio DPLL power-up + calibration, then the
 * audio FIFO / tracking / preset-code init - the order matters (hdmi_rx.c).
 * The output itself is then switched on by the receiver's automatic flow when
 * it detects audio.  hdmi_audio_rate(): AUDIO_FREQ_RESULT, 0 = no audio,
 * 3 = 48 kHz (1 32k, 2 44.1k, ...). */
void hdmi_audio_start(void) __banked;
u8 hdmi_audio_rate(void) __banked;
/* The aspect the last mode set used (what output_window() decided from). */
extern u8 hdmi_ar_used;

/* Output window of the last mode set, in panel pixels (width and left edge
 * inside the 1920-wide background), for the debug console. */
extern u16 hdmi_out_w, hdmi_out_x;

/* Extra frame-sync lead in input lines, added to the calculated IVS->DVS
 * delay by the next mode set.  Set by core/source.c's closed-loop trim (and
 * DDC/CI FAh).  The line buffer leaves only 1-2 lines of slack when scaling
 * vertically, and the per-mode error in the calculation is about that size. */
extern s8 hdmi_fs_extra_lines;
void hdmi_clock_watch(void) __banked;         /* clear the unstable flag: hdmi_clock_moved() from now */
u8 hdmi_clock_moved(void) __banked;          /* TMDS clock left its locked window or stopped */
u8 hdmi_clock_present(void) __banked;     /* 1 if the RX sees a TMDS clock (cheap poll; RX must be armed) */

#endif
