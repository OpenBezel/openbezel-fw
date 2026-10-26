/* RTD2556 display domain (D-domain): DTG, DPLL, dither, pattern generator.
 *
 * Fixed-mode free-run: the display timing generator runs the panel timing
 * without an input, the pattern generator / background color provides the
 * pixels.  Two things gate the D-domain output and are invisible to register
 * snapshots:
 *  - I_DB_CTRL (31C0) imd_db_en: the I/D double-buffer mechanism.  When set,
 *    register updates wait for an input VSYNC/DEN that never comes without
 *    HDMI input, so nothing is applied.  The free-run path clears it.
 *  - D_DITHER_COMMON_CTRL1 (006A) dither_enable with the dither SRAM tables
 *    loaded via the port 0069.
 * Gamma (GAMMA_CTRL_SETA bit6) must stay bypassed until a gamma LUT is loaded.
 *
 * Everything the D-domain needs is the explicit, named sequence in
 * display_freerun_init().
 */
#ifndef RTD2556_DISPLAY_H
#define RTD2556_DISPLAY_H

#include "ob_types.h"
#include "board.h"

/* dither SRAM tables, boards/<board>/display_tables.c */
extern const __code u8 dither_table[0x90];
extern const __code u8 dither_seq_table[0x30];
extern const __code u8 dither_temporal_table[0x20];

/* The DTG's line length entry holds the panel's line length minus 4 (2230 ->
 * 2226).  Writing its low byte is what loads the bank, so anything that edits
 * the DTG ends with it (hdmi_rx.c's window). */
#define DISPLAY_DTG_HTOTAL (PANEL_HTOTAL - 4)
#define DISPLAY_DTG_HTOTAL_L ((u8)DISPLAY_DTG_HTOTAL)

void display_freerun_init(void);        /* clocks, PLLs, DTG free-run, dither, gamma bypass */
void display_bg_rgb(u8 r, u8 g, u8 b);   /* background color (shown when force_bg_en) */
void display_force_bg(u8 on);            /* show the background color instead of the (absent) input */
void display_input_lock(void);           /* frame-sync the display to the M1 input (hdmi_input_mode) */
void display_freerun(void);              /* free-run + background color (debug mode) */
/* Display dither on/off.  The SRAM tables are loaded once in
 * display_freerun_init() and stay loaded, so this only flips the enable -
 * which is why it is safe to call at any time, unlike the input-dither block
 * whose tables we reload on every change. */
void display_dither(u8 on);

/* Display clock (DDS offset).  See display.c: 1 = changed, so re-measure the
 * eDP MVID.  0x843 = 150.516 MHz, the 1080p60 value. */
#define DISPLAY_DCLK_DEFAULT 0x843
u8 display_dclk_set(u16 off);
u16 display_dclk_get(void);

/* Display PLL M as well as the DDS offset - for clocks outside the DDS range on
 * the boot M (50 Hz).  1 = changed, re-measure the eDP MVID.  See display.c. */
#define DISPLAY_DPLL_M_DEFAULT 0x4D
u8 display_dclk_program(u8 m, u16 off);
u8 display_dpll_m_get(void);
void display_dpll_restart(u8 m, u16 off);   /* full power cycle of the PLL; re-measure MVID after */

/* Timer0 overflows across `frames` display VSYNCs (0 = no VSYNC).  The panel
 * refresh is the one clock in the system that does not move with the MCU, so
 * this is how the timebase gets calibrated - see display.c and timebase.h. */
u16 display_frame_ticks(u8 frames);
void display_wait_vsync(void);          /* returns just after the next display VSYNC (bounded) */

void display_crc_start(void);            /* kick the D-domain CRC engine (result valid after a frame) */
u32 display_crc_read(void);              /* last D_MAIN1_CRC result (0 = no display data) */

/* WATCH_DOG_CTRL0 (000C) bit 0 is READ-ONLY STATUS, not a write-1-to-clear flag
 * (writing 0x00 leaves 0x01), and it is only meaningful
 * while its enable CR000C[5] is set - with the watchdogs disarmed it reads
 * stale-high, so it must NOT be used as a health signal. */
void display_watchdogs_disarm(void);     /* all actions off; MUST precede any reprogramming */
/* Arms the watchdog ACTIONS.  Callable ONLY from core/source.c's SOURCE_ACTIVE
 * entry: the actions leave a self-sustaining forced state, and the state
 * machine is the only thing that notices and disarms.  See display.c. */
void display_watchdogs_arm(void);

#endif
