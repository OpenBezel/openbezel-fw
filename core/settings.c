/* User settings - see settings.h. */
#include "settings.h"
#include "board.h"
#include "rtd2556_regs.h"
#include "pipeline.h"
#include "hdmi_rx.h"
#include "display.h"
#include "backlight.h"
#include "source.h"
#include "settings_store.h"
#include "assets.h"
#include "timebase.h"
#include "keys.h"
#include "audio.h"
#include "lang_ids.h"
#include "gui.h"

/* Kconfig "Default settings" (config.h: y = defined) */
#ifdef CONFIG_DEFAULT_BOOT_LOGO
#define DEF_BOOT_LOGO 1
#else
#define DEF_BOOT_LOGO 0
#endif
#ifdef CONFIG_DEFAULT_MENU_2X
#define DEF_MENU_2X 1
#else
#define DEF_MENU_2X 0
#endif
#ifdef CONFIG_DEFAULT_SLIDER_POPUP
#define DEF_SLIDER_POPUP 1
#else
#define DEF_SLIDER_POPUP 0
#endif

struct ob_settings ob_settings;

void settings_defaults(void)
{
    ob_settings.brightness = 0;
    ob_settings.contrast = 0;
    ob_settings.saturation = PIPELINE_SAT_NORMAL;
    ob_settings.ctemp = PIPELINE_CTEMP_NORMAL;
    ob_settings.sharpness = PIPELINE_SHARPNESS_DEFAULT;
    ob_settings.gamma = PIPELINE_GAMMA_DEFAULT;
    ob_settings.idither = 1;
    ob_settings.ddither = 1;        /* display_freerun_init() enables it; keep the menu in step */
    ob_settings.boot_logo = DEF_BOOT_LOGO;
    ob_settings.res_popup = 1;
    ob_settings.backlight = 1;
    ob_settings.bl_level = 100;
    ob_settings.scaling = HDMI_SCALE_FIT;   /* 1080p is unaffected; 4:3 / 5:4 / 16:10 keep their shape */
    ob_settings.powersave = SETTINGS_POWERSAVE_DEFAULT;
    ob_settings.ddcci = 1;
    ob_settings.wd_blank = 0;
    ob_settings.key_repeat = SETTINGS_KEYREP_NORM;
    ob_settings.volume = 80;
    ob_settings.mute = 0;
    ob_settings.input = SETTINGS_INPUT_AUTO;
    ob_settings.menu_time = SETTINGS_MENUTIME_DEFAULT;
    ob_settings.osd_h = 50;
    ob_settings.osd_v = 50;
    ob_settings.osd_scale = DEF_MENU_2X;
    ob_settings.adjust_popup = DEF_SLIDER_POPUP;
    ob_settings.language = LANG_DEFAULT;    /* Kconfig LANG_DEFAULT (lang_ids.h) */
    ob_settings.theme = GUI_THEME_DEFAULT_ID;   /* Kconfig THEME_DEFAULT (theme.h) */
    ob_settings.low_blue = 0;
    ob_settings.usage_h = 0;
    ob_settings.usage_20 = 0;
    ob_settings.user_r = 100;
    ob_settings.user_g = 100;
    ob_settings.user_b = 100;
    ob_settings.dcr = 0;
}

/* The scale-up coefficient SRAM is in the I-domain, so like the rest of the
 * input path it only accepts writes while imd_db_en is clear.  Reloading it
 * with video running is safe as long as that bit is cleared around the write. */
void settings_apply_sharpness(void)
{
    u8 db = SR(I_DB_CTRL);

    SR(I_DB_CTRL) = db & (u8)~I_DB_CTRL_IMD_DB_EN;
    pipeline_scaleup_coefs_load(ob_settings.sharpness);
    SR(I_DB_CTRL) = db;
}

void settings_apply_idither(void)
{
    u8 db = SR(I_DB_CTRL);

    SR(I_DB_CTRL) = db & (u8)~I_DB_CTRL_IMD_DB_EN;
    pipeline_idither_load(8, ob_settings.idither);
    SR(I_DB_CTRL) = db;
}

/* The display dither tables are loaded once at bring-up and never reloaded, so
 * unlike the input side this is just the enable bit. */
void settings_apply_ddither(void)
{
    display_dither(ob_settings.ddither);
}



/* Gamma lives in the display domain, not the I-domain, so it needs no
 * imd_db_en dance - and pipeline_gamma_load() disables the block across the
 * reload itself so a half-written LUT is never on screen. */
void settings_apply_gamma(void)
{
    pipeline_gamma_load(ob_settings.gamma);
}

void settings_apply_levels(void)
{
    pipeline_user_gains(ob_settings.user_r, ob_settings.user_g, ob_settings.user_b);
    pipeline_picture_levels(ob_settings.brightness, ob_settings.contrast, ob_settings.ctemp,
                            ob_settings.low_blue);
}

void settings_apply_user_gain(void)
{
    ob_settings.ctemp = PIPELINE_CTEMP_USER;
    settings_apply_levels();
}

/* At normal saturation the matrix is bypassed rather than loaded with an
 * identity: identity *should* be a perfect pass-through, but bypassing keeps
 * the default path bit-identical to the one that is known good. */
void settings_apply_saturation(void)
{
    pipeline_saturation(ob_settings.saturation);
}

__xdata u8 dcr_cut;
static __xdata u16 bl_pct;
static u8 bl_gate = 1;          /* powersave: 0 = dark regardless of the setting */
static u8 bl_host = 1;          /* MCCS D6h power mode: 0 = host put the display in standby */

void settings_apply_backlight(void)
{
    /* Active low: 1 = off.  The panel lights only when the user asked for it
     * AND neither powersave nor the host (MCCS D6h) is holding it down; the
     * level itself is the PWM5 duty (backlight.c). */
    BOARD_BACKLIGHT_OFF = (ob_settings.backlight && bl_gate && bl_host) ? 0 : 1;
    bl_pct = ob_settings.bl_level;
    bl_pct *= 100 - dcr_cut;            /* dynamic contrast (core/dcr.c) */
    bl_pct /= 100;
    backlight_pwm_level((u8)bl_pct);
}

void settings_backlight_gate(u8 on)
{
    bl_gate = on;
    settings_apply_backlight();
}

void settings_backlight_host(u8 on)
{
    bl_host = on;
    settings_apply_backlight();
}

void settings_apply_scaling(void)
{
    hdmi_scaling_set(ob_settings.scaling);
    source_remode();
}

static const __code u16 keyrep_ms[SETTINGS_KEYREP_COUNT][2] = {
    { 0, 0 }, { 600, 200 }, { 400, 100 }, { 250, 50 }
};

void settings_apply_keyrepeat(void)
{
    u8 k = ob_settings.key_repeat < SETTINGS_KEYREP_COUNT ? ob_settings.key_repeat : SETTINGS_KEYREP_NORM;

    keys_set_repeat(tb_ms(keyrep_ms[k][0]), tb_ms(keyrep_ms[k][1]));
}

void settings_apply_audio(void)
{
    audio_set(ob_settings.volume, ob_settings.mute);    /* a no-op until audio_init() has run */
}

void settings_apply_all(void)
{
    settings_apply_audio();
    settings_apply_keyrepeat();
    settings_apply_scaling();
    settings_apply_sharpness();
    settings_apply_idither();
    settings_apply_ddither();
    settings_apply_gamma();
    settings_apply_levels();
    settings_apply_saturation();
    settings_apply_backlight();
    gui_theme_apply();
}

void settings_reset(void)
{
    u16 h = ob_settings.usage_h;
    u8 m = ob_settings.usage_20;

    settings_defaults();
    ob_settings.usage_h = h;            /* an odometer, not a setting */
    ob_settings.usage_20 = m;
    settings_apply_all();
}

static u8 clamp_u8(u8 v, u8 lo, u8 hi)
{
    return (v < lo) ? lo : ((v > hi) ? hi : v);
}

static s8 clamp_s8(s8 v, s8 lo, s8 hi)
{
    return (v < lo) ? lo : ((v > hi) ? hi : v);
}

void settings_sanitize(void)
{
    ob_settings.brightness = clamp_s8(ob_settings.brightness, -PIPELINE_CB_RANGE, PIPELINE_CB_RANGE);
    ob_settings.contrast = clamp_s8(ob_settings.contrast, -PIPELINE_CB_RANGE, PIPELINE_CB_RANGE);
    ob_settings.saturation = clamp_u8(ob_settings.saturation, 0, PIPELINE_SAT_MAX);
    ob_settings.ctemp = clamp_u8(ob_settings.ctemp, 0, PIPELINE_CTEMP_COUNT - 1);
    ob_settings.sharpness = clamp_u8(ob_settings.sharpness, 0, PIPELINE_SHARPNESS_MAX);
    ob_settings.gamma = clamp_u8(ob_settings.gamma, 0, ASSET_GAMMA_COUNT);
    ob_settings.idither = ob_settings.idither ? 1 : 0;
    ob_settings.ddither = ob_settings.ddither ? 1 : 0;
    ob_settings.boot_logo = ob_settings.boot_logo ? 1 : 0;
    ob_settings.res_popup = ob_settings.res_popup ? 1 : 0;
    ob_settings.backlight = ob_settings.backlight ? 1 : 0;
    ob_settings.bl_level = clamp_u8(ob_settings.bl_level, 0, 100);
    ob_settings.scaling = clamp_u8(ob_settings.scaling, 0, HDMI_SCALE_COUNT - 1);
    ob_settings.powersave = clamp_u8(ob_settings.powersave, 0, SETTINGS_POWERSAVE_COUNT - 1);
    ob_settings.ddcci = ob_settings.ddcci ? 1 : 0;
    ob_settings.wd_blank = ob_settings.wd_blank ? 1 : 0;
    ob_settings.key_repeat = clamp_u8(ob_settings.key_repeat, 0, SETTINGS_KEYREP_COUNT - 1);
    ob_settings.volume = clamp_u8(ob_settings.volume, 0, 100);
    ob_settings.mute = ob_settings.mute ? 1 : 0;
    ob_settings.input = clamp_u8(ob_settings.input, 0, SETTINGS_INPUT_COUNT - 1);
    ob_settings.menu_time = clamp_u8(ob_settings.menu_time, 0, SETTINGS_MENUTIME_COUNT - 1);
    ob_settings.osd_h = clamp_u8(ob_settings.osd_h, 0, 100);
    ob_settings.osd_v = clamp_u8(ob_settings.osd_v, 0, 100);
    ob_settings.osd_scale = ob_settings.osd_scale ? 1 : 0;
    ob_settings.adjust_popup = ob_settings.adjust_popup ? 1 : 0;
    if (ob_settings.language >= LANG_COUNT)
        ob_settings.language = LANG_DEFAULT;
    if (ob_settings.low_blue >= SETTINGS_LOWBLUE_COUNT)
        ob_settings.low_blue = 0;
    if (ob_settings.usage_20 > 2)
        ob_settings.usage_20 = 0;
    ob_settings.user_r = clamp_u8(ob_settings.user_r, 0, 100);
    ob_settings.user_g = clamp_u8(ob_settings.user_g, 0, 100);
    ob_settings.user_b = clamp_u8(ob_settings.user_b, 0, 100);
    ob_settings.dcr = ob_settings.dcr ? 1 : 0;
    /* a setting saved by a build that had the feature: off in one without it */
#ifndef CONFIG_FEATURE_DCR
    ob_settings.dcr = 0;
#endif
#ifndef CONFIG_FEATURE_LOW_BLUE
    ob_settings.low_blue = 0;
#endif
#ifndef CONFIG_FEATURE_USER_GAINS
    if (ob_settings.ctemp == PIPELINE_CTEMP_USER)
        ob_settings.ctemp = PIPELINE_CTEMP_NORMAL;
#endif
}

u8 settings_load(void)
{
    u8 r;

    settings_defaults();
    r = store_load(&ob_settings);
    if (r != STORE_OK) {
        settings_defaults();            /* store_load may have left it partial */
        return r;
    }
    settings_sanitize();
    ob_settings.backlight = 1;
    return r;
}

#define AUTOSAVE_MS 3000

/* Usage time (MCCS C0h): minutes with the panel lit, booked into the settings every
 * 20 minutes - the autosave then stores them, three log records an hour.  A power
 * cut loses the minutes since. */
static __xdata u16 use_t, use_step;
static __xdata u8 use_min, use_init;

static void usage_tick(u16 t0ovf)
{
    if (!use_init) {
        use_init = 1;
        use_t = t0ovf;
        use_step = tb_ms(60000);
        return;
    }
    if ((u16)(t0ovf - use_t) < use_step)
        return;
    use_t += use_step;
    if (BOARD_BACKLIGHT_OFF)            /* dark: powersave, host standby, user off */
        return;
    use_min++;
    if (use_min < 20)
        return;
    use_min = 0;
    ob_settings.usage_20++;
    if (ob_settings.usage_20 < 3)
        return;
    ob_settings.usage_20 = 0;
    ob_settings.usage_h++;
}

void settings_autosave(u16 t0ovf)
{
    static __xdata struct ob_settings seen;
    static u16 changed_t, delay;
    static u8 pending;
    u8 i, diff = 0;

    usage_tick(t0ovf);
    for (i = 0; i < sizeof(seen); i++) {
        if (((__xdata u8 *)&seen)[i] != ((__xdata u8 *)&ob_settings)[i]) {
            ((__xdata u8 *)&seen)[i] = ((__xdata u8 *)&ob_settings)[i];
            diff = 1;
        }
    }
    if (diff) {
        changed_t = t0ovf;
        pending = 1;
        return;
    }
    if (!pending)
        return;
    if (!delay)
        delay = tb_ms(AUTOSAVE_MS);     /* after calibration; one-shot */
    if ((u16)(t0ovf - changed_t) < delay)
        return;
    pending = 0;
    store_save(&ob_settings);           /* a no-op if it matches the last record */
}
