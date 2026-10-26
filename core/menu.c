/* Settings menu + input-mode popup - see menu.h.
 *
 * Two levels: a short top-level list of pages, and one screen of items per
 * page.  UP/DOWN move, ENTER opens a page / changes the highlighted value.
 * Numeric items wrap at their limits, so every control is reachable with the
 * two keys the front panel actually has. */
#include "menu.h"
#include "keys.h"
#include "settings.h"
#include "console.h"
#include "osd.h"
#include "pipeline.h"
#include "assets.h"
#include "hdmi_rx.h"
#include "dp_rx.h"
#include "source.h"
#include "rtd2556_regs.h"
#include "timebase.h"
#include "mccs.h"
#include "gui.h"
#include "lang.h"
#include "picmode.h"
#include "board.h"

/* Console cells are OSD_FONT_W x OSD_FONT_H (12 x 18), so 40 cols = 480 px and
 * 8 rows = 144 px.  Both overlays are centered in the 1920x1080 active area. */

#define POPUP_MS 10000          /* converted through tb_ms(): overflows are not a fixed
                                 * duration across the PLL switch - see timebase.h */

#define ROW_TITLE 0
#define ROW_FIRST 1             /* up to 7 rows of items */
#define MAX_ITEMS GUI_BODY_ROWS  /* core/gui.h; the console has the rows */

/* item kinds */
#define K_PAGE 0                /* opens another page */
#define K_RANGE 1               /* signed -50..+50 */
#define K_COUNT 2               /* 0..limit */
#define K_BOOL 3
#define K_BACK 4
#define K_PCT 5                 /* 0..arg, edited in steps of arg/100 */
#define K_ENUM 6                /* 0..arg, shown as a name from `labels` */
#define K_ACTION 7              /* arg = ACT_*: does something on ENTER */

#define ACT_DEBUG 1             /* hand the screen to the debug console (main.c) */
#define ACT_RESET 2             /* factory defaults - ENTER twice */

#define QUICK_MS 3000           /* hotkey bars close this long after the last key */

/* Text is by string id (S_*, core/lang.h): the menu's language is a setting. */
#define L(id) lang_str(id)

struct item {
    u8 name;                    /* S_* */
    u8 kind;
    u8 arg;                     /* K_PAGE: page index.  K_COUNT/K_PCT/K_ENUM: inclusive max. */
    void *val;                  /* pointer into ob_settings (u8 or s8) */
    void (*apply)(void);
    const __code u8 *labels;    /* K_ENUM only: S_* per value; 0 = the languages' own names */
};

static const __code u8 ctemp_labels[] = { S_V_COOL, S_V_NORMAL, S_V_WARM, S_V_USER };
/* the presets, as fitted in core/mccs.c (MCCS 72h); Off = LUT bypassed */
static const __code u8 gamma_labels[] = { S_V_OFF, S_N_GAMMA18, S_N_GAMMA20, S_N_GAMMA22, S_N_GAMMA24 };
static const __code u8 scaling_labels[] = { S_V_FULL, S_V_FIT };
/* Backlight off after this long with no source - see core/source.c. */
static const __code u8 powersave_labels[] = { S_V_OFF, S_N_5S, S_N_30S, S_N_2MIN };
static const __code u8 keyrep_labels[] = { S_V_OFF, S_V_SLOW, S_V_NORMAL, S_V_FAST };
static const __code u8 input_labels[] = { S_V_AUTO, S_V_PREF_HDMI, S_V_PREF_USBC, S_V_HDMI, S_V_USBC };
static const __code u8 menutime_labels[] = { S_V_OFF, S_N_10S, S_N_30S, S_N_60S };
static const __code u16 menutime_ms[] = { 0, 10000, 30000, 60000 };
static const __code u8 scale_labels[] = { S_N_1X, S_N_2X };
static const __code u8 theme_labels[GUI_THEME_COUNT] = GUI_THEME_LABELS;
/* The theme as an index into theme_labels: the setting itself is its stable id */
static __xdata u8 theme_shown;
static const __code u8 adjust_labels[] = { S_V_INROW, S_V_POPUP };
static const __code u8 lowblue_labels[] = { S_V_OFF, S_V_LOW, S_V_MEDIUM, S_V_HIGH };
#ifdef CONFIG_FEATURE_PICTURE_MODES
static const __code u8 picmode_labels[] = { S_V_CUSTOM, S_V_STANDARD, S_V_MOVIE, S_V_GAME, S_V_TEXT };
/* Picture mode is derived from the settings (picmode.h): draw() refreshes this */
static __xdata u8 pm_shown;
#endif
/* Kconfig features (config.h) take their items out of the pages below */
#ifdef CONFIG_FEATURE_USER_GAINS
#define CTEMP_MAX (PIPELINE_CTEMP_COUNT - 1)            /* ... User */
#else
#define CTEMP_MAX (PIPELINE_CTEMP_USER - 1)             /* Cool, Normal, Warm */
#endif

/* pages */
#define PAGE_MAIN 0
#define PAGE_PICTURE 1
#define PAGE_DISPLAY 2
#define PAGE_SYSTEM 3
#define PAGE_AUDIO 4
#define PAGE_OSD 5
#define PAGE_INFO 6

static const __code struct item page_main[] = {
    { S_T_PICTURE,    K_PAGE, PAGE_PICTURE, 0, 0, 0 },
    { S_T_AUDIO,      K_PAGE, PAGE_AUDIO,   0, 0, 0 },
    { S_T_DISPLAY,    K_PAGE, PAGE_DISPLAY, 0, 0, 0 },
    { S_T_OSD,        K_PAGE, PAGE_OSD,     0, 0, 0 },
    { S_T_SYSTEM,     K_PAGE, PAGE_SYSTEM,  0, 0, 0 },
    { S_T_INFO,       K_PAGE, PAGE_INFO,    0, 0, 0 },
    { S_T_EXIT,       K_BACK, 0, 0, 0, 0 },
};

/* Color temp sits next to Contrast because they are the same hardware: both
 * write the CTS/BRI per-channel gains, which is why they share one apply. */
static const __code struct item page_picture[] = {
#ifdef CONFIG_FEATURE_PICTURE_MODES
    { S_I_PICMODE,    K_ENUM,  PICMODE_COUNT - 1, &pm_shown, 0, picmode_labels },
#endif
    { S_I_BRIGHTNESS, K_RANGE, 0, &ob_settings.brightness, settings_apply_levels, 0 },
    { S_I_CONTRAST,   K_RANGE, 0, &ob_settings.contrast,   settings_apply_levels, 0 },
    { S_I_CTEMP,      K_ENUM,  CTEMP_MAX, &ob_settings.ctemp, settings_apply_levels, ctemp_labels },
#ifdef CONFIG_FEATURE_USER_GAINS
    { S_I_RGAIN,      K_PCT,   100, &ob_settings.user_r, settings_apply_user_gain, 0 },
    { S_I_GGAIN,      K_PCT,   100, &ob_settings.user_g, settings_apply_user_gain, 0 },
    { S_I_BGAIN,      K_PCT,   100, &ob_settings.user_b, settings_apply_user_gain, 0 },
#endif
#ifdef CONFIG_FEATURE_LOW_BLUE
    { S_I_LOWBLUE,    K_ENUM,  SETTINGS_LOWBLUE_COUNT - 1, &ob_settings.low_blue, settings_apply_levels, lowblue_labels },
#endif
    { S_I_SATURATION, K_PCT,   PIPELINE_SAT_MAX, &ob_settings.saturation, settings_apply_saturation },
    { S_I_SHARPNESS,  K_COUNT, PIPELINE_SHARPNESS_MAX, &ob_settings.sharpness, settings_apply_sharpness, 0 },
    { S_I_GAMMA,      K_ENUM,  ASSET_GAMMA_COUNT, &ob_settings.gamma, settings_apply_gamma, gamma_labels },
    { S_I_BACK,       K_BACK, 0, 0, 0, 0 },
};

static const __code struct item page_display[] = {
    { S_I_INPUT,      K_ENUM, SETTINGS_INPUT_COUNT - 1, &ob_settings.input, 0, input_labels },
    { S_I_IDITHER,    K_BOOL, 0, &ob_settings.idither,   settings_apply_idither, 0 },
    { S_I_DDITHER,    K_BOOL, 0, &ob_settings.ddither,   settings_apply_ddither, 0 },
    { S_I_SCALING,    K_ENUM, HDMI_SCALE_COUNT - 1, &ob_settings.scaling, settings_apply_scaling, scaling_labels },
    { S_I_WDBLANK,    K_BOOL, 0, &ob_settings.wd_blank, 0, 0 },
    { S_I_MODEPOPUP,  K_BOOL, 0, &ob_settings.res_popup, 0, 0 },
    { S_I_BACK,       K_BACK, 0, 0, 0, 0 },
};

/* H/V position and Menu size act on the menu itself (edit_step / item_enter - no
 * apply callback: a pointer to banked code would break the banking rules). */
static const __code struct item page_osd[] = {
    { S_I_LANGUAGE,   K_ENUM, LANG_COUNT - 1, &ob_settings.language, 0, 0 },
    { S_I_MENUTIME,   K_ENUM, SETTINGS_MENUTIME_COUNT - 1, &ob_settings.menu_time, 0, menutime_labels },
#if GUI_THEME_COUNT > 1
    { S_I_THEME,      K_ENUM, GUI_THEME_COUNT - 1, &theme_shown, 0, theme_labels },
#endif
    { S_I_MENUSIZE,   K_ENUM, 1, &ob_settings.osd_scale, 0, scale_labels },
    { S_I_SLIDER,     K_ENUM, 1, &ob_settings.adjust_popup, 0, adjust_labels },
    { S_I_HPOS,       K_PCT, 100, &ob_settings.osd_h, 0, 0 },
    { S_I_VPOS,       K_PCT, 100, &ob_settings.osd_v, 0, 0 },
#ifdef CONFIG_DEBUG_CONSOLE
    { S_I_DEBUG,      K_ACTION, ACT_DEBUG, 0, 0, 0 },
#endif
    { S_I_RESET,      K_ACTION, ACT_RESET, 0, 0, 0 },
    { S_I_BACK,       K_BACK, 0, 0, 0, 0 },
};

/* The page's text is drawn by draw_info(); Back is its only item. */
static const __code struct item page_info[] = {
    { S_I_BACK,       K_BACK, 0, 0, 0, 0 },
};
#define INFO_BACK_ROW 9

static const __code struct item page_audio[] = {
    { S_I_VOLUME,     K_PCT, 100, &ob_settings.volume, settings_apply_audio, 0 },
    { S_I_MUTE,       K_BOOL, 0, &ob_settings.mute,   settings_apply_audio, 0 },
    { S_I_BACK,       K_BACK, 0, 0, 0, 0 },
};

static const __code struct item page_system[] = {
    { S_I_BOOTLOGO,   K_BOOL, 0, &ob_settings.boot_logo, 0, 0 },
    { S_I_BACKLIGHT,  K_BOOL, 0, &ob_settings.backlight, settings_apply_backlight, 0 },
    { S_I_BLLEVEL,    K_PCT, 100, &ob_settings.bl_level, settings_apply_backlight, 0 },
#ifdef CONFIG_FEATURE_DCR
    { S_I_DCR,        K_BOOL, 0, &ob_settings.dcr, 0, 0 },
#endif
    { S_I_POWERSAVE,  K_ENUM, SETTINGS_POWERSAVE_COUNT - 1, &ob_settings.powersave, 0, powersave_labels },
    { S_I_KEYREPEAT,  K_ENUM, SETTINGS_KEYREP_COUNT - 1, &ob_settings.key_repeat, settings_apply_keyrepeat, keyrep_labels },
    { S_I_DDCCI,      K_BOOL, 0, &ob_settings.ddcci, 0, 0 },
    { S_I_BACK,       K_BACK, 0, 0, 0, 0 },
};

static const __code struct item *const pages[] = {
    page_main, page_picture, page_display, page_system, page_audio, page_osd, page_info
};
#define NITEMS(p) ((u8)(sizeof(p) / sizeof(p[0])))
static const __code u8 page_len[] = {
    NITEMS(page_main), NITEMS(page_picture), NITEMS(page_display), NITEMS(page_system),
    NITEMS(page_audio), NITEMS(page_osd), NITEMS(page_info)
};
static const __code u8 page_title[] = {
    S_T_SETTINGS, S_T_PICTURE, S_T_DISPLAY, S_T_SYSTEM, S_T_AUDIO, S_T_OSD, S_T_INFO
};

static u8 active;
static u8 page;
static u8 sel;
static u8 main_sel;             /* where Back returns to on the main page */
static u8 locked;               /* MCCS CAh: host disabled the OSD */
static u8 popup_on;
static __xdata u8 popup_nosig;  /* the popup up is the "No signal" box */
static __xdata u8 popup_later;  /* POPUP_LATER_*: raised while the menu / a bar was up */
#define POPUP_LATER_MODE 1
#define POPUP_LATER_NOSIG 2
static u16 popup_until;
static u8 editing;              /* the highlighted numeric item is being adjusted (bar gauge) */
static __xdata u8 quick;        /* hotkey bar, no menu behind it: QUICK_VOLUME / QUICK_INPUT */
#define QUICK_VOLUME 1
#define QUICK_INPUT 2
static __xdata u8 confirm;      /* Reset all: armed by the first ENTER */
#ifdef CONFIG_DEBUG_CONSOLE
static __xdata u8 request;      /* MENU_REQ_* for main.c */
#endif
static __xdata u16 last_key;    /* tb_now() of the last key: menu / hotkey timeouts */
static __xdata u16 info_t;      /* Information page refresh */
static __xdata u8 top;          /* first item shown: pages longer than MAX_ITEMS scroll */
/* draw()'s working variables */
static __xdata u8 d_i, d_r, d_t, d_k, d_e, d_n;

/* the graphical menu's top-left (core/gui.h: its size, x the 1x/2x setting) */
static u16 menu_x(void)
{
    return (u16)(((u32)(PANEL_WIDTH - GUI_W * gui_scale()) * ob_settings.osd_h) / 100);
}

static u16 menu_y(void)
{
    return (u16)(((u32)(PANEL_HEIGHT - gui_h * gui_scale()) * ob_settings.osd_v) / 100);
}

/* popups: centered */
static u16 pop_x(void)
{
    return (PANEL_WIDTH - gui_pop_w * gui_scale()) / 2;
}

static u16 pop_y(void)
{
    return (PANEL_HEIGHT - gui_pop_h * gui_scale()) / 2;
}

static u16 pop_msg_y(void)
{
    return (PANEL_HEIGHT - gui_pop_msg_h * gui_scale()) / 2;
}

/* Editing a value: a slider of bar glyphs and the value - in the item's own row
 * (ROW_*: the menu stays up), or as a popup (POP_*: name, slider, value in one row,
 * low on the screen so the picture being adjusted stays visible).  The hotkey bars
 * are the popup. */
/* The name gets columns 1-16 (an I_ string: 16 characters, lang/en.txt) and two
 * spaces, so it never runs into the slider; in the row the slider takes the value's
 * place, so the name stays whole in any language. */
#define POP_BAR_COL 19
#define POP_BAR_CELLS 16
#define POP_VAL_COL 36
#define ROW_BAR_COL VALUE_COL
#define ROW_BAR_CELLS (gui_body_cols - VALUE_COL - 4)   /* then a space and a 3-column value */
#define ROW_VAL_COL (ROW_BAR_COL + ROW_BAR_CELLS + 1)
#define BAR_CELL_STEPS 12       /* bar glyph k = k of 12 columns filled (gen_assets.py) */
#define EDIT_Y ((PANEL_HEIGHT * 3) / 4)

u8 menu_active(void) __banked { return active; }
void menu_set_locked(u8 on) __banked { locked = on; }
u8 menu_popup_active(void) __banked { return popup_on; }

/* Values start in one column, left-aligned, without padding: the graphical menu sets
 * text proportionally, so a box or spaces padding a number to a width would not line
 * up.  (The digits of the theme fonts are all one width.) */
#ifndef OB_FW_VERSION
#define OB_FW_VERSION ((const __code char *)0xFF04)   /* tools/osdsim defines its own */
#endif

#define VALUE_COL 20
static __xdata char num_buf[6];

/* v as decimal, no padding, from (row, col); sign: '+' / '-' or 0 for none.
 * -> the column after it */
static u8 con_num(u8 row, u8 col, u16 v, char sign)
{
    d_n = 5;
    num_buf[5] = 0;
    do {
        num_buf[--d_n] = (char)('0' + v % 10);
        v /= 10;
    } while (v && d_n > 1);
    if (sign)
        num_buf[--d_n] = sign;
    con_puts(row, col, &num_buf[d_n]);
    return col + 5 - d_n;
}

/* string id at (row, col) -> the column after it */
static u8 con_str(u8 row, u8 col, u8 id)
{
    const char *t = L(id);

    con_puts(row, col, t);
    while (*t++)
        col++;
    return col;
}

/* the text of a list (K_ENUM) item's value v */
static const char *enum_text(const __code struct item *it, u8 v)
{
    if (v > it->arg)
        v = 0;
    if (!it->labels)                    /* Language: each in its own language */
        return lang_str_in(v, S_LANG_NAME);
    return L(it->labels[v]);
}

static void draw_value(u8 row, const __code struct item *it)
{
    s8 sv;
    u8 uv;

    if (it->kind == K_PAGE) {
        con_putc(row, VALUE_COL, '>');
        return;
    }
    if (it->kind == K_BACK)
        return;
    if (it->kind == K_ACTION) {
        if (it->arg == ACT_RESET && confirm)
            con_str(row, VALUE_COL, S_V_CONFIRM);
        return;
    }
    if (it->kind == K_RANGE) {
        sv = *(s8 *)it->val;
        if (sv < 0)
            con_num(row, VALUE_COL, (u16)-sv, '-');
        else if (sv > 0)
            con_num(row, VALUE_COL, (u16)sv, '+');
        else
            con_num(row, VALUE_COL, 0, 0);
        return;
    }
    uv = *(u8 *)it->val;
    if (it->kind == K_ENUM) {
        con_puts(row, VALUE_COL, enum_text(it, uv));
        return;
    }
    if (it->kind == K_BOOL) {
        d_n = S_V_OFF;
        if (uv)
            d_n = S_V_ON;
        con_str(row, VALUE_COL, d_n);
    } else
        con_num(row, VALUE_COL, uv, 0);
}

static void draw_slider(u8 row, u8 col, u8 cells, u8 vcol);

/* Information page: what is on screen and what is running. */
#define INFO_COL 16             /* values: clear of the longest label ("Pixel clock") */

/* pixel clock (0.1 MHz) and line length: HDMI measures the TMDS clock and the line;
 * a DP stream's come from its MSA (source_seen() has no totals for DP) */
static __xdata u32 i_pc;
static __xdata u16 i_ht, i_hf;

static void info_clocks(const struct hdmi_timing *t)
{
    if (rx_port_dp) {
        i_ht = dp_log.msa_htt;
        i_pc = (u32)dp_log.msa_htt * dp_log.msa_vtt;
        i_pc /= 1000;                   /* (u32 range: a 4K frame x 600 would not fit) */
        i_pc *= t->refresh_x10;
        i_pc /= 1000;
    } else {
        i_ht = t->htotal;
        i_pc = hdmi_stat.f_tmds;
    }
    i_hf = 0;                           /* line rate, 0.1 kHz */
    if (i_ht)
        i_hf = (u16)((i_pc * 1000) / i_ht);
}

static void draw_info(void)
{
    const struct hdmi_timing *t = source_seen();
    u8 up = source_picture(), c;

    con_str(1, 2, S_IL_INPUT);
    con_puts(1, INFO_COL, rx_port_dp ? "USB-C" : "HDMI");
    con_str(2, 2, S_IL_MODE);
    if (!up)
        con_str(2, INFO_COL, S_IV_NOSIGNAL);
    con_str(3, 2, S_IL_HFREQ);
    con_str(4, 2, S_IL_CLOCK);
    con_str(5, 2, S_IL_HDCP);
    if (up) {
        info_clocks(t);
        /* each value one run (single spaces), from INFO_COL - menu.c's layout is set
         * proportionally by the GUI, so no column padding inside it */
        c = con_num(2, INFO_COL, t->hactive, 0);
        con_putc(2, c, 'x');
        c = con_num(2, c + 1, t->vactive, 0);
        c = con_num(2, c + 1, (u16)(t->refresh_x10 / 10), 0);
        con_putc(2, c, '.');
        c = con_num(2, c + 1, (u16)(t->refresh_x10 % 10), 0);
        con_puts(2, c, "Hz");
        c = con_num(3, INFO_COL, i_hf / 10, 0);
        con_putc(3, c, '.');
        c = con_num(3, c + 1, i_hf % 10, 0);
        con_puts(3, c + 1, "kHz");
        c = con_num(4, INFO_COL, (u16)(i_pc / 10), 0);
        con_putc(4, c, '.');
        c = con_num(4, c + 1, (u16)(i_pc % 10), 0);
        con_puts(4, c + 1, "MHz");
        /* DP: B020 ENC_ENABLE (Km added, encrypting).  HDMI: 72AA HDCP_ENC_EN. */
        con_str(5, INFO_COL, (rx_port_dp ? (SR(D1_PB0_HDCP_DEBUG) & D1_PB0_HDCP_DEBUG_ENC_ENABLE)
                                         : (SR(TMDS_ABC1) & TMDS_ABC1_HDCP_ENC_EN)) ? S_V_ON : S_V_OFF);
    }
    con_str(6, 2, S_IL_FIRMWARE);
    {
        /* mkimage's info block: 'OBFW' + 12-char version, in every bank */
        const __code char *v = OB_FW_VERSION;
        u8 i;
        for (i = 0; i < 12 && v[i]; i++)
            con_putc(6, (u8)(INFO_COL + i), v[i]);
    }
    con_str(7, 2, S_IL_THEME);
    con_str(7, INFO_COL, theme_labels[gui_theme_index()]);
    con_str(8, 2, S_IL_USAGE);                  /* the panel's lit hours (MCCS C0h) */
    c = con_num(8, INFO_COL, ob_settings.usage_h, 0);
    con_putc(8, c + 1, 'h');
}

/* Lay the page out in the console buffer (title row 0, items rows 1-7), then let the
 * graphical menu draw it (core/gui.h).  On the main page the categories are the
 * GUI's icons, so the body previews the highlighted category instead of listing them. */
static void draw(void)
{
    const __code struct item *items = pages[page];
    u8 n = page_len[page];
    u8 show = page;

#ifdef CONFIG_FEATURE_PICTURE_MODES
    pm_shown = picmode_now();
#endif
    theme_shown = gui_theme_index();

    if (sel < top)                              /* keep the selection in view */
        top = sel;
    d_k = top + MAX_ITEMS;
    if (sel >= d_k)
        top = sel - MAX_ITEMS + 1;
    con_clear();
    if (page == PAGE_MAIN) {
        con_str(ROW_TITLE, 2, page_main[sel].name);
        if (page_main[sel].kind != K_PAGE)
            goto render;                    /* Exit: nothing behind it */
        show = page_main[sel].arg;
        items = pages[show];
        n = page_len[show];
    } else {
        con_str(ROW_TITLE, 2, page_title[page]);
    }
    if (show == PAGE_INFO) {
        draw_info();
        con_putc(INFO_BACK_ROW, 0, '>');
        con_str(INFO_BACK_ROW, 2, items[0].name);
        goto render;
    }
    d_t = top;
    if (page == PAGE_MAIN)
        d_t = 0;                                /* the preview shows the top of the page */
    d_e = 0;                                    /* the slider in the selected row */
    if (editing)
        if (!ob_settings.adjust_popup)
            d_e = 1;
    for (d_i = 0; d_i < MAX_ITEMS; d_i++) {
        d_k = d_t + d_i;
        if (d_k >= n)
            break;
        d_r = ROW_FIRST + d_i;
        if (page != PAGE_MAIN && d_k == sel)
            con_putc(d_r, 0, '>');
        con_str(d_r, 2, items[d_k].name);
        if (d_e && d_k == sel)
            draw_slider(d_r, ROW_BAR_COL, ROW_BAR_CELLS, ROW_VAL_COL);
        else
            draw_value(d_r, &items[d_k]);
    }
    if (d_t)
        con_putc(ROW_FIRST, 32, '^');           /* more above / below */
    d_k = d_t + MAX_ITEMS;
    if (d_k < n)
        con_putc(ROW_FIRST + MAX_ITEMS - 1, 32, 'v');
render:
    if (page == PAGE_MAIN)
        gui_render(sel, 0, 0);
    else if (page == PAGE_INFO)
        gui_render(main_sel, 1, INFO_BACK_ROW);
    else
        gui_render(main_sel, 1, (u8)(ROW_FIRST + sel - top));
}

static void menu_show(void)
{
    gui_show(menu_x(), menu_y());
    draw();
}

/* Numeric range of an editable item. */
static s16 item_min(const __code struct item *it)
{
    return (it->kind == K_RANGE) ? -PIPELINE_CB_RANGE : 0;
}

static s16 item_max(const __code struct item *it)
{
    return (it->kind == K_RANGE) ? PIPELINE_CB_RANGE : it->arg;
}

static s16 item_get(const __code struct item *it)
{
    return (it->kind == K_RANGE) ? *(s8 *)it->val : *(u8 *)it->val;
}

static void item_set(const __code struct item *it, s16 v)
{
    if (it->kind == K_RANGE)
        *(s8 *)it->val = (s8)v;
    else
        *(u8 *)it->val = (u8)v;
}

static u8 editable(const __code struct item *it)
{
    return it->kind == K_RANGE || it->kind == K_COUNT || it->kind == K_PCT;
}

static u8 is_pos(const __code struct item *it)
{
    if (it->val == &ob_settings.osd_h)
        return 1;
    if (it->val == &ob_settings.osd_v)
        return 1;
    return 0;
}

/* The selected item's slider and value into a console row. */
static __xdata s16 sl_lo, sl_hi, sl_v;
static __xdata u32 sl_m;
static __xdata u16 sl_px, sl_range;

static void draw_slider(u8 row, u8 col, u8 cells, u8 vcol)
{
    const __code struct item *it = &pages[page][sel];
    u8 c, k;

    sl_lo = item_min(it);
    sl_hi = item_max(it);
    sl_v = item_get(it);
    sl_px = 0;
    if (sl_hi > sl_lo) {
        sl_range = (u16)(sl_hi - sl_lo);
        sl_m = (u16)(sl_v - sl_lo);
        sl_m *= cells;
        sl_m *= BAR_CELL_STEPS;
        sl_m /= sl_range;
        sl_px = (u16)sl_m;
    }
    for (c = 0; c < cells; c++) {
        k = BAR_CELL_STEPS;
        if (sl_px < BAR_CELL_STEPS)
            k = (u8)sl_px;
        sl_px -= k;
        con_putc(row, col + c, (char)(ASSET_OSD_CHAR_BAR0 + k));
    }
    if (it->kind == K_RANGE) {
        k = ' ';
        if (sl_v < 0) {
            k = '-';
            sl_v = -sl_v;
        } else if (sl_v > 0) {
            k = '+';
        }
        con_putc(row, vcol, (char)k);
        con_dec(row, vcol + 1, (u16)sl_v, 2);
    } else {
        con_dec(row, vcol, (u16)sl_v, 3);
    }
}

/* the popup's row: name | slider | value (a list, the input picker: name | value) */
static void draw_edit(void)
{
    const __code struct item *it = &pages[page][sel];
    u8 v = (u8)item_get(it);

    con_clear();
    con_str(0, 1, it->name);
    if (it->kind == K_ENUM)
        con_puts(0, POP_BAR_COL, enum_text(it, v));
    else
        draw_slider(0, POP_BAR_COL, POP_BAR_CELLS, POP_VAL_COL);
}

static u8 adjust_in_popup(void)
{
    if (quick)
        return 1;
    return ob_settings.adjust_popup;
}

/* Where the popup goes: low on the screen (the picture being adjusted stays
 * visible), centered for the input picker, and ON the menu for its own position -
 * so the move is what you see. */
static void edit_show(void)
{
    const __code struct item *it = &pages[page][sel];

    if (quick == QUICK_INPUT)
        gui_popup(pop_x(), pop_y(), GUI_POP_ADJUST, 0);
    else if (!quick && is_pos(it))
        gui_popup(menu_x(), menu_y(), GUI_POP_ADJUST, 0);
    else
        gui_popup(pop_x(), EDIT_Y, GUI_POP_ADJUST, 0);
}

static void edit_begin(void)
{
    editing = 1;
    if (adjust_in_popup()) {
        draw_edit();
        edit_show();
    } else {
        draw();                         /* the slider in the item's row */
    }
}

static void edit_end(void)
{
    editing = 0;
    if (adjust_in_popup())
        menu_show();
    else
        draw();
}

/* UP/DOWN in edit mode: step, clamp (no wrap - a bar that jumps from full to
 * empty is a trap), apply live.  Apply on every step, repeats included: the
 * cheap items are a few register writes, and the two expensive ones (gamma, a
 * 6 KB stream; sharpness, 256 bytes of coefficients) have only five values. */
static void edit_step(u8 up)
{
    const __code struct item *it = &pages[page][sel];
    s16 v = item_get(it), step = 1;

    if (it->kind == K_PCT && it->arg >= 100)
        step = it->arg / 100;
    v = up ? v + step : v - step;
    if (it->kind == K_ENUM) {           /* a list wraps */
        if (v > item_max(it))
            v = 0;
        if (v < 0)
            v = item_max(it);
    }
    if (v > item_max(it))
        v = item_max(it);
    if (v < item_min(it))
        v = item_min(it);
    if (v == item_get(it))
        return;
    item_set(it, v);
    if (it->apply)
        it->apply();
    mccs_note_setting(it->val);
    if (adjust_in_popup()) {
        draw_edit();
        edit_show();                    /* redraws (and follows a new position) */
    } else if (is_pos(it)) {
        menu_show();                    /* the menu moves */
    } else {
        draw();
    }
}

void menu_open(void) __banked
{
    if (locked)
        return;
    popup_later = 0;
    popup_on = 0;
    editing = 0;
    quick = 0;
    confirm = 0;
    active = 1;
    page = PAGE_MAIN;
    sel = 0;
    top = 0;
    last_key = tb_now();
    menu_show();
}

/* A front-panel key with the menu closed: UP/DOWN = volume bar, POWER = input
 * picker.  The bar is the menu's own edit row on its own. */
void menu_hotkey(u8 key) __banked
{
    if (locked)
        return;
    popup_on = 0;
    active = 1;
    confirm = 0;
    last_key = tb_now();
    if (key == KEY_POWER) {
        quick = QUICK_INPUT;
        page = PAGE_DISPLAY;
        sel = 0;                        /* Input */
    } else {
        quick = QUICK_VOLUME;
        page = PAGE_AUDIO;
        sel = 0;                        /* Volume */
    }
    edit_begin();
    if (quick == QUICK_VOLUME)
        edit_step(key == KEY_UP);       /* the first press already adjusts */
}

#ifdef CONFIG_DEBUG_CONSOLE
u8 menu_take_request(void) __banked
{
    u8 r = request;

    request = 0;
    return r;
}
#endif

void menu_close(void) __banked
{
    active = 0;
    editing = 0;
    quick = 0;
    confirm = 0;
    gui_hide();
    con_disable();
    con_clear();
    con_colors(OSD_COL_WHITE, OSD_COL_TRANSPARENT);   /* back to the debug console's look */
    /* a popup that came up while the menu or a bar was showing: now */
    if (popup_later == POPUP_LATER_MODE && source_picture())
        menu_popup_mode(tb_now());
    else if (popup_later == POPUP_LATER_NOSIG && !source_picture())
        menu_popup_nosignal(tb_now());
    popup_later = 0;
}

static void item_enter(void)
{
    const __code struct item *it = &pages[page][sel];

    if (editable(it)) {
        edit_begin();
        return;
    }
    if (it->kind == K_ACTION) {
#ifdef CONFIG_DEBUG_CONSOLE
        if (it->arg == ACT_DEBUG) {
            menu_close();
            request = MENU_REQ_DEBUG;
            return;
        }
#endif
        if (!confirm) {                 /* ACT_RESET: once to arm, twice to do */
            confirm = 1;
            draw();
            return;
        }
        confirm = 0;
        settings_reset();
        menu_show();                    /* the position was reset too */
        return;
    }
    switch (it->kind) {
    case K_PAGE:
        main_sel = sel;
        page = it->arg;
        sel = 0;
        top = 0;
        break;
    case K_BACK:
        if (page == PAGE_MAIN) {
            menu_close();
            return;
        }
        page = PAGE_MAIN;
        sel = main_sel;
        top = 0;
        break;
    case K_BOOL:
        *(u8 *)it->val = !*(u8 *)it->val;
        break;
    case K_ENUM:                    /* a list: ENTER steps through it (wraps), no bar */
        *(u8 *)it->val = *(u8 *)it->val >= it->arg ? 0 : *(u8 *)it->val + 1;
        break;
    }
#ifdef CONFIG_FEATURE_PICTURE_MODES
    if (it->val == &pm_shown) {         /* a preset: Custom is not one to pick */
        if (pm_shown == PICMODE_CUSTOM)
            pm_shown = PICMODE_STANDARD;
        picmode_apply(pm_shown);
    }
#endif
    if (it->val == &theme_shown) {
        ob_settings.theme = gui_theme_id(theme_shown);
        gui_theme_apply();
    }
    if (it->apply)
        it->apply();
    mccs_note_setting(it->val);
    if (it->val == &ob_settings.osd_scale || it->val == &theme_shown)
        menu_show();                    /* the new size / look, now */
    else if (it->val == &ob_settings.language)
        menu_show();                    /* (re-centers the footer too) */
    else
        draw();
}

void menu_key(u8 key) __banked
{
    u8 n;

    if (!active)
        return;
    last_key = tb_now();
    if (quick) {
        /* volume: UP louder, DOWN quieter, POWER/ENTER close.
         * input picker: POWER/DOWN next, UP previous, ENTER close. */
        u8 up = 0;

        if (key == KEY_ENTER) {
            menu_close();
            return;
        }
        if (quick == QUICK_VOLUME) {
            if (key == KEY_POWER) {
                menu_close();
                return;
            }
            if (key == KEY_UP)
                up = 1;
        } else if (key != KEY_UP) {
            up = 1;
        }
        edit_step(up);
        return;
    }
    if (key == KEY_POWER) {             /* POWER leaves the menu from anywhere */
        menu_close();
        return;
    }
    if (editing) {
        if (key == KEY_UP || key == KEY_DOWN)
            edit_step(key == KEY_UP);
        else if (key == KEY_ENTER)
            edit_end();
        return;
    }
    confirm = 0;                        /* any other key disarms Reset all */
    n = page_len[page];
    if (key == KEY_UP)
        sel = sel ? (u8)(sel - 1) : (u8)(n - 1);
    else if (key == KEY_DOWN)
        sel = (u8)(sel + 1 >= n ? 0 : sel + 1);
    else if (key == KEY_ENTER) {
        item_enter();
        return;
    } else
        return;
    draw();
}

/* Active size straight out of the M1 capture window, so the popup reports what
 * the scaler is actually capturing rather than a compile-time constant.
 *
 * The high nibbles live in the *_ACT_STA_H registers, NOT in *_ACT_WID_H /
 * *_ACT_LEN_H as the names suggest: 0014[7:4] is IH_ACT_WID[11:8] and 0018[7:4]
 * is IV_ACT_LEN[11:8], with 0017 / 001B holding the low bytes.
 *
 * Refresh comes from hdmi_measure(), which counts the real input periods - the
 * same measurement the source state machine gates its mode set on, so the
 * popup and the state machine can never disagree about the input. */
void menu_popup_mode(u16 t0ovf) __banked
{
    struct hdmi_timing t;
    u16 w, h;
    u8 hz_ok;

    if (!ob_settings.res_popup)
        return;
    if (active) {                       /* e.g. the input picker switched: show it after */
        popup_later = POPUP_LATER_MODE;
        return;
    }
    if (rx_port_dp) {
        /* DP: what the mode set used (boundary search, D1 frame time) - the
         * page-30 re-measure and the HDMI mode table are wrong for DP (e.g.
         * 1920x1080 @ ~48 Hz for a phone's 1280x960) */
        t = *source_seen();
        hz_ok = 1;
    } else {
        hz_ok = hdmi_measure(&t);
    }
    if (hz_ok && t.active_ok) {
        /* What the input actually IS, from the boundary search (a live
         * measurement) - not what we programmed the capture window to. */
        w = t.hactive;
        h = t.vactive;
    } else {
        w = (u16)((u16)(SR(M1_IPH_ACT_STA_H) >> M1_IPH_ACT_STA_H_IH_ACT_WID_SHIFT) << 8)
          | SR(M1_IPH_ACT_WID_L);
        h = (u16)((u16)(SR(M1_IPV_ACT_STA_H) >> M1_IPV_ACT_STA_H_IV_ACT_LEN_SHIFT) << 8)
          | SR(M1_IPV_ACT_LEN_L);
    }

    /* icon | the input (accent) over the mode (value) */
    con_clear();
    con_puts(0, 4, rx_port_dp ? "USB-C" : "HDMI");
    con_dec(1, 4, w, 4);
    con_putc(1, 8, 'x');
    con_dec(1, 9, h, 4);
    if (hz_ok) {
        con_dec(1, 15, (u16)(t.refresh_x10 / 10), 3);
        con_putc(1, 18, '.');
        con_dec(1, 19, (u16)(t.refresh_x10 % 10), 1);
        con_puts(1, 20, "Hz");
    }
    gui_popup(pop_x(), pop_msg_y(), GUI_POP_MESSAGE, rx_port_dp ? GUI_ICON_USBC : GUI_ICON_HDMI);
    popup_on = 1;
    popup_nosig = 0;
    popup_until = (u16)(t0ovf + tb_ms(POPUP_MS));
}

/* A picture came up: take the "No signal" box down now (only that one - the
 * mode popup the mode set just raised stays). */
void menu_popup_nosignal_off(void) __banked
{
    if (popup_on && popup_nosig) {
        popup_on = 0;
        popup_nosig = 0;
        gui_hide();
        con_clear();
    }
}

/* No picture on any input (source.c, after NOSIGNAL_MS): the same box as the
 * mode popup, regardless of the Mode popup setting. */
void menu_popup_nosignal(u16 t0ovf) __banked
{
    if (active) {
        popup_later = POPUP_LATER_NOSIG;
        return;
    }
    con_clear();
    con_str(0, 4, S_MSG_NOSIGNAL);
    d_n = con_str(1, 4, S_MSG_INPUT) + 1;
    d_k = ob_settings.input;
    if (d_k >= SETTINGS_INPUT_COUNT)
        d_k = 0;
    con_str(1, d_n, input_labels[d_k]);
    gui_popup(pop_x(), pop_msg_y(), GUI_POP_MESSAGE, GUI_ICON_NOSIGNAL);
    popup_on = 1;
    popup_nosig = 1;
    popup_until = (u16)(t0ovf + tb_ms(POPUP_MS));
}

/* Forget a pending popup WITHOUT touching the console, for a caller that is
 * about to reconfigure the console itself.
 *
 * Otherwise, switching to the debug console while the mode popup is up leaves
 * popup_on set: the console inherits the popup's opaque colors, and then
 * menu_tick() expires the popup and runs con_disable() + con_clear() straight
 * over the debug screen. */
void menu_popup_cancel(void) __banked
{
    popup_on = 0;
}

void menu_tick(u16 t0ovf) __banked
{
    if (active) {
        u16 idle = (u16)(t0ovf - last_key);

        /* t0ovf is the main loop's, taken before the key handler stamped last_key
         * with a later tb_now(): a "negative" idle would wrap to ~65000 and close
         * the menu mid-scroll */
        if (idle >= 0x8000)
            idle = 0;

        if ((quick && idle >= tb_ms(QUICK_MS))
            || (!quick && ob_settings.menu_time
                && idle >= tb_ms(menutime_ms[ob_settings.menu_time < SETTINGS_MENUTIME_COUNT
                                              ? ob_settings.menu_time : SETTINGS_MENUTIME_DEFAULT]))) {
            menu_close();
            return;
        }
        if (page == PAGE_INFO && !editing && (u16)(t0ovf - info_t) >= tb_ms(1000)) {
            info_t = t0ovf;
            draw();                     /* live values */
        }
    }
    if (popup_on && (u16)(t0ovf - popup_until) < 0x8000) {
        popup_on = 0;
        gui_hide();
        con_clear();
    }
}
