/* osdsim host API (osdsim_api.h) and the chip-independent firmware glue: the
 * asset image behind flash_read(), time, and the input the menu reports. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include "osdsim_api.h"
#include "settings.h"
#include "menu.h"
#include "keys.h"
#include "console.h"
#include "osd.h"
#include "assets.h"
#include "hdmi_rx.h"
#include "dp_rx.h"
#include "source.h"
#include "gui.h"

volatile u8 osdsim_sr[65536];
volatile unsigned char EA, EX0, IT0, IE0, P1_4, P3_2, SP, TF0, TR0, ET0;

const u8 *osdsim_assets;
u32 osdsim_assets_len;

int osdsim_load_assets(const char *path)
{
    FILE *f = fopen(path, "rb");
    long n;
    u8 *b;

    if (!f)
        return -1;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    b = malloc((size_t)n);
    if (!b || fread(b, 1, (size_t)n, f) != (size_t)n) {
        fclose(f);
        free(b);
        return -1;
    }
    fclose(f);
    free((void *)osdsim_assets);
    osdsim_assets = b;
    osdsim_assets_len = (u32)n;
    return 0;
}

/* flash_rw.h: only the asset partition is readable here (lang.c's strings) */
void flash_read(u32 addr, u8 *dst, u16 n)
{
    while (n--) {
        u32 o = addr++ - ASSET_BASE;
        *dst++ = (o < osdsim_assets_len) ? osdsim_assets[o] : 0xFF;
    }
}

/* ---- time: one tick = 1 ms (tb_ms is the identity) ---- */
static u16 now;
u16 tb_now(void) { return now; }
u16 tb_ms(u16 ms) { return ms; }

/* ---- the input, as set by osdsim_set_signal ---- */
struct hdmi_status hdmi_stat;
struct dclk_dbg hdmi_dclk_dbg;
u8 rx_port_dp;
struct dp_rx_log dp_log;
static struct hdmi_timing sig;
static u8 sig_up;

void osdsim_set_signal(u8 dp, u8 up, u16 w, u16 h, u16 hz_x10, u16 htotal, u16 vtotal, u16 f_tmds_x10)
{
    rx_port_dp = dp;
    sig_up = up;
    memset(&sig, 0, sizeof(sig));
    sig.hactive = w;
    sig.vactive = h;
    sig.refresh_x10 = hz_x10;
    sig.htotal = htotal;
    sig.vtotal = vtotal;
    sig.active_ok = 1;
    hdmi_stat.f_tmds = f_tmds_x10;
    dp_log.msa_htt = htotal;
    dp_log.msa_vtt = vtotal;
    dp_log.msa_hwd = w;
    dp_log.msa_vht = h;
}

const struct hdmi_timing *source_seen(void) { return &sig; }
u8 source_picture(void) { return sig_up; }
u8 source_input(void) { return rx_port_dp ? SOURCE_IN_DP : SOURCE_IN_HDMI; }
u8 hdmi_measure(struct hdmi_timing *t) { *t = sig; return sig_up; }

/* ---- API ---- */
void osdsim_reset(void)
{
    if (menu_active())
        menu_close();
    menu_popup_cancel();
    gui_hide();                         /* gui.c remembers what it showed: forget it */
    memset((void *)osdsim_sr, 0, sizeof(osdsim_sr));
    osd_init();
    con_init();
    settings_defaults();
    gui_theme_apply();
    now = 1;
}

/* main.c's routing: ENTER opens the menu, the others are the hotkeys; with the
 * menu up everything goes to it */
void osdsim_key(u8 key)
{
    now += 50;
    if (menu_active()) {
        menu_key(key);
        return;
    }
    if (key == KEY_ENTER)
        menu_open();
    else
        menu_hotkey(key);
}

void osdsim_open_menu(void) { menu_open(); }

void osdsim_close(void)
{
    if (menu_active())
        menu_close();
    else
        osdsim_tick(30000);             /* let a popup expire */
}

void osdsim_popup_mode(void) { menu_popup_mode(now); }
void osdsim_popup_nosignal(void) { menu_popup_nosignal(now); }

void osdsim_tick(u16 ms)
{
    while (ms) {
        u16 step = ms > 500 ? 500 : ms;
        now += step;
        ms -= step;
        menu_tick(now);
    }
}

u8 osdsim_menu_active(void) { return menu_active(); }

/* ---- settings by name ---- */
#define F(n, t) { #n, offsetof(struct ob_settings, n), sizeof(((struct ob_settings *)0)->n), t }
static const struct { const char *name; size_t off, size; u8 is_signed; } fields[] = {
    F(brightness, 1), F(contrast, 1), F(saturation, 0), F(ctemp, 0), F(sharpness, 0), F(gamma, 0),
    F(idither, 0), F(ddither, 0), F(boot_logo, 0), F(res_popup, 0), F(backlight, 0), F(bl_level, 0),
    F(scaling, 0), F(powersave, 0), F(ddcci, 0), F(wd_blank, 0), F(key_repeat, 0), F(volume, 0),
    F(mute, 0), F(input, 0), F(menu_time, 0), F(osd_h, 0), F(osd_v, 0), F(osd_scale, 0),
    F(adjust_popup, 0), F(language, 0), F(low_blue, 0), F(usage_h, 0), F(usage_20, 0),
    F(user_r, 0), F(user_g, 0), F(user_b, 0), F(dcr, 0), F(theme, 0),
};
#define NFIELDS (sizeof(fields) / sizeof(fields[0]))

static int find(const char *name)
{
    size_t i;

    for (i = 0; i < NFIELDS; i++)
        if (!strcmp(fields[i].name, name))
            return (int)i;
    return -1;
}

int osdsim_setting_set(const char *name, int value)
{
    int i = find(name);
    u8 *p;

    if (i < 0)
        return -1;
    p = (u8 *)&ob_settings + fields[i].off;
    if (fields[i].size == 2) {
        u16 v = (u16)value;
        memcpy(p, &v, 2);
    } else {
        *p = (u8)value;
    }
    if (!strcmp(name, "theme"))
        gui_theme_apply();          /* as the menu does: the next show uses it */
    return 0;
}

int osdsim_setting_get(const char *name, int *value)
{
    int i = find(name);
    u8 *p;

    if (i < 0)
        return -1;
    p = (u8 *)&ob_settings + fields[i].off;
    if (fields[i].size == 2) {
        u16 v;
        memcpy(&v, p, 2);
        *value = v;
    } else {
        *value = fields[i].is_signed ? (int)(s8)*p : *p;
    }
    return 0;
}

const char *osdsim_setting_name(int i)
{
    return (i >= 0 && (size_t)i < NFIELDS) ? fields[i].name : NULL;
}
