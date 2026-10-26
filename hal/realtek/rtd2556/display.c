/* RTD2556 display domain bring-up: clocks, PLLs and the free-running DTG.
 * See display.h. */
#include "display.h"
#include "timebase.h"
#include "delay.h"
#include "edp_tx.h"
#include "rtd2556_regs.h"

/* Display format bank (DISPLAY_FORMAT_ADDR_PORT / DATA_PORT): the panel's
 * timing from board.h - line length - 4, sync, background = active window.  The
 * free-run frame is longer than the panel's: V total +64 and
 * the fixed DV total +65 lines (only the free-run display uses them - with an
 * input the display is frame-synced to it). */
#define DTG_H_END (PANEL_HSTART + PANEL_WIDTH)
#define DTG_V_END (PANEL_VSTART + PANEL_HEIGHT)
#define DTG_V_TOTAL (PANEL_VTOTAL + 64)
static const __code u8 dtg_bank[][2] = {
    {DF_DISPLAY_HOR_TOTAL_H, HI(DISPLAY_DTG_HTOTAL)},
    {DF_DISPLAY_HOR_SYNC_END, PANEL_HSYNC},
    {DF_DISPLAY_HOR_BG_STA_H, HI(PANEL_HSTART)}, {DF_DISPLAY_HOR_BG_STA_L, LO(PANEL_HSTART)},
    {DF_DISPLAY_HOR_ACT_STA_H, HI(PANEL_HSTART)}, {DF_DISPLAY_HOR_ACT_STA_L, LO(PANEL_HSTART)},
    {DF_DISPLAY_HOR_ACT_END_H, HI(DTG_H_END)}, {DF_DISPLAY_HOR_ACT_END_L, LO(DTG_H_END)},
    {DF_DISPLAY_HOR_BG_END_H, HI(DTG_H_END)}, {DF_DISPLAY_HOR_BG_END_L, LO(DTG_H_END)},
    {DF_DISPLAY_VER_TOTAL_H, HI(DTG_V_TOTAL)}, {DF_DISPLAY_VER_TOTAL_L, LO(DTG_V_TOTAL)},
    {DF_DISPLAY_VER_SYNC_END, PANEL_VSYNC},
    {DF_DISPLAY_VER_BG_STA_H, HI(PANEL_VSTART)}, {DF_DISPLAY_VER_BG_STA_L, LO(PANEL_VSTART)},
    {DF_DISPLAY_VER_ACT_STA_H, HI(PANEL_VSTART)}, {DF_DISPLAY_VER_ACT_STA_L, LO(PANEL_VSTART)},
    {DF_DISPLAY_VER_ACT_END_H, HI(DTG_V_END)}, {DF_DISPLAY_VER_ACT_END_L, LO(DTG_V_END)},
    {DF_DISPLAY_VER_BG_END_H, HI(DTG_V_END)}, {DF_DISPLAY_VER_BG_END_L, LO(DTG_V_END)},
    {DF_FIXED_DVTOTAL_MSB, HI(DTG_V_TOTAL + 1)}, {DF_FIXED_DVTOTAL_LSB, LO(DTG_V_TOTAL + 1)},
    {DF_FIXED_LAST_LINE_MSB, HI(DISPLAY_DTG_HTOTAL)}, {DF_FIXED_LAST_LINE_LSB, LO(DISPLAY_DTG_HTOTAL)},
    {DF_DISPLAY_HOR_TOTAL_L, DISPLAY_DTG_HTOTAL_L},                     /* written last: loads the bank */
};

/* Sync processor and GDI clocks from M2PLL /13 (27 MHz), the measure clock
 * M2PLL /4, the M1 FIFO clock from the crystal. */
static void display_clocks_init(void)
{
    SR_SET(M1_FIFO_CLOCK_SELECT, M1_FIFO_CLOCK_SELECT_XTAL_SEL);
    SR(SYNCPROC_CLK_DIVIDER) = SYNCPROC_CLK_DIVIDER_SYNC_CLK_SEL | FV(SYNCPROC_CLK_DIVIDER_SYNC_CLK_DIV_M2PLL, 13);
    SR(MEASURE_CLK_DIVIDER) = MEASURE_CLK_DIVIDER_MS_CLK_SEL | FV(MEASURE_CLK_DIVIDER_MS_CLK_DIV, 4);
    SR(GDI_CLK_DIVIDER) = GDI_CLK_DIVIDER_GDI_CLK_SEL | FV(GDI_CLK_DIVIDER_GDI_CLK_DIV, 13);   /* also clocks the DP TX AUX */
    SR_CLR(POWER_CTRL, POWER_CTRL_SCDC_CLK_SEL);                          /* SCDC reference clock: the crystal */
}

/* M2PLL power-down, program, power-up, WD pulse */
static void m2pll_start(void)
{
    SR_SET(M2PLL_WD, M2PLL_WD_REG_DPLL_PWDN);
    SR(M2PLL_M) = 0x2F; SR(M2PLL_N) = 0x00;                 /* N write commits M */
    SR(M2PLL_CRNT) = M2PLL_CRNT_REG_DPLL_RS_13K | M2PLL_CRNT_REG_DPLL_CS_58PF | FV(M2PLL_CRNT_REG_DPLL_IP, 3);
    SR_CLR(M2PLL_WD, M2PLL_WD_REG_DPLL_VCORSTB);
    SR_CLR(M2PLL_WD, M2PLL_WD_REG_DPLL_PWDN);
    delay_ms(30);
    SR_SET(M2PLL_WD, M2PLL_WD_REG_DPLL_WDSET); delay_ms(3); SR_CLR(M2PLL_WD, M2PLL_WD_REG_DPLL_WDSET);
    delay_ms(75);
}

/* DPLL programming, power-on + WD pulse */
static void dpll_power_up(u8 m)
{
    SR_SET(DPLL_WD, DPLL_WD_REG_DPLL_PWDN);
    SR_WRF(DPLL_CAL, DPLL_CAL_REG_DPLL_VCOMD | DPLL_CAL_REG_DPLL_CALBP | DPLL_CAL_REG_DPLL_CALSW
                     | DPLL_CAL_REG_DPLL_CALLCH | DPLL_CAL_REG_DPLL_CMPEN,
           FV(DPLL_CAL_REG_DPLL_VCOMD, 2) | DPLL_CAL_REG_DPLL_CALBP);
    /* f = 25.06 MHz x (M + 2) / 14 (measured): M = 0x4D with the DCLK_GATED_CTRL/
     * fine-tune values below gives the 150.6 MHz display clock */
    SR(DPLL_M) = m; SR(DPLL_M_N_MSB) = DPLL_M_N_MSB_DPLL_M_N_SET_MODE;
    delay_ms(30);
    SR(DPLL_N) = 0x00; SR(DPLL_OUTPUT_CLK_DIV) = DPLL_OUTPUT_CLK_DIV_PIXELCLK_DIV_DIV2;
    SR(DPLL_CRNT) = DPLL_CRNT_REG_DPLL_RS_13K | DPLL_CRNT_REG_DPLL_CS_66PF | FV(DPLL_CRNT_REG_DPLL_IP, 5);
    SR_CLR(M2PLL_WD, M2PLL_WD_REG_DPLL_VCORSTB);
    SR_CLR(DPLL_WD, DPLL_WD_REG_DPLL_PWDN);
    delay_ms(8);
    SR_SET(DPLL_WD, DPLL_WD_REG_DPLL_WDSET); delay_ms(2); SR_CLR(DPLL_WD, DPLL_WD_REG_DPLL_WDSET);
    /* DCLK fine tune (DDS offset, 4.31 kHz per unit, latched by a DCLK_READY pulse): the DTG
     * line is DH_TOTAL + 4 = 2230 clocks, so 1080p60 line lock needs
     * 2230 x 67.5 kHz = 150.52 MHz = 0x843 (1124 lines + a 2080-clock partial last line). */
    SR(DCLK_FINE_TUNE_OFFSET_MSB) = 0x08; SR(DCLK_FINE_TUNE_OFFSET_LSB) = 0x43;
    SR(FIXED_LAST_LINE_CTRL) = FIXED_LAST_LINE_CTRL_DCLK_DDS_EN;
    SR(DCLK_SSC_COUNT) = DCLK_SSC_COUNT_DCLK_FMDIV_CNT_EN | FV(DCLK_SSC_COUNT_DCLK_FMDIV_CNT, 10);
    SR_PULSE_LO(DCLK_SPREAD_SPECTRUM, DCLK_SPREAD_SPECTRUM_DCLK_READY);          /* latch the offset */
    delay_ms(75);
}

static void load_dither(const __code u8 *t, u8 n, u8 access)
{
    u8 i;
    SR_WRF(D_DITHER_COMMON_CTRL1, D_DITHER_COMMON_CTRL1_DITHER_ACCESS, access);
    for (i = 0; i < n; i++)
        SR(D_DITHER_DATA_ACCESS) = t[i];
    SR_CLR(D_DITHER_COMMON_CTRL1, D_DITHER_COMMON_CTRL1_DITHER_ACCESS);
}

void display_freerun_init(void)
{
    u8 i;

    display_clocks_init();
    /* FREE-RUN: I/D double-buffer mechanism off, or updates wait for an input VSYNC */
    SR_CLR(I_DB_CTRL, I_DB_CTRL_IMD_DB_EN | I_DB_CTRL_IMD_DB_READY);
    SR_CLR(I_DB_CTRL0, I_DB_CTRL0_I_DB_EN | I_DB_CTRL0_I_M1_DB_READY);
    SR_CLR(D_DB_CTRL0, D_DB_CTRL0_D_DB_EN | D_DB_CTRL0_D_DB_READY);
    SR_SET(MN_SCLKG_CTRL, MN_SCLKG_CTRL_SCLKG_DBUF);        /* stream clock gen: commit PLL regs */
    m2pll_start();
    dpll_power_up(DISPLAY_DPLL_M_DEFAULT);
    for (i = 0; i < sizeof(dtg_bank) / 2; i++) {
        SR(DISPLAY_FORMAT_ADDR_PORT) = dtg_bank[i][0];
        SR(DISPLAY_FORMAT_DATA_PORT) = dtg_bank[i][1];
    }
    /* free-run display: force_en | dout_en | dtime_en (HW adds force_bg_en) */
    SR(VIDEO_DISPLAY_CONTROL_REG) = VIDEO_DISPLAY_CONTROL_REG_FORCE_EN
                                  | VIDEO_DISPLAY_CONTROL_REG_DOUT_EN
                                  | VIDEO_DISPLAY_CONTROL_REG_DTIME_EN;
    /* display dither: tables into SRAM, then enable (spatial + temporal) */
    load_dither(dither_table, sizeof(dither_table), DITHER_ACCESS_DITHER_TABLE);
    load_dither(dither_seq_table, sizeof(dither_seq_table), DITHER_ACCESS_SEQ_TABLE);
    load_dither(dither_temporal_table, sizeof(dither_temporal_table), DITHER_ACCESS_TEMPORAL);
    SR(D_DITHER_COMMON_CTRL1) = D_DITHER_COMMON_CTRL1_DITHER_ENABLE | D_DITHER_COMMON_CTRL1_DITHER_TEMPORAL_EN;
    /* gamma LUT is not loaded: table bypassed, channel ctrl 2 (0x20) */
    SR(GAMMA_CTRL_SETA) = FV(GAMMA_CTRL_SETA_GAMMA_CHANNEL_CTRL, 2);
}

/* Display clock DDS offset (0x01C4[3:0]:0x01C5), latched by the 0x01C6 bit 2
 * pulse exactly as dpll_power_up() does it.  f = f_base * (32768 + off)
 * / 32768 with f_base ~141.39 MHz, so the reachable range is ~141.4..159.0 MHz;
 * DISPLAY_DCLK_DEFAULT = 0x843 = 150.516 MHz is the 1080p60 value.
 *
 * Returns 1 if the clock actually changed - the caller must then re-measure
 * the eDP MVID (edp_mvid_update), or the panel regenerates the wrong pixel
 * clock.  An unchanged value is not rewritten, so repeated mode sets at the
 * same rate (1080p60 <-> 720p60) never disturb the clock. */
static u16 dclk_off = DISPLAY_DCLK_DEFAULT;

u8 display_dclk_set(u16 off)
{
    off &= 0x0FFF;
    if (off == dclk_off)
        return 0;
    dclk_off = off;
    SR(DCLK_FINE_TUNE_OFFSET_MSB) = HI(off);
    SR(DCLK_FINE_TUNE_OFFSET_LSB) = LO(off);
    SR_PULSE_LO(DCLK_SPREAD_SPECTRUM, DCLK_SPREAD_SPECTRUM_DCLK_READY);      /* latch */
    return 1;
}

#if defined(CONFIG_DEBUG_CONSOLE) || defined(CONFIG_DEBUG_VCP)
u16 display_dclk_get(void) { return dclk_off; }
#endif

/* Display PLL M, for display clocks the DDS alone cannot reach (50 Hz needs
 * ~125.4 MHz; the DDS spans ~141-159 MHz on the boot M of 77).  Base clock is
 * 25.06 MHz x (M + 2) / 14 (M 77 -> 141.4, M 78 -> 143.3 MHz), i.e. ~1.79 MHz
 * per step.
 *
 * M is changed WITHOUT powering the PLL down - write M, then the watchdog-set
 * pulse, as dpll_power_up does after power-up - so the clock slews rather than
 * stopping; a stopped DCLK starves the eDP transmitter mid-stream.  The DDS
 * offset is rewritten afterwards regardless, since it is relative to the new
 * base.  Returns 1 if anything changed (then re-measure the eDP MVID). */
static u8 dpll_m = DISPLAY_DPLL_M_DEFAULT;

u8 display_dclk_program(u8 m, u16 off)
{
    u8 changed = 0;

    if (m != dpll_m) {
        dpll_m = m;
        /* Order: double-buffer mode, M, 01D9 again, then N.  M alone
         * does NOT take effect - the double buffer commits on the N write,
         * which is also what dpll_power_up does at boot. */
        SR(DPLL_M_N_MSB) = DPLL_M_N_MSB_DPLL_M_N_SET_MODE;
        SR(DPLL_M) = m;
        SR(DPLL_M_N_MSB) = DPLL_M_N_MSB_DPLL_M_N_SET_MODE;   /* M/N high bits 0 */
        SR(DPLL_N) = 0x00;                                    /* as dpll_power_up: commits M/N */
        delay_ms(30);
        SR_SET(DPLL_WD, DPLL_WD_REG_DPLL_WDSET); delay_ms(2); SR_CLR(DPLL_WD, DPLL_WD_REG_DPLL_WDSET);
        dclk_off = 0xFFFF;                                    /* force the offset rewrite below */
        changed = 1;
    }
    if (display_dclk_set(off))
        changed = 1;
    return changed;
}

u8 display_dpll_m_get(void) { return dpll_m; }

/* The heavy fallback: the full boot power-down / power-up with the new M, then
 * the DDS offset.  The display clock STOPS for the duration, so the caller
 * must re-measure the eDP MVID afterwards. */
void display_dpll_restart(u8 m, u16 off)
{
    dpll_power_up(m);           /* leaves the DDS at the boot 0x843 */
    dpll_m = m;
    dclk_off = 0xFFFF;
    display_dclk_set(off);
}

void display_dither(u8 on)
{
    if (on)
        SR_SET(D_DITHER_COMMON_CTRL1, D_DITHER_COMMON_CTRL1_DITHER_ENABLE);
    else
        SR_CLR(D_DITHER_COMMON_CTRL1, D_DITHER_COMMON_CTRL1_DITHER_ENABLE);
}

/* bg_color_access_en resets the BG_COLOR_DATA_PORT pointer; write R, G, B, LSBs */
void display_bg_rgb(u8 r, u8 g, u8 b)
{
    SR_CLR(OVERLAY_CTRL, OVERLAY_CTRL_BG_COLOR_ACCESS_EN); SR_SET(OVERLAY_CTRL, OVERLAY_CTRL_BG_COLOR_ACCESS_EN);
    SR(BG_COLOR_DATA_PORT_M1) = r; SR(BG_COLOR_DATA_PORT_M1) = g; SR(BG_COLOR_DATA_PORT_M1) = b;
    SR(BG_COLOR_DATA_PORT_M1) = ((r & 3) << 6) | ((g & 3) << 4) | ((b & 3) << 2);
    SR_CLR(OVERLAY_CTRL, OVERLAY_CTRL_BG_COLOR_ACCESS_EN); SR_SET(OVERLAY_CTRL, OVERLAY_CTRL_BG_COLOR_ACCESS_EN);
}

/* VIDEO_DISPLAY_CONTROL_REG's timing-mode bits (DATA_INV and the reserved bits 4 and 2 are kept) */
#define VDC_RUN (VIDEO_DISPLAY_CONTROL_REG_FORCE_EN | VIDEO_DISPLAY_CONTROL_REG_FORCE_BG_EN \
                 | VIDEO_DISPLAY_CONTROL_REG_FRAME_SYNC_EN | VIDEO_DISPLAY_CONTROL_REG_DOUT_EN \
                 | VIDEO_DISPLAY_CONTROL_REG_DTIME_EN)

/* input-locked display: frame_sync | dout | dtime, with the background FORCED
 * in the same write - the caller releases it with display_force_bg(0) once the
 * first frames are through.  Must be one write: clearing force_bg_en and
 * setting it again a statement later lets a partial line of input noise
 * through on every mode change. */
void display_input_lock(void)
{
    SR_WRF(VIDEO_DISPLAY_CONTROL_REG, VDC_RUN,
           VIDEO_DISPLAY_CONTROL_REG_FORCE_BG_EN | VIDEO_DISPLAY_CONTROL_REG_FRAME_SYNC_EN
           | VIDEO_DISPLAY_CONTROL_REG_DOUT_EN | VIDEO_DISPLAY_CONTROL_REG_DTIME_EN);
}

/* back to free run (no input needed): double buffer off, force_en, background color.
 * Also disarms the input watchdogs that hdmi_input_mode() arms: left armed with no input
 * they keep firing and hold the OSD overlay off (OVERLAY_CTRL overlay_port_en will not
 * stick while they are armed). */
void display_freerun(void)
{
    SR(WATCH_DOG_CTRL0) = 0x00;                 /* all watchdog actions off (bit 0 is status, not a flag) */
    SR_CLR(I_DB_CTRL, I_DB_CTRL_IMD_DB_EN | I_DB_CTRL_IMD_DB_READY);
    SR_WRF(VIDEO_DISPLAY_CONTROL_REG, VDC_RUN,
           VIDEO_DISPLAY_CONTROL_REG_FORCE_EN | VIDEO_DISPLAY_CONTROL_REG_FORCE_BG_EN
           | VIDEO_DISPLAY_CONTROL_REG_DOUT_EN | VIDEO_DISPLAY_CONTROL_REG_DTIME_EN);
}

/* The watchdog action (m1_bg_en + wd_freerun_en) forces force_bg_en on and
 * clears force_en/frame_sync_en.  That state is SELF-SUSTAINING: with force_en
 * clear the timing generator waits for an input IVS trigger, and with the
 * output forced to background it never gets one, so DVS never returns and the
 * watchdog never releases.  Nothing short of disarming it recovers, and an
 * 8051 reset leaves the block armed. */
void display_watchdogs_disarm(void)
{
    SR(WATCH_DOG_CTRL0) = 0x00;
}

/* Arm the watchdog actions (all but buf_error_wden).
 *
 * ONLY core/source.c may call this, and only from SOURCE_ACTIVE, because the
 * escape is not in this file: the state machine watches for the forced state
 * and disarms (WATCH_DOG_CTRL0 = 0 brings a wedged panel straight back).
 *
 * What each bit buys:
 *   hv_err_wden     input HSYNC/VSYNC went wrong      <- a source-loss detector
 *   dvs_timeout_wden  display VSYNC stopped arriving    <- a source-loss detector
 *   buf_error_wden  capture FIFO over/underflow         <- NOT armed, see below
 *   wd_freerun_en   ACTION: run the display timing free rather than frame-synced
 *   m1_bg_en        ACTION: show the background color instead of input data
 * Both actions are needed together: m1_bg_en alone would blank the data while
 * the timing generator stayed slaved to a dead input, which outputs nothing.
 *
 * buf_error_wden is deliberately NOT armed.  It
 * reports capture FIFO over/underflow against a frame-synced display, which is
 * a *quality* signal during normal operation, not a source-loss signal - and
 * our DCLK is a fixed DDS offset (0x843) rather than tracking, so an occasional
 * slip is expected rather than exceptional.  Arming it makes every slip blank
 * the panel and re-run the mode set.  The other two cover what this feature is
 * actually for. */
#define WATCHDOG_ARMED (WATCH_DOG_CTRL0_HV_ERR_WDEN | WATCH_DOG_CTRL0_DVS_TIMEOUT_WDEN \
                      | WATCH_DOG_CTRL0_WD_FREERUN_EN | WATCH_DOG_CTRL0_M1_BG_EN)

void display_watchdogs_arm(void)
{
    SR(WATCH_DOG_CTRL0) = WATCHDOG_ARMED;
}

void display_force_bg(u8 on)
{
    if (on)
        SR_SET(VIDEO_DISPLAY_CONTROL_REG, VIDEO_DISPLAY_CONTROL_REG_FORCE_BG_EN);
    else
        SR_CLR(VIDEO_DISPLAY_CONTROL_REG, VIDEO_DISPLAY_CONTROL_REG_FORCE_BG_EN);
}

/* Timer0 overflows elapsed across `frames` display VSYNCs; 0 if VSYNC never
 * arrives.  This is the calibration the firmware otherwise lacks: the overflow
 * period moves with the MCU clock (see timebase.h), but the panel refresh does
 * not - the DTG runs at ~60.06 Hz off its own DCLK.  So counting overflows
 * against display frames measures the timebase in real time, with no dependence
 * on what the MCU clock happens to be doing.
 *
 * 64 frames is ~1.067 s, which makes the result read almost directly as
 * "overflows per second". */
#define FRAME_TICKS_MIN 4       /* fewer overflows than this is not a real measurement */

u16 display_frame_ticks(u8 frames)
{
    u16 t0 = tb_now();
    u16 guard, elapsed;
    u8 i;

    SR(STATUS1) = STATUS1_DVS_START;                /* w1c: start from a clean edge */
    for (i = 0; i < frames; i++) {
        guard = 0;
        while (!(SR(STATUS1) & STATUS1_DVS_START)) {
            tb_service();
            if (++guard == 0)
                return 0;                           /* no display VSYNC at all */
        }
        SR(STATUS1) = STATUS1_DVS_START;
    }
    elapsed = (u16)(tb_now() - t0);
    /* A run of frames that took almost no time did not measure frames: it means
     * DVS_START was not behaving as an event latch here (already set, or not
     * clearing), so the loop fell straight through.  Report failure rather than
     * a number - a bogus calibration is far worse than none, because every
     * timeout in the firmware is derived from it. */
    if (elapsed < FRAME_TICKS_MIN)
        return 0;
    return elapsed;
}

/* Return just after the next display VSYNC (DVS_START), so what is written next lands
 * in the vertical blanking.  Bounded like display_frame_ticks(): no VSYNC, no hang. */
void display_wait_vsync(void)
{
    u16 guard = 0;

    SR(STATUS1) = STATUS1_DVS_START;                /* w1c */
    while (!(SR(STATUS1) & STATUS1_DVS_START)) {
        tb_service();
        if (++guard == 0)
            return;
    }
}

#ifdef CONFIG_DEBUG_CONSOLE
void display_crc_start(void)
{
    SR_SET(D_CRC_CTRL, D_CRC_CTRL_D_CRC_M1_OUTCTRL);
}

u32 display_crc_read(void)
{
    return ((u32)SR(D_MAIN1_CRC_1) << 24) | ((u32)SR(D_MAIN1_CRC_2) << 16)
         | ((u16)SR(D_MAIN1_CRC_3) << 8) | SR(D_MAIN1_CRC_4);
}
#endif

