/* OpenBezel firmware - recovery-safe base + auto boot bring-up.
 *
 * Boot presents a healthy HDMI display to the source (valid EDID at I2C 0x50
 * + HDMI hot-plug asserted) as the FIRST thing after the proven
 * clock/pinshare init, before any code that could hang.  Then the GPU keeps
 * clocking the DDC bus, so DDC-ISP recovery is always possible - a hang
 * costs a reflash over DDC, never a desolder.  (With HPD low the GPU tears
 * the DDC bus down.)
 *
 * After the recovery-safe base (HDMI HPD + EDID) there is an ISP break-in
 * window, then bring-up runs automatically: MCU clock to PLL,
 * eDP link training, free-run display, video stream, OSD.  The UI starts on the
 * SPLASH (boot logo, a setting) and switches to INPUT - the picture from HDMI or
 * USB-C, or "No signal" - when video appears on either, or after ~60 s or a key.
 * With no menu up, ENTER opens the settings menu, UP/DOWN change the volume and
 * POWER opens the input picker (menu_hotkey).  The DEBUG console
 * (CONFIG_DEBUG_CONSOLE) is reached from the menu; DDC/CI E7h selects any of
 * the three.
 *
 * Debug without a picture: halt-and-inspect via the font-SRAM mailbox, read
 * over DDC-ISP (word 0 = MAGIC 0x5A | heartbeat | checkpoint).
 */
#include "ob_types.h"
#include "dbgmon.h"
#include "console.h"
#include "board.h"
#include "rtd2556_regs.h"
#include "delay.h"
#include "edp_tx.h"
#include "display.h"
#include "osd.h"
#include "hdmi_rx.h"
#include "keys.h"
#include "settings.h"
#include "mccs.h"
#include "audio.h"
#include "dp_rx.h"
#include "hdcp.h"
#include "menu.h"
#include "dcr.h"
#include "source.h"

extern __xdata u8 code_page_shadow;  /* startup.c: current code bank */
u8 banks_consistent(void);           /* startup.c: all banks from this build? */
#include "flash_assets.h"
#include "timebase.h"
#include "backlight.h"

void rtd_ddc_external_enable(void);
void xinit_copy(void);        /* hal/realtek/rtd2556/startup.c */
void scaler_soft_reset(void); /* hal/realtek/rtd2556/startup.c */
void rtd_serve_edid(void);

/* build-info block stamped by tools/mkimage.py: 'OBFW' + version string */
#define FW_VERSION ((const __code char *)0xFF04)

#if defined(CONFIG_DEBUG_CONSOLE)
/* console rows (see con_refresh) */
#define ROW_BANNER 0
#define ROW_LOOP 1
#define ROW_KEYS 2
#define ROW_EDP 3
#define ROW_LINK 4
#define ROW_DISP 5
#define ROW_STREAM 6
#define ROW_CRC 7
#endif

/* eDP test result -> font-SRAM mailbox words (read with tools/rtdwords.py):
 *  4 LT result | aux_err | pll_stat(CMU4)
 *  5 cr_tries | eq_tries | eDP HPD
 *  6..10 DPCD 000-00E
 *  11..12 DPCD 202-207
 *  13 aux_dbg irq|fsm|fifo
 *  14 ML_PAT_SEL | DP_PHY_CTRL | PHY0 analog power */
static void report_edp(u8 lt)
{
    dbgmon_diag(4, lt, edp_stat.aux_err, edp_stat.pll_stat);
    dbgmon_diag(5, edp_stat.cr_tries, edp_stat.eq_tries, edp_hpd());
    dbgmon_diag(6, edp_stat.caps[0], edp_stat.caps[1], edp_stat.caps[2]);
    dbgmon_diag(7, edp_stat.caps[3], edp_stat.caps[4], edp_stat.caps[5]);
    dbgmon_diag(8, edp_stat.caps[6], edp_stat.caps[7], edp_stat.caps[8]);
    dbgmon_diag(9, edp_stat.caps[9], edp_stat.caps[10], edp_stat.caps[11]);
    dbgmon_diag(10, edp_stat.caps[12], edp_stat.caps[13], edp_stat.caps[14]);
    dbgmon_diag(11, edp_stat.lane[0], edp_stat.lane[1], edp_stat.lane[2]);
    dbgmon_diag(12, edp_stat.lane[3], edp_stat.lane[4], edp_stat.lane[5]);
    dbgmon_diag(13, edp_stat.aux_dbg[0], edp_stat.aux_dbg[1], edp_stat.aux_dbg[2]);
    dbgmon_diag(14, SR(DPTX_ML_PAT_SEL), SR(DP_PHY_CTRL), SR(P55_LVDS_COMMON_AB_CTRL0));
    /* 2: display domain: VIDEO_DISPLAY_CONTROL | I_DB_CTRL | TX1 arbiter debug (0x19 = DE reaching TX1) */
    dbgmon_diag(2, SR(VIDEO_DISPLAY_CONTROL_REG), SR(I_DB_CTRL), SR(ARBITER_DEBUG));
    /* 3: AUX TX PHY activity during the last transaction (MCU-speed samples) */
    dbgmon_diag(3, edp_stat.mac_fsm_max, edp_stat.phy_fsm_max, edp_stat.phy_flags);
}

static u8 led_color;            /* 0 amber, 1 green, 2 red */
static __xdata u8 led_slow;     /* powersave blink divider */
#ifdef CONFIG_DEBUG_CONSOLE
static __xdata u8 menu_was_up;  /* menu_active() at the last UI tick */
#endif
static u8 test_lt = 0xFF;       /* link-training result (0xFF = not run) */
static u8 display_up;

/* UI modes: SPLASH (boot logo), INPUT (HDMI / USB-C), DEBUG (console) - see the top of this file */
#define UI_SPLASH 0
#define UI_INPUT 1
#define UI_DEBUG 2
#define UI_MODES 3
static u8 ui_mode = UI_SPLASH;
static u8 key_presses;
static u16 ui_t0ovf;            /* Timer0 overflows; the popup's timebase */

/* Boot milestones, in Timer0 overflows since tb_init(): nothing displays them;
 * read them with the DDC/CI peek (F0h/F1h at boot_mark's address in the map),
 * because the question "where do the seconds go" wants a measurement and not
 * a guess.  NOTE the overflow period is NOT constant
 * across these: BM_ISP is on the crystal clock, everything after it is on the
 * PLL (mcu_clock_to_pll runs first thing in bringup), so the marks either side
 * of that switch are in different units.  See timebase.h. */
#define BM_ISP 0                /* ISP break-in window done - still on the crystal */
#define BM_LT 1                 /* eDP link training returned */
#define BM_DISP 2               /* display domain + video stream up */
#define BM_HDMI 3               /* HDMI receiver started */
#define BM_PIC 4                /* first UI mode set: picture or logo on the panel */
#define BOOT_MARKS 5
static u16 boot_mark[BOOT_MARKS];
static u16 boot_cal;            /* Timer0 overflows measured across CAL_FRAMES */
#define CAL_FRAMES 32
#define CAL_MS 533              /* 32 frames at the panel's ~60.06 Hz */

/* logo centered on the panel */
#define LOGO_X ((PANEL_WIDTH - LOGO_W) / 2)
#define LOGO_Y ((PANEL_HEIGHT - LOGO_H) / 2)

#if defined(CONFIG_DEBUG_CONSOLE)
/* Row 0 of the debug console.  Drawn on every entry to the debug screen, not
 * just at boot: the popup and menu share the console buffer and clear or
 * overwrite row 0. */
static void banner_draw(void)
{
    con_puts(ROW_BANNER, 0, "                                        ");
    con_puts(ROW_BANNER, 0, "OpenBezel ");
    con_puts(ROW_BANNER, 10, FW_VERSION);
    con_puts(ROW_BANNER, 22, "debug console");
}
#endif

static void ui_set_mode(u8 mode)
{
    if (ui_mode == UI_INPUT && mode != UI_INPUT)
        source_detach();                        /* disarm before anyone reprograms the display */
    /* Every branch below reconfigures the console, so any pending popup has to
     * be forgotten first - otherwise it expires later and clears the screen
     * that replaced it. */
    menu_popup_cancel();
    ui_mode = mode;
    mccs_ui_now = mode;
    if (mode == UI_SPLASH) {
        con_disable();                          /* OSD off before reloading font SRAM */
        display_freerun();                      /* free-run DTG, watchdogs disarmed */
        display_bg_rgb(0x00, 0x00, 0x00);       /* black behind the logo */
        osd_logo_show(LOGO_X, LOGO_Y);          /* loads logo glyphs + tilemap */
        osd_on();
    } else if (mode == UI_INPUT) {
        con_disable();
        /* The source state machine owns the display from here: it runs the
         * mode set once the measure block agrees, and it is what arms the
         * display watchdogs.  Nothing else may drive the input path. */
        display_freerun();                      /* background until it gets there */
        source_attach();
    }
#ifdef CONFIG_DEBUG_CONSOLE
    else {                                      /* UI_DEBUG */
        display_freerun();                      /* free-run DTG, watchdogs disarmed */
        display_bg_rgb(0xFF, 0x00, 0x00);       /* red background, so the modes are distinguishable */
        osd_load_text_font();                   /* logo mode overwrote the glyphs */
        con_colors(OSD_COL_WHITE, OSD_COL_TRANSPARENT);  /* the menu/popup leave opaque colors behind */
        banner_draw();                          /* the popup may have left its text in row 0 */
        con_enable(32, 24);                     /* draws the console and turns the OSD on */
    }
#endif
}

#if defined(CONFIG_DEBUG_CONSOLE) || defined(CONFIG_DEBUG_VCP)
/* Stack high-water mark.  The frame-sync / display-clock maths are __reentrant,
 * which puts their temporaries on the stack during a mode set.  An 8051 stack overflow corrupts memory silently, so this
 * measures instead of assuming: everything above SP is painted at boot, and
 * stack_peak() reports the highest address that has ever been touched.
 * 0xFF is the top of internal RAM - a reading of FF means it has been hit. */
#define STACK_PAINT 0xA5

static void stack_paint(void)
{
    __idata u8 *p = (__idata u8 *)(SP + 4);             /* leave main's own frame alone */

    do {
        *p = STACK_PAINT;
    } while (++p != 0);                                 /* 8-bit pointer: stops after 0xFF */
}

u8 stack_peak(void)
{
    __idata u8 *p = (__idata u8 *)0xFF;

    while (*p == STACK_PAINT && p != (__idata u8 *)SP)
        p--;
    return (u8)p;
}
#endif

static void mcu_clock_to_pll(void)
{
    /* Switch the MCU clock to its runtime configuration
     * (flash_clk_div=4, flash_clk_div2=2, THEN mcu_clk_sel=PLL).
     * Order matters: dividers first, so the flash clock lands at
     * PLL/8 - switching with the dividers still at 1 runs the flash at PLL/1
     * and bricks the chip.  The AUX TX PHY never transmits under the crystal
     * clock, so this is a precondition for eDP. */
    SR_WRF(MCU_CONTROL, MCU_CONTROL_FLASH_CLK_DIV, FV(MCU_CONTROL_FLASH_CLK_DIV, 4));   /* still on xtal */
    SR_WRF(MCU_CLK_CONTROL_1, MCU_CLK_CONTROL_1_FLASH_CLK_DIV2, FV(MCU_CLK_CONTROL_1_FLASH_CLK_DIV2, 2));
    SR_SET(MCU_CONTROL, MCU_CONTROL_MCU_CLK_SEL);
}

/* Full bring-up, run automatically after the ISP break-in window.  eDP link, display domain, video stream, backlight, OSD,
 * HDMI receiver; then show the boot logo. */
static __xdata u8 lt_try;       /* eDP training attempts at boot (debug console) */

static void bringup(void)
{
    dbgmon_checkpoint(0x44);
    mcu_clock_to_pll();
    dbgmon_checkpoint(0x45);
    /* TX1 (pages 9C/9D + PHY0) eDP bring-up; the order matters. */
    dbgmon_checkpoint(0x40);
    edp_tx_init();                /* clock select, power domain, TX1 AUX PHY + HPD */
    /* After a soft reset (the end of every flash) the first training sometimes
     * fails: retry the panel power cycle and the training a couple of times. */
    for (lt_try = 0; lt_try < 3; lt_try++) {
        edp_panel_power_cycle();  /* real panel-VCC edge */
        dbgmon_checkpoint(0x41);
        edp_phy0_on();            /* PHY0 analog/CMU */
        delay_ms(250);            /* settle before the status poll */
        dbgmon_checkpoint(0x43);
        test_lt = edp_link_train();   /* SET_POWER, caps, config, TPS1/CR, TPS2/EQ, finish */
        if (test_lt == EDP_LT_OK)
            break;
    }
    boot_mark[BM_LT] = tb_now();
    dbgmon_checkpoint(0x42);
    if (test_lt == EDP_LT_OK) {
        /* free-run display domain + video stream, backlight on */
        dbgmon_checkpoint(0x46);
        display_freerun_init();          /* starts M2PLL, which PWM5 runs from */
        backlight_pwm_init();
        display_bg_rgb(0x00, 0x00, 0x00);
        display_force_bg(1);
        edp_video_on();
        settings_apply_backlight();
        boot_mark[BM_DISP] = tb_now();
        dbgmon_checkpoint(0x47);
        /* OSD: palette, bases, text font (console); logo loads its own glyphs on demand */
        osd_init();
#ifdef CONFIG_DEBUG_CONSOLE
        banner_draw();
#endif
        display_up = 1;
        dbgmon_checkpoint(0x48);
        /* HDMI receiver up (PHY, decoder); try to lock a source now */
        hdmi_rx_start();
        boot_mark[BM_HDMI] = tb_now();
        dbgmon_checkpoint(0x49);
        /* boot logo is a setting; without it go straight to the input */
        ui_set_mode(ob_settings.boot_logo ? UI_SPLASH : UI_INPUT);
        boot_mark[BM_PIC] = tb_now();
        dbgmon_checkpoint(0x4A);
        /* Calibrate LAST, with a real picture on the panel.  Any earlier (e.g.
         * right after edp_video_on) DVS_START does not behave as an event latch
         * yet, display_frame_ticks() falls straight through, and the bogus
         * calibration makes powersave blank the backlight right after boot.
         * tb_calibrate() also rejects an implausible result, so a bad reading
         * degrades to the defaults instead of losing the picture. */
        boot_cal = display_frame_ticks(CAL_FRAMES);
        tb_calibrate(boot_cal, CAL_MS);
        source_init();
    }
    report_edp(test_lt);
    led_color = (test_lt == EDP_LT_OK) ? 1 : 2;
}

#if defined(CONFIG_DEBUG_CONSOLE)
/* Refresh the console status rows (called once per heartbeat tick). */
static void con_refresh(u16 ticks, u16 t0ovf, u8 a0, u8 a1, u8 min_a0)
{
    static u8 slow;

    con_puts(ROW_LOOP, 0, "tick");      con_hex16(ROW_LOOP, 5, ticks);
    con_puts(ROW_LOOP, 10, "t0");       con_hex16(ROW_LOOP, 13, t0ovf);
    con_puts(ROW_LOOP, 18, "cp");       con_hex8(ROW_LOOP, 21, dbgmon_last_checkpoint());
    /* timebase calibration: Timer0 overflows per CAL_FRAMES display frames.
     * 0x3E (62) is the measured post-PLL figure = 8.59 ms per overflow. */
    con_puts(ROW_LOOP, 24, "cal");      con_hex16(ROW_LOOP, 28, boot_cal);
    con_putc(ROW_LOOP, 32, tb_calibrated() ? 'c' : '!');   /* '!' = rejected, running on defaults */
    /* display clock DDS offset for the current mode; 0843 = 1080p60.
     * '!' after it = this mode needs a clock the DDS cannot reach (50 Hz). */
    con_putc(ROW_LOOP, 34, 'd');
    con_hex16(ROW_LOOP, 35, display_dclk_get());
    con_putc(ROW_LOOP, 39, hdmi_stat.dclk_ok ? ' ' : '!');
    (void)a0; (void)a1; (void)min_a0;
    /* DDC/CI: Set VCP count, then the last code and value the host wrote.
     * KDE PowerDevil writes 10h on every hotplug - this is where that shows. */
    con_puts(ROW_KEYS, 0, "dc");        con_hex8(ROW_KEYS, 2, mccs_sets);
    con_hex8(ROW_KEYS, 5, mccs_last_code); con_hex16(ROW_KEYS, 7, mccs_last_val);
    /* Scaling: output window width as computed, then the DTG active start
     * READ BACK through the format port (48 = full width; Fit at 4:3 = 288). */
    con_putc(ROW_KEYS, 12, 'w');        con_dec(ROW_KEYS, 13, hdmi_out_w, 4);
    {
        u16 sta;
        SR(DISPLAY_FORMAT_ADDR_PORT) = DF_DISPLAY_HOR_ACT_STA_H; sta = (u16)SR(DISPLAY_FORMAT_DATA_PORT) << 8;
        SR(DISPLAY_FORMAT_ADDR_PORT) = DF_DISPLAY_HOR_ACT_STA_L; sta |= SR(DISPLAY_FORMAT_DATA_PORT);
        con_dec(ROW_KEYS, 18, sta, 4);
    }
    con_puts(ROW_KEYS, 24, "key");      con_hex8(ROW_KEYS, 28, key_presses);
    con_puts(ROW_KEYS, 30, "ui");       con_putc(ROW_KEYS, 33, "SID"[ui_mode]);
    con_putc(ROW_KEYS, 35, 'S');        con_hex8(ROW_KEYS, 36, stack_peak());   /* FF = stack hit the top */
    /* Display clock + capture timing: PLL multiplier, eDP MVID (the real pixel
     * clock), how the last multiplier change went, the frequency count the
     * last mode set used and the live one, and the programmed IHS delay. */
    /* display PLL M, then the eDP MVID - pixel clock / link clock x 32768, so
     * f_dclk = MVID x 270 MHz / 32768 (1080p60: ~475C-4769 = 150.5 MHz).  It is
     * the independent check that the PLL really produced what was asked for. */
    con_putc(ROW_EDP, 0, 'M');          con_hex8(ROW_EDP, 1, display_dpll_m_get());
    con_puts(ROW_EDP, 4, "mv");         con_hex8(ROW_EDP, 6, edp_stat.mvid[0]);
    con_hex8(ROW_EDP, 8, edp_stat.mvid[1]); con_hex8(ROW_EDP, 10, edp_stat.mvid[2]);
    con_putc(ROW_EDP, 12, "-shx"[hdmi_stat.dclk_path & 3]);   /* PLL change: - none, s soft, h hard, x failed */
    con_puts(ROW_EDP, 14, "cnt");       con_hex16(ROW_EDP, 18, hdmi_stat.tmds_count);
    con_hex16(ROW_EDP, 23, hdmi_freq_count());
    /* IHS delay as programmed (001E bit 2 = bit 9, bit 0 = bit 8, 001D low byte).
     * 1080p 0106, 720p 0160, 1024x768 012E. */
    {
        u8 hv = SR(M1_VGIP_HV_DELAY);
        u16 ihs = (u16)(((hv & M1_VGIP_HV_DELAY_IHS_DLY) ? 0x200 : 0) | ((hv & M1_VGIP_HV_DELAY_IHS_DLY_) ? 0x100 : 0)
                        | SR(M1_IHS_DELAY));
        con_putc(ROW_EDP, 29, 'h');
        con_hex16(ROW_EDP, 30, ihs);
    }
    /* Boundary search, decimal: before the mode set (S) and ~1 s into ACTIVE
     * (A) - H start and V start each, then the active size from A, then
     * whether each completed.  1080p should read 271 43, size 1920x1080. */
    {
        const struct hdmi_bound *bs, *ba;
        u8 oks, oka;

        bs = source_bound(0, &oks);
        ba = source_bound(1, &oka);
        con_puts(ROW_LINK, 0, "bS");
        con_dec(ROW_LINK, 3, bs->hs, 4);
        con_dec(ROW_LINK, 8, bs->vs, 4);
        con_puts(ROW_LINK, 13, "bA");
        con_dec(ROW_LINK, 16, ba->hs, 4);
        con_dec(ROW_LINK, 21, ba->vs, 4);
        con_dec(ROW_LINK, 26, (u16)(ba->he - ba->hs + 1), 4);
        con_dec(ROW_LINK, 31, (u16)(ba->ve - ba->vs + 1), 4);
        con_putc(ROW_LINK, 36, (char)('0' + oks));
        con_putc(ROW_LINK, 37, (char)('0' + oka));
        con_hex8(ROW_LINK, 38, source_remeasures());   /* 00 unless the safety net fired */
    }
    con_puts(ROW_DISP, 0, "0028");      con_hex8(ROW_DISP, 5, SR(VIDEO_DISPLAY_CONTROL_REG));
    con_puts(ROW_DISP, 8, "31C0");      con_hex8(ROW_DISP, 13, SR(I_DB_CTRL));
    con_puts(ROW_DISP, 16, "000C");     con_hex8(ROW_DISP, 21, SR(WATCH_DOG_CTRL0));
    con_puts(ROW_DISP, 24, "st");       con_hex8(ROW_DISP, 27, SR(STATUS0)); con_hex8(ROW_DISP, 29, SR(STATUS1));
    SR(STATUS0) = 0xFF; SR(STATUS1) = 0xFF;                    /* W1C: show what happened since the last tick */
    /* STATUS0/1 as captured at the last watchdog trip: which armed condition
     * fired. */
    con_puts(ROW_DISP, 32, "wd");       con_hex8(ROW_DISP, 35, source_trip_st0());
    con_hex8(ROW_DISP, 38, source_trip_st1());
    con_puts(ROW_STREAM, 0, "hdmi");    con_hex8(ROW_STREAM, 5, hdmi_stat.result);
    con_puts(ROW_STREAM, 8, "tmds");    con_dec(ROW_STREAM, 13, hdmi_stat.f_tmds, 4);   /* 0.1 MHz */
    con_puts(ROW_STREAM, 18, "72A4");   con_hex8(ROW_STREAM, 23, SR(TMDS_CTRL));
    con_puts(ROW_STREAM, 26, "sr");     con_hex8(ROW_STREAM, 29, SR(HDMI_SR));
    /* source state machine: I/L/S/A (idle, locking, settling, active), whether
     * the watchdogs are armed ('A' armed, 'a' allowed but not yet, '-' valve shut), and
     * the trip count.  The state is the one held when HDMI mode was left, since
     * reaching this screen detaches the machine. */
    con_puts(ROW_STREAM, 32, "src");    con_putc(ROW_STREAM, 36, "ILSA"[source_state()]);
    con_putc(ROW_STREAM, 37, "-aA"[source_armed()]);   /* - valve shut, a allowed, A armed */
    con_hex8(ROW_STREAM, 38, source_trips());
    if ((++slow & 0x0F) == 0) {
        /* slow items: D-domain CRC (started one slow tick ago), sink link status over AUX */
        con_puts(ROW_CRC, 0, "dcrc");
        con_hex16(ROW_CRC, 5, display_crc_read() >> 16);
        con_hex16(ROW_CRC, 9, display_crc_read() & 0xFFFF);
        /* What the input is, in decimal: measured htotal/vtotal, then the
         * active size those totals resolve to, then whether the lookup matched. */
        {
            const struct hdmi_timing *m = source_seen();
            con_putc(ROW_CRC, 14, 'm');
            con_dec(ROW_CRC, 16, m->htotal, 4);
            con_dec(ROW_CRC, 21, m->vtotal, 4);
            con_dec(ROW_CRC, 26, m->hactive, 4);
            con_dec(ROW_CRC, 31, m->vactive, 4);
            con_putc(ROW_CRC, 36, (char)('0' + m->active_ok));
        }
        display_crc_start();
        edp_read_link_status();
    }
}
#endif

/* Alive indicator: the status LED blinks (500 ms on / 500 ms off, via tb_ms()
 * so it holds on both sides of the PLL switch) in
 * the current status color: amber = idle, green = link trained, red = test
 * failed.  Time base = Timer0 overflows (see timebase.h). */
static void led_show(u8 on)
{
    if (!on)            { BOARD_LED_OFF(); return; }
    /* Powersave: a short green flash every 2 s.  NOT amber: the red half
     * (0xFE2A) is more than an LED - toggling it blinks the backlight - so it
     * is only ever driven during boot. */
    if (led_color == 1 && source_asleep() && (++led_slow & 3)) { BOARD_LED_OFF(); return; }
    if (led_color == 1) { BOARD_LED_GREEN(); return; }
    if (led_color == 2) { BOARD_LED_RED(); return; }
    BOARD_LED_AMBER();
}

void main(void)
{
    u16 divider = 0, ticks = 0;

    /* An ISP-exit reset (every flash, every host reset) restarts the 8051 only:
     * the scaler's PLLs and the eDP PHY keep the previous run's state, and the
     * panel PLL may then not lock (EDP_LT_ERR_PLL until a power cycle).  A
     * watchdog reset clears it.  The watchdog enable
     * (MCU_CLK_CONTROL_1 bit 7) is SET at power-up and after a watchdog reset,
     * and board_early_init() clears it - so clear here means "warm ISP-exit
     * restart": arm it and let it reset the chip properly.  Set before the spin,
     * so the next boot always goes on. */
    if (!(SR(MCU_CLK_CONTROL_1) & MCU_CLK_CONTROL_1_WDT_EN_3)) {
        SR_SET(MCU_CLK_CONTROL_1, MCU_CLK_CONTROL_1_WDT_EN_3);
        for (;;)
            ;
    }
    xinit_copy();              /* FIRST: SDCC's own XDATA init misses on this core (startup.c) */
    code_page_shadow = 0;      /* bank 0 mapped (we run from it); the banked-call shadow agrees */
    SR(PBANK_SWITCH) = 0;      /* CODE_PAGE */
#if defined(CONFIG_DEBUG_CONSOLE) || defined(CONFIG_DEBUG_VCP)
    stack_paint();             /* before anything deep runs - see stack_peak() */
#endif
    scaler_soft_reset();       /* clean scaler after an ISP-exit reboot (startup.c) */
    board_early_init();        /* clocks, cache, XDATA map, pinshare (proven) */
    dp_hpd(0);                 /* USB-C hot-plug (P1.4) resets HIGH: a phone would talk to an unready port */
    BOARD_LED_RED();           /* main() reached */
    dbgmon_init();
    con_init();
    dbgmon_checkpoint(CP_MAIN_LOOP);

    /* Present the display to the GPU.  Order matters: bring the DDC
     * channels up and load the EDID BEFORE asserting HPD, so the GPU's
     * first EDID read after hot-plug hits a ready buffer. */
    rtd_ddc_external_enable();
    rtd_serve_edid();
    board_gpio_init();         /* golden GPIO levels (HDMI HPD is P1.1: resets high, never written) */

    dbgmon_checkpoint(0x30);
    BOARD_LED_AMBER();         /* presenting to GPU; DDC-ISP recovery safe */
    tb_init();                 /* Timer0 free-running: the one timebase (see timebase.h) */

    /* ISP break-in window: the recovery-safe state (HDMI HPD + EDID up, DDC
     * alive) BEFORE the bring-up runs, so a host can enter ISP and reflash if the
     * bring-up wedges the chip. */
    dbgmon_checkpoint(0x31);
    /* Still on the crystal clock, where one delay_ms unit is ~6.8 ms
     * (delay.h), so this is ~2.4 s.  It also gives the board settling time
     * before edp_panel_power_cycle() drives a real panel-VCC edge; a shorter
     * window has failed, so do not shorten it.
     * A host needs only enough time to notice the chip on the bus and enter
     * ISP; rtdflash --race exists for the hostile case. */
    delay_ms(360);
    boot_mark[BM_ISP] = tb_now();
    dbgmon_checkpoint(0x32);
    /* Past the recovery window: DDC is up, the EDID is served, HPD is asserted
     * and a host has had its second to break in.  Only now may anything touch
     * the flash asset partition - see hal/realtek/rtd2556/flash_assets.h. */
    assets_enable();
    /* Settings from flash (defaults when there is no valid record).  After
     * the ISP window like everything else that reads the flash beyond the
     * code bank; flash_rw.c only READS here. */
    settings_load();
    dbgmon_checkpoint(0x50);
    /* Before the first banked call (bringup): every bank must be from this
     * build.  If not - a half-written flash, a blank bank - stop here with the
     * recovery path up (EDID, ISP) rather than jump into the wrong code. */
    if (!banks_consistent()) {
        BOARD_LED_RED();
        for (;;)
            ;
    }
    dbgmon_checkpoint(0x51);
    bringup();
    if (display_up) {
        audio_init();               /* DAC + outputs up, muted; the HDMI side starts with each mode set */
        settings_apply_all();       /* gamma, dither, scaling, volume etc. as loaded */
    }
    /* DDC/CI after the recovery window, like everything else that is not
     * needed to reflash.  It never stretches SCL (see core/mccs.h), so it
     * cannot hold the DDC bus the recovery path shares. */
    mccs_init();
    dbgmon_checkpoint(0x53);
    /* USB-C (DisplayPort over D1): port, link-training handler, hot-plug,
     * then the automatic stream / input selection.  After the recovery
     * window like everything else not needed to reflash; it holds the
     * hot-plug low for 0.5 s first. */
    if (display_up) {
        hdcp_init();                /* HDCP 1.4 keys, both inputs - before the DP hot-plug advertises them */
        dbgmon_checkpoint(0x54);
        dp_rx_start();
        dbgmon_checkpoint(0x55);
        source_dp_arm();
    }

    keys_init();
    dbgmon_checkpoint(0x56);
    {
        u8 polls = 0, min_a0 = 0xFF, led_on = 1, a0 = 0xFF, a1 = 0xFF;
        u16 led_t = 0;
        /* Hoisted out of the loop ON PURPOSE.  tb_ms() is a 32-bit multiply
         * and a 32-bit divide; calling it per spin slows the whole main loop
         * (debug console refresh ~10x slower, key polling and the source tick
         * with it).  The calibration is one-shot, so this never changes. */
        u16 led_period = tb_ms(500);
        u8 splash_active = 1;      /* auto-switch splash -> HDMI while a source may appear */
        u16 splash_t0 = 0;         /* Timer0 overflows since bring-up, for the ~60 s ceiling */
        for (;;) {
            ui_t0ovf = tb_now();       /* the ONLY consumer of TF0 is tb_service() */
            if ((u16)(ui_t0ovf - led_t) >= led_period) {
                led_t = ui_t0ovf;
                led_on = !led_on;
                led_show(led_on);
            }
            if ((++divider & 0x03FF) == 0) {
                ticks++;
                dbgmon_heartbeat();
                settings_autosave(ui_t0ovf);
                /* key-poll diagnostics: word 1 = completed polls | lowest A0 seen | ui */
                dbgmon_diag(1, polls, min_a0, ui_mode);
#ifdef CONFIG_DEBUG_CONSOLE
                if (display_up && ui_mode == UI_DEBUG && !menu_active()) {
                    con_refresh(ticks, ui_t0ovf, a0, a1, min_a0);
                    con_flush();
                }
#endif
                /* Boot logo -> the input view when video appears on HDMI or USB-C (up to 60 s,
                 * then stop retrying).  The source state machine picks which one shows. */
                if (display_up && splash_active && ui_mode == UI_SPLASH) {
                    if ((u16)(ui_t0ovf - splash_t0) < tb_ms(60000)) {
                        if (hdmi_stat.result != HDMI_OK && hdmi_clock_present())
                            hdmi_rx_start();               /* a source appeared: bring it up */
                        if (hdmi_stat.result == HDMI_OK || source_dp_video()) {
                            ui_set_mode(UI_INPUT);
                            splash_active = 0;
                        }
                    } else {
                        /* 60 s ceiling: hand over to the input view - the source
                         * state machine then says "No signal", powersaves, and
                         * still picks up a source that appears later */
                        ui_set_mode(UI_INPUT);
                        splash_active = 0;
                    }
                }
            }
            /* rate-limited key poll (ACONTROL read mid-conversion stalls ~1.5ms) */
            if ((divider & 0x3F) == 0) {
                u8 key = keys_poll();

                a0 = keys_a0; a1 = keys_a1;
                if (a0 < min_a0) min_a0 = a0;
                polls++;
                dbgmon_diag(15, a0, a1, ui_mode);
                if (key != KEY_NONE) {
                    splash_active = 0;             /* manual control stops the auto-switch */
                    key_presses++;
                    source_wake(ui_t0ovf);         /* a blanked panel must light before the menu draws on it */
                    if (menu_active()) {
                        menu_key(key);
                    }
#ifdef CONFIG_DEBUG_CONSOLE
                    else if (ui_mode == UI_DEBUG) {
                        /* debug console: any key = back to the picture; ENTER also
                         * opens the menu there (over the console it would be redrawn
                         * by con_refresh every heartbeat, on the red debug background) */
                        ui_set_mode(UI_INPUT);
                        if (key == KEY_ENTER)
                            menu_open();
                    }
#endif
                    else if (key == KEY_ENTER) {
                        /* off the splash first: the key just stopped its auto-switch,
                         * and a menu opened over the splash would leave the panel
                         * black once it closed */
                        if (display_up && ui_mode == UI_SPLASH)
                            ui_set_mode(UI_INPUT);
                        if (display_up)
                            menu_open();
                    } else if (display_up) {
                        if (ui_mode == UI_SPLASH)
                            ui_set_mode(UI_INPUT);  /* the hotkeys act on the input view */
                        menu_hotkey(key);          /* UP/DOWN volume, POWER input picker */
                    }
                }
                dbgmon_diag(3, ui_mode, key_presses, a1);
                source_tick(ui_t0ovf, (u8)(display_up && ui_mode == UI_INPUT));
                menu_tick(ui_t0ovf);
#ifdef CONFIG_FEATURE_DCR
                dcr_tick(ui_t0ovf);
#endif
#ifdef CONFIG_DEBUG_CONSOLE
                if (menu_take_request() == MENU_REQ_DEBUG)
                    ui_set_mode(UI_DEBUG);
                /* the menu closed (key, timeout) over the debug console: give it back */
                if (menu_was_up && !menu_active() && ui_mode == UI_DEBUG)
                    con_enable(32, 24);
                menu_was_up = menu_active();
#endif
                audio_tick(ui_t0ovf);
                if (menu_active() || menu_popup_active())
                    con_flush();
            }
            /* every spin: the host reads the reply ~50 ms after asking, and
             * there is no clock stretch to buy more time (core/mccs.h) */
            {
                mccs_poll();
                dp_rx_poll();
                if (mccs_ui_request != 0xFF) {
                    if (display_up && mccs_ui_request != ui_mode) {
                        splash_active = 0;
                        if (menu_active())
                            menu_close();
                        ui_set_mode(mccs_ui_request);
                    }
                    mccs_ui_request = 0xFF;
                }
            }
        }
    }
}
