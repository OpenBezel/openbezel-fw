/* DDC/CI + MCCS - see mccs.h. */
#include "mccs.h"
#include "osd.h"
#include "ddcci_hw.h"
#include "settings.h"
#include "menu.h"
#include "lang_ids.h"
#include "picmode.h"
#include "source.h"
#include "hdmi_rx.h"
#include "display.h"
#include "audio.h"
#include "dp_rx.h"
#include "hdcp.h"
#include "timebase.h"
#include "pipeline.h"
#include "assets.h"
#include "rtd2556_regs.h"
#include "flash_rw.h"
#include "settings_store.h"
#include "delay.h"
#include "edp_tx.h"
#include "vesa_mccs.h"

/* Debug VCP codes (CONFIG_DEBUG_VCP), not in the capabilities string: see
 * vcp_get / vcp_set for what each does */
#define VCP_DBG_OSD_ADDR 0xEC           /* OSD SRAM address */
#define VCP_DBG_OSD_B01 0xED            /* OSD SRAM word bytes 0/1 */
#define VCP_DBG_OSD_B2 0xEE             /* OSD SRAM word byte 2: write */
#define VCP_DBG_OSD_LUT 0xEF            /* OSD palette entry */
#define VCP_DBG_PEEK_ADDR 0xF0          /* scaler address for F1h / FCh */
#define VCP_DBG_PEEK 0xF1               /* read 2 bytes there */
#define VCP_DBG_DF_PEEK 0xF2            /* display-format port index / read */
#define VCP_DBG_FLASH_TEST 0xF3         /* scratch-sector flash test */
#define VCP_DBG_FLASH_READ 0xF4
#define VCP_DBG_STORE_SCAN 0xF5         /* settings log re-scan */
#define VCP_DBG_STORE_STATUS 0xF6
#define VCP_DBG_STACK_REBOOT 0xF7       /* get: stack high-water; set: reboot */
#define VCP_DBG_STATUS 0xF8             /* STATUS0/1, then clear */
#define VCP_DBG_DCLK 0xF9               /* display clock DPLL M / DDS offset */
#define VCP_DBG_TRIM 0xFA               /* frame-sync lead trim */
#define VCP_DBG_PKT_PEEK 0xFB           /* HDMI packet SRAM */
#define VCP_DBG_POKE 0xFC               /* write the byte at F0h's address */
#define VCP_DBG_HDMI_IDX 0xFD           /* HDMI indirect register */
#define VCP_DBG_AUDIO 0xFE              /* staged audio bring-up / state */
#define VCP_DBG_DP 0xFF                 /* staged DP RX bring-up / state */
#define REBOOT_SCALER 0x5A5A            /* F7h set: scaler soft reset + restart */
#define REBOOT_RESTART 0x0001           /* F7h set: restart only */

#define CAPS_FRAG 8             /* data bytes per fragment: 5 + 8 + 1 = 14 <= the 16-byte FIFO */

/* The capabilities string.  Only what is actually implemented below - the
 * Kconfig features (config.h) add their codes. */
#ifdef CONFIG_FEATURE_USER_GAINS
#define CAPS_14 "14(04 05 08 0B) 16 18 1A "
#else
#define CAPS_14 "14(04 05 08) "
#endif
#ifdef CONFIG_FEATURE_PICTURE_MODES
#define CAPS_DC "DC(00 01 03 04 05) "
#else
#define CAPS_DC ""
#endif
#ifdef CONFIG_FEATURE_LOW_BLUE
#define CAPS_E6 "E6 "
#else
#define CAPS_E6 ""
#endif
#ifdef CONFIG_FEATURE_DCR
#define CAPS_EA " EA"
#else
#define CAPS_EA ""
#endif
static const __code char caps[] =
    "(prot(monitor)type(lcd)model(OpenBezel)cmds(01 02 03 07 0C E3 F3)"
    "vcp(02 04 05 08 10 12 " CAPS_14 "52 60(0F 11) 62 6B 72(05 78 FA 50 64 78 8C) 86(02 05) 87 8A 8D(01 02) AC AE B2 B6 C0 C8 C9 CA CC(" LANG_MCCS_CAPS ") D6(01 04 05) " CAPS_DC "DF "
    "E0 E1 E2 E3 E4 E5 " CAPS_E6 "E7 E8 E9" CAPS_EA ")mccs_ver(2.2))";

/* VCP DCh (display application) per picture mode: Custom = 04 user defined,
 * Standard 00, Movie 03, Game 05, Text = 01 productivity */
#ifdef CONFIG_FEATURE_PICTURE_MODES
static const __code u8 picmode_mccs[PICMODE_COUNT] = {
    VCP_APP_USER, VCP_APP_STANDARD, VCP_APP_MOVIE, VCP_APP_GAMES, VCP_APP_PRODUCTIVITY
};
#endif

/* VCP CCh: the build's languages (LANGS, core/lang.h) as MCCS language codes */
static const __code u8 lang_mccs[LANG_COUNT] = LANG_MCCS_CODES;

u8 mccs_ui_request = 0xFF;
u8 mccs_ui_now;
#if defined(CONFIG_DEBUG_VCP) || defined(CONFIG_DEBUG_CONSOLE)
u8 mccs_sets, mccs_last_code;
u16 mccs_last_val;
#endif
#ifdef CONFIG_DEBUG_VCP
__xdata u8 mccs_log[MCCS_LOG][3];
#endif

static u8 new_ctrl;             /* VCP 02h: a non-host change is pending */
static u8 changed_vcp;          /* VCP 52h: single-slot FIFO of the last changed code */
static u8 osd_locked;           /* VCP CAh */
static u8 host_off;             /* VCP D6h */
static u8 video_blank;          /* VCP E8h */
#ifdef CONFIG_DEBUG_VCP
static u16 peek_addr;           /* debug F0h/F1h */
static u8 peek_df;              /* debug F2h: display-format port index */
static u8 peek_ps;              /* debug FBh: HDMI packet-storage SRAM index */
static u8 peek_hidx;            /* debug FDh: HDMI indirect (HDMI_AP/DP) index of the last access */
/* Debug flash test on the scratch sector FTEST (never the settings log):
 * F3h set = op << 8 | data - 01 erase, 02 program one byte `data` at the
 * pointer (then ptr++), 03 program 16 bytes data, data+1, ... (ptr += 16),
 * 04 ptr = data * 16.  F3h get = last result (SH) and ptr (SL, low byte).
 * F4h set = read offset, F4h get = the two bytes there, read back from flash. */
#define FTEST 0x1FF000UL
static u16 ftest_ptr, ftest_rd;
static u8 ftest_res = 0xFF;
static __xdata u8 ftest_buf[16];
/* Settings store: 0Ch saves; F5h get re-scans the
 * log WITHOUT applying it (SH = STORE_* result, SL = next free slot, MH:ML =
 * sequence); F6h get = last save of any kind (SH), active sector (SL), last
 * 0Ch save (ML). */
static u8 save_res = 0xFF;
u8 stack_peak(void);                    /* core/main.c */
static __xdata u16 osd_dbg_addr, osd_dbg_b01;
static __xdata u8 osd_dbg_b2, osd_dbg_b2v;
#endif

#ifdef CONFIG_DEBUG_VCP
/* EEh / EFh (see vcp_set) */
static void osd_dbg(u8 op)
{
    u8 hi = HI(osd_dbg_b01), lo = LO(osd_dbg_b01);

    if (!op) {                          /* EFh: LUT[index] = staged R, G, B */
        osd_lut_set(osd_dbg_b2v, hi, lo, osd_dbg_b2);
        return;
    }
    osd_sram_word(osd_dbg_addr & 0x7FFF, hi, lo, osd_dbg_b2,
                  (osd_dbg_addr & 0x8000) ? OSD_SRAM_WINDOW : OSD_SRAM_FONT);
    osd_dbg_addr++;
    if (op & 1)
        osd_commit();
}
static __xdata struct ob_settings scan_tmp;
#endif

static __xdata u8 msg[DDCCI_RX_MAX];
static __xdata u8 tx[DDCCI_TX_MAX];

void mccs_init(void) __banked
{
    ddcci_hw_init();
}

/* ---- replies ---------------------------------------------------------------- */

/* tx[2..] holds the payload; fill in the header and checksum and queue it.
 * `len` is the payload length; `raw_len` is the length byte as sent (the
 * timing report is the one message whose length byte lacks bit 7). */
static void send(u8 len, u8 raw_len)
{
    u8 i, chk = DDCCI_ADDR_VHOST;

    tx[0] = DDCCI_ADDR_DISPLAY;
    tx[1] = raw_len;
    for (i = 0; i < len + 2; i++)
        chk ^= tx[i];
    tx[len + 2] = chk;
    ddcci_hw_reply(tx, (u8)(len + 3));
}

/* ---- VCP get / set ----------------------------------------------------------- */

struct vcp_val {
    u8 type;                    /* 0 = set parameter, 1 = momentary */
    u16 max, cur;
};

static u8 ctemp_to_mccs(u8 c)
{
    if (c == PIPELINE_CTEMP_USER)
        return VCP_COLOR_USER1;
    return c == PIPELINE_CTEMP_WARM ? VCP_COLOR_5000K : (c == PIPELINE_CTEMP_COOL ? VCP_COLOR_9300K : VCP_COLOR_6500K);
}

/* ---- gamma (72h) ------------------------------------------------------------
 *
 * The four gamma LUTs are corrections on top of the panel's own response,
 * not absolute curves: fitted to out = in^e (tools/gen_assets.py tables)
 * they come out e = 0.83 / 0.92 / 1.02 / 1.12 (R/G/B mean), in the
 * exact ratios of 1.8 : 2.0 : 2.2 : 2.4.  So they are 1.8, 2.0, 2.2 and 2.4 on
 * a panel whose native gamma is ~2.2 (2.2 / 1.02 = 2.16).  Per-channel fit
 * error is <= 5 % - red bends most, probably a white-point trim - hence the
 * tolerance of 05 in the capabilities string.  Our default, preset 0, bypasses
 * the LUT, which leaves that native ~2.2.
 *
 * MCCS values are gamma x 100 - 100: 1.8 = 50h, 2.0 = 64h, 2.2 = 78h, 2.4 = 8Ch.
 */
#define GAMMA_NATIVE 120

static const __code u8 gamma_mccs[ASSET_GAMMA_COUNT] = { 80, 100, 120, 140 };

static u8 gamma_nearest(s16 want)
{
    u8 i, best = 0;
    s16 d, bd = 0x7FFF;

    for (i = 0; i < ASSET_GAMMA_COUNT; i++) {
        d = want - (s16)gamma_mccs[i];
        if (d < 0)
            d = -d;
        if (d < bd) {
            bd = d;
            best = i;
        }
    }
    return (u8)(best + 1);
}

static void gamma_set(u8 sh, u8 sl)
{
    u8 g;

    if (sl == VCP_GAMMA_DISABLE || (sl == VCP_GAMMA_WHITE_REL && sh == VCP_GAMMA_REL_DISABLE))
        g = 0;                                                         /* bypass the LUT */
    else if (sl == VCP_GAMMA_WHITE_ABS)
        g = gamma_nearest(sh);
    else if (sl == VCP_GAMMA_WHITE_REL && sh == VCP_GAMMA_REL_DEFAULT)
        g = 0;
    else if (sl == VCP_GAMMA_WHITE_REL && sh >= VCP_GAMMA_REL_DOWN_FIRST && sh <= VCP_GAMMA_REL_DOWN_LAST)
        g = gamma_nearest(GAMMA_NATIVE - 10 * (s16)sh);                /* 0.1 x SH below */
    else if (sl == VCP_GAMMA_WHITE_REL && sh >= VCP_GAMMA_REL_UP_FIRST && sh <= VCP_GAMMA_REL_UP_LAST)
        g = gamma_nearest(GAMMA_NATIVE + 10 * (s16)(sh - (VCP_GAMMA_REL_UP_FIRST - 1)));   /* 0.1 x (SH - 10h) above */
    else
        return;                                                        /* R/G/B-only: no per-channel LUT choice */
    ob_settings.gamma = g;
    settings_apply_gamma();
}

/* 1 = supported, vcp_out filled */
static __xdata struct vcp_val vcp_out;
#define v (&vcp_out)

static u8 vcp_get(u8 code)
{
    const struct hdmi_timing *m;
    u32 hz;

    v->type = DDCCI_VCP_SET_PARAMETER;
    v->max = 100;
    switch (code) {
    case VCP_NEW_CONTROL: v->max = 0xFF; v->cur = new_ctrl ? VCP_NEW_CONTROL_CHANGED : VCP_NEW_CONTROL_NONE; return 1;
    case VCP_RESTORE_DEFAULTS: case VCP_RESTORE_LUM_CONTRAST: case VCP_RESTORE_COLOR: v->type = DDCCI_VCP_MOMENTARY; v->max = 1; v->cur = 0; return 1;
    case VCP_LUMINANCE: v->cur = (u16)(ob_settings.brightness + PIPELINE_CB_RANGE); return 1;
    case VCP_CONTRAST: v->cur = (u16)(ob_settings.contrast + PIPELINE_CB_RANGE); return 1;
    case VCP_COLOR_PRESET: v->max = VCP_COLOR_USER1; v->cur = ctemp_to_mccs(ob_settings.ctemp); return 1;
#ifdef CONFIG_FEATURE_USER_GAINS
    case VCP_GAIN_RED: v->max = 100; v->cur = ob_settings.user_r; return 1;
    case VCP_GAIN_GREEN: v->max = 100; v->cur = ob_settings.user_g; return 1;
    case VCP_GAIN_BLUE: v->max = 100; v->cur = ob_settings.user_b; return 1;
#endif
    case VCP_ACTIVE_CONTROL: v->max = 0xFF; v->cur = changed_vcp; changed_vcp = 0; new_ctrl = 0; return 1;
    case VCP_INPUT_SOURCE:                                              /* DP 1 (USB-C) / HDMI 1 */
        v->max = VCP_INPUT_HDMI2;
        v->cur = source_input() == SOURCE_IN_DP ? VCP_INPUT_DP1 : VCP_INPUT_HDMI1;
        return 1;
    case VCP_BACKLIGHT_LEGACY:
    case VCP_BACKLIGHT: v->cur = ob_settings.bl_level; return 1;
    case VCP_SCALING:
        v->max = VCP_SCALE_VARIABLE;
        v->cur = ob_settings.scaling == HDMI_SCALE_FIT ? VCP_SCALE_MAX : VCP_SCALE_MAX_V_DISTORT;
        return 1;
    case VCP_AUDIO_VOLUME: v->max = 100; v->cur = ob_settings.volume; return 1;
    case VCP_AUDIO_MUTE: v->max = VCP_AUDIO_UNMUTED; v->cur = ob_settings.mute ? VCP_AUDIO_MUTED : VCP_AUDIO_UNMUTED; return 1;
    case VCP_SHARPNESS: v->max = PIPELINE_SHARPNESS_MAX; v->cur = ob_settings.sharpness; return 1;
    case VCP_SATURATION: v->max = PIPELINE_SAT_MAX; v->cur = ob_settings.saturation; return 1;
    case VCP_H_FREQ:                                                         /* H freq, Hz, 24-bit */
    case VCP_V_FREQ:                                                         /* V freq, 0.01 Hz */
        v->max = 0;
        v->cur = 0;
        if (source_state() != SOURCE_ACTIVE)
            return 1;                                                  /* all zero = not synced */
        m = source_seen();
        if (code == VCP_V_FREQ) {
            v->cur = (u16)(m->refresh_x10 * 10U);
            return 1;
        }
        if (rx_port_dp) {               /* DP: the stream's MSA (seen has no totals) */
            hz = dp_log.msa_vtt;
            hz *= m->refresh_x10;
            hz /= 10;
        } else {
            hz = ((u32)hdmi_stat.f_tmds * 100000UL) / m->htotal;
        }
        v->max = BYTE2(hz);                                            /* MH = 0, ML = bits 23:16 */
        v->cur = (u16)hz;
        return 1;
    case VCP_SUBPIXEL_LAYOUT: v->max = VCP_SUBPIXEL_MOSAIC; v->cur = VCP_SUBPIXEL_RGB_V; return 1;
    case VCP_DISPLAY_TECH: v->max = VCP_TECH_DYNAMIC_MEM; v->cur = VCP_TECH_LCD_ACTIVE; return 1;
    case VCP_USAGE_TIME: v->max = 0; v->cur = ob_settings.usage_h; return 1;         /* usage time, hours */
    case VCP_CONTROLLER_ID: v->max = 0x0025; v->cur = 0x5600 | VCP_CONTROLLER_REALTEK; return 1;   /* chip 002556 */
    case VCP_FIRMWARE_LEVEL: v->max = 0; v->cur = 0x0001; return 1;                  /* firmware level 0.1 */
    case VCP_OSD_CONTROL: v->max = VCP_OSD_DISABLED_NO_EVENTS; v->cur = osd_locked ? VCP_OSD_DISABLED : VCP_OSD_ENABLED; return 1;
#ifdef CONFIG_FEATURE_PICTURE_MODES
    case VCP_DISPLAY_APP: v->max = VCP_APP_DEMO; v->cur = picmode_mccs[picmode_now()]; return 1;
#endif
    case VCP_OSD_LANGUAGE: v->max = VCP_LANG_LAST; v->cur = lang_mccs[ob_settings.language]; return 1;   /* sanitized < LANG_COUNT */
    case VCP_POWER_MODE: v->max = VCP_POWER_OFF_BUTTON; v->cur = host_off ? VCP_POWER_OFF : VCP_POWER_ON; return 1;
    case VCP_VERSION: v->max = 0; v->cur = VCP_VERSION_2_2; return 1;
    case VCP_OB_BOOT_LOGO: v->max = 1; v->cur = ob_settings.boot_logo; return 1;
    case VCP_OB_RES_POPUP: v->max = 1; v->cur = ob_settings.res_popup; return 1;
    case VCP_OB_POWERSAVE: v->max = SETTINGS_POWERSAVE_COUNT - 1; v->cur = ob_settings.powersave; return 1;
    case VCP_OB_IDITHER: v->max = 1; v->cur = ob_settings.idither; return 1;
    case VCP_OB_DDITHER: v->max = 1; v->cur = ob_settings.ddither; return 1;
    case VCP_GAMMA:                                                    /* SH = gamma */
        v->max = 0;
        v->cur = ob_settings.gamma
               ? (u16)(((u16)gamma_mccs[ob_settings.gamma - 1] << 8) | VCP_GAMMA_WHITE_ABS)
               : (u16)((GAMMA_NATIVE << 8) | VCP_GAMMA_DISABLE);       /* bypassed: native */
        return 1;
    case VCP_OB_GAMMA_PRESET: v->max = ASSET_GAMMA_COUNT; v->cur = ob_settings.gamma; return 1;
#ifdef CONFIG_FEATURE_LOW_BLUE
    case VCP_OB_LOW_BLUE: v->max = SETTINGS_LOWBLUE_COUNT - 1; v->cur = ob_settings.low_blue; return 1;
#endif
#ifdef CONFIG_FEATURE_DCR
    case VCP_OB_DCR: v->max = 1; v->cur = ob_settings.dcr; return 1;
#endif
    case VCP_OB_UI_MODE: v->max = MCCS_UI_MAX; v->cur = mccs_ui_now; return 1;     /* UI: 0 splash, 1 input, 2 debug */
    case VCP_OB_VIDEO_BLANK: v->max = 1; v->cur = video_blank; return 1;
    case VCP_OB_KEY_REPEAT: v->max = SETTINGS_KEYREP_COUNT - 1; v->cur = ob_settings.key_repeat; return 1;
#ifdef CONFIG_DEBUG_VCP
    /* Debug register peek, NOT in the capabilities string: set F0h to a
     * scaler address, read F1h for that byte (SH) and the next (SL).  F2h the
     * same through the display-format port (002A/002B) at the index set with
     * F2h.  Reading a data port or a clear-on-read status HAS the side effect
     * - this is for registers, not FIFOs. */
    case VCP_DBG_PEEK_ADDR: v->max = 0xFFFF; v->cur = peek_addr; return 1;
    case VCP_DBG_PEEK:
        v->max = 0xFFFF;
        v->cur = (u16)(((u16)SR(peek_addr) << 8) | SR((u16)(peek_addr + 1)));
        return 1;
    case VCP_DBG_FLASH_TEST: v->max = ftest_ptr; v->cur = (u16)(((u16)ftest_res << 8) | LO(ftest_ptr)); return 1;
    case VCP_DBG_STORE_SCAN: {
        u8 r = store_load(&scan_tmp);
        v->max = store_seq;
        v->cur = (u16)(((u16)r << 8) | store_slot);
        return 1;
    }
    case VCP_DBG_TRIM: v->max = 0; v->cur = (u16)(s16)source_trim(); return 1;
    case VCP_DBG_PKT_PEEK:                                                         /* HDMI packet SRAM (72CD/72CE) at peek_ps, peek_ps+1; ML = GPVS */
        v->max = SR(HDMI_GPVS);
        SR(HDMI_PSAP) = peek_ps;
        v->cur = (u16)SR(HDMI_PSDP) << 8;
        SR(HDMI_PSAP) = (u8)(peek_ps + 1);
        v->cur |= SR(HDMI_PSDP);
        return 1;
    case VCP_DBG_AUDIO: v->max = ((u16)dp_audio_on << 8) | (SR(D1_AUD_FSM_CTRL_0));
               v->cur = rx_port_dp ? 0x100 | dp_audio_present() : hdmi_audio_rate(); return 1;
    case VCP_DBG_DP: v->max = ((u16)(P3_2 ? 1 : 0) << 8) | (IE0 ? 1 : 0);   /* INT0 pin level (0 = asserted) | IE0 */
               v->cur = ((u16)dp_log.aux_rx << 8) | dp_log.lt_writes; return 1;      /* 0 no audio, 3 = 48 kHz */
    case VCP_DBG_HDMI_IDX: v->max = peek_hidx; SR(HDMI_AP) = peek_hidx; v->cur = SR(HDMI_DP); return 1;
    case VCP_DBG_DCLK: v->max = display_dpll_m_get(); v->cur = display_dclk_get(); return 1;
    case VCP_DBG_STATUS:                                                         /* STATUS0/1 since last read, then clear (W1C) */
        v->max = 0;
        v->cur = (u16)(((u16)SR(STATUS0) << 8) | SR(STATUS1));
        SR(STATUS0) = 0xFF;
        SR(STATUS1) = 0xFF;
        return 1;
    case VCP_DBG_STACK_REBOOT: v->max = 0; v->cur = stack_peak(); return 1;   /* stack high-water (FF = overflowed) */
    case VCP_DBG_STORE_STATUS: v->max = save_res; v->cur = (u16)(((u16)store_last << 8) | store_sector); return 1;
    case VCP_DBG_FLASH_READ:
        v->max = ftest_rd;
        flash_read(FTEST + ftest_rd, ftest_buf, 2);
        v->cur = (u16)(((u16)ftest_buf[0] << 8) | ftest_buf[1]);
        return 1;
    case VCP_DBG_DF_PEEK:
        v->max = peek_df;
        SR(DISPLAY_FORMAT_ADDR_PORT) = peek_df;
        v->cur = (u16)SR(DISPLAY_FORMAT_DATA_PORT) << 8;
        SR(DISPLAY_FORMAT_ADDR_PORT) = (u8)(peek_df + 1);
        v->cur |= SR(DISPLAY_FORMAT_DATA_PORT);
        return 1;
#endif
    }
    return 0;
}
#undef v

static u8 clamp(u16 v, u16 max)
{
    return (u8)(v > max ? max : v);
}

static void vcp_set(u8 code, u16 val)
{
#if LANG_COUNT > 1 || defined(CONFIG_FEATURE_PICTURE_MODES)
    u8 i;
#endif

#if defined(CONFIG_DEBUG_VCP) || defined(CONFIG_DEBUG_CONSOLE)
    if (code == VCP_DBG_PEEK_ADDR || code == VCP_DBG_DF_PEEK || code == VCP_DBG_FLASH_READ || code == VCP_DBG_PKT_PEEK
        || code == VCP_DBG_POKE || code == VCP_DBG_HDMI_IDX || code == VCP_DBG_AUDIO
        || (u8)(code - VCP_DBG_OSD_ADDR) < 4)    /* EC-EF, no bit temporary */
        goto peek_only;                     /* the debug peek does not log itself */
#ifdef CONFIG_DEBUG_VCP
    mccs_log[mccs_sets & (MCCS_LOG - 1)][0] = code;
    mccs_log[mccs_sets & (MCCS_LOG - 1)][1] = HI(val);
    mccs_log[mccs_sets & (MCCS_LOG - 1)][2] = LO(val);
#endif
    mccs_sets++;
    mccs_last_code = code;
    mccs_last_val = val;
peek_only:
#endif
    switch (code) {
    case VCP_NEW_CONTROL:
        if (val == VCP_NEW_CONTROL_NONE) { new_ctrl = 0; changed_vcp = 0; }
        return;
    case VCP_RESTORE_DEFAULTS:                                                         /* everything (the usage time stays) */
        settings_reset();
        return;
    case VCP_RESTORE_LUM_CONTRAST:                                                         /* luminance + contrast */
        ob_settings.brightness = 0;
        ob_settings.contrast = 0;
        settings_apply_levels();
        return;
    case VCP_RESTORE_COLOR:                                                         /* color */
        ob_settings.ctemp = PIPELINE_CTEMP_NORMAL;
        ob_settings.saturation = PIPELINE_SAT_NORMAL;
        settings_apply_levels();
        settings_apply_saturation();
        return;
    case VCP_LUMINANCE:
        ob_settings.brightness = (s8)((s16)clamp(val, 100) - PIPELINE_CB_RANGE);
        settings_apply_levels();
        return;
    case VCP_CONTRAST:
        ob_settings.contrast = (s8)((s16)clamp(val, 100) - PIPELINE_CB_RANGE);
        settings_apply_levels();
        return;
    case VCP_COLOR_PRESET:
        if (val == VCP_COLOR_4000K || val == VCP_COLOR_5000K)
            ob_settings.ctemp = PIPELINE_CTEMP_WARM;
        else if (val == VCP_COLOR_SRGB || val == VCP_COLOR_NATIVE || val == VCP_COLOR_6500K)
            ob_settings.ctemp = PIPELINE_CTEMP_NORMAL;
        else if (val >= VCP_COLOR_7500K && val <= VCP_COLOR_11500K)
            ob_settings.ctemp = PIPELINE_CTEMP_COOL;
#ifdef CONFIG_FEATURE_USER_GAINS
        else if (val == VCP_COLOR_USER1)
            ob_settings.ctemp = PIPELINE_CTEMP_USER;
#endif
        else
            return;
        settings_apply_levels();
        return;
#ifdef CONFIG_FEATURE_USER_GAINS
    /* video gain (drive): the User color temperature's gains - writing one selects
     * User, as AOC's firmware does */
    case VCP_GAIN_RED: ob_settings.user_r = clamp(val, 100); settings_apply_user_gain(); return;
    case VCP_GAIN_GREEN: ob_settings.user_g = clamp(val, 100); settings_apply_user_gain(); return;
    case VCP_GAIN_BLUE: ob_settings.user_b = clamp(val, 100); settings_apply_user_gain(); return;
#endif
    case VCP_BACKLIGHT_LEGACY:
    case VCP_BACKLIGHT:
        ob_settings.bl_level = clamp(val, 100);
        settings_apply_backlight();
        return;
    case VCP_GAMMA:
        gamma_set(HI(val), LO(val));
        return;
    case VCP_SCALING:
        /* max image without distortion = Fit; max vertical (no distortion)
         * is the same thing for every input narrower than the panel, squeeze
         * (all content, bars allowed) too.  Filling an axis or both with
         * distortion = Full.  No scaling and max horizontal (= crop
         * vertically) cannot be built on a line buffer - see hdmi_rx.h - and
         * are ignored, as are full / variable (TV modes). */
        if (val == VCP_SCALE_MAX || val == VCP_SCALE_MAX_V || val == VCP_SCALE_SQUEEZE)
            ob_settings.scaling = HDMI_SCALE_FIT;
        else if (val == VCP_SCALE_MAX_V_DISTORT || val == VCP_SCALE_MAX_H_DISTORT || val == VCP_SCALE_ZOOM)
            ob_settings.scaling = HDMI_SCALE_STRETCH;
        else
            return;
        settings_apply_scaling();
        return;
    case VCP_INPUT_SOURCE:
        if (val == VCP_INPUT_HDMI1) ob_settings.input = SETTINGS_INPUT_HDMI;
        else if (val == VCP_INPUT_DP1) ob_settings.input = SETTINGS_INPUT_USBC;
        return;
    case VCP_AUDIO_VOLUME:
        ob_settings.volume = clamp(val, 100);
        settings_apply_audio();
        return;
    case VCP_AUDIO_MUTE:
        if (val == VCP_AUDIO_MUTED) ob_settings.mute = 1;
        else if (val == VCP_AUDIO_UNMUTED) ob_settings.mute = 0;
        else return;
        settings_apply_audio();
        return;
    case VCP_SHARPNESS:
        ob_settings.sharpness = clamp(val, PIPELINE_SHARPNESS_MAX);
        settings_apply_sharpness();
        return;
    case VCP_SATURATION:
        ob_settings.saturation = clamp(val, PIPELINE_SAT_MAX);
        settings_apply_saturation();
        return;
    case VCP_OSD_CONTROL:
        if (val == VCP_OSD_DISABLED || val == VCP_OSD_DISABLED_NO_EVENTS) { osd_locked = 1; if (menu_active()) menu_close(); }
        else if (val == VCP_OSD_ENABLED) osd_locked = 0;
        menu_set_locked(osd_locked);
        return;
#ifdef CONFIG_FEATURE_PICTURE_MODES
    case VCP_DISPLAY_APP:                                                         /* display application: a preset */
        for (i = 1; i < PICMODE_COUNT; i++)
            if (picmode_mccs[i] == (u8)val)
                picmode_apply(i);
        return;
#endif
    case VCP_OSD_LANGUAGE:                                                         /* OSD language: MCCS code */
#if LANG_COUNT > 1                      /* one language: nothing to choose (and no constant loop) */
        for (i = 0; i < LANG_COUNT; i++)
            if (lang_mccs[i] == (u8)val)
                ob_settings.language = i;
#endif
        return;
    case VCP_POWER_MODE:                                               /* standby / suspend / off: all off */
        if (val == VCP_POWER_ON) host_off = 0;
        else if (val >= VCP_POWER_STANDBY && val <= VCP_POWER_OFF_BUTTON) host_off = 1;
        else return;
        settings_backlight_host(!host_off);
        return;
    case VCP_OB_BOOT_LOGO: ob_settings.boot_logo = val ? 1 : 0; return;
    case VCP_OB_RES_POPUP: ob_settings.res_popup = val ? 1 : 0; return;
    case VCP_OB_POWERSAVE: ob_settings.powersave = clamp(val, SETTINGS_POWERSAVE_COUNT - 1); return;
    case VCP_OB_IDITHER: ob_settings.idither = val ? 1 : 0; settings_apply_idither(); return;
    case VCP_OB_DDITHER: ob_settings.ddither = val ? 1 : 0; settings_apply_ddither(); return;
    case VCP_OB_GAMMA_PRESET: ob_settings.gamma = clamp(val, ASSET_GAMMA_COUNT); settings_apply_gamma(); return;
#ifdef CONFIG_FEATURE_LOW_BLUE
    case VCP_OB_LOW_BLUE: ob_settings.low_blue = clamp(val, SETTINGS_LOWBLUE_COUNT - 1); settings_apply_levels(); return;
#endif
#ifdef CONFIG_FEATURE_DCR
    case VCP_OB_DCR: ob_settings.dcr = val ? 1 : 0; return;                  /* dcr_tick() follows */
#endif
    case VCP_OB_UI_MODE: if (val <= MCCS_UI_MAX) mccs_ui_request = (u8)val; return;
    case VCP_OB_KEY_REPEAT:
        ob_settings.key_repeat = clamp(val, SETTINGS_KEYREP_COUNT - 1);
        settings_apply_keyrepeat();
        return;
#ifdef CONFIG_DEBUG_VCP
    case VCP_DBG_PEEK_ADDR: peek_addr = val; return;
    case VCP_DBG_FLASH_READ: ftest_rd = val; return;
    case VCP_DBG_PKT_PEEK: peek_ps = (u8)val; return;
    case VCP_DBG_DP:
        /* Debug, staged DP RX (USB-C) bring-up: 1 = stage 1
         * (AUX + DPCD + port power-on + HPD + training ISR), 2 = HPD low, 3 = HPD
         * high, 4 = re-arm INT0 and zero the stray count (dp_rx.h). */
        if (val == 1) dp_rx_start();
        else if (val == 2) dp_hpd(0);
        else if (val == 3) dp_hpd(1);
        else if (val == 4) dp_irq_rearm();
        else if (val == 6) dp_log.stage3 = dp_stream_start();
        else if (val == 7) dp_log.stage3 = 0x10 | source_dp_show(tb_now());
        else if (val == 8) source_dp_arm();
        else if (val == 0x20) hdcp_init();             /* HDCP keys (stage: then FFh = 1 re-plugs DP) */
        else if (val == 9) { EX0 = 0; IE0 = 0; }       /* INT0 off: read the request via FFh get (IE0) */
        else if (val == 10) IE0 = 0;
        else if (HI(val) == 1) peek_hidx = LO(val);    /* 0x1nn: select HDMI index nn for FDh get, no write */
        return;
    /* Debug, OSD SRAM / LUT pokes: ECh = SRAM address
     * (bit 15: window SRAM, else font SRAM); EDh = bytes 0/1 of a word; EEh =
     * byte 2 - writes the word, address + 1, bit 8 also commits the double
     * buffer; EFh = blue << 8 | LUT index, with red/green from EDh. */
    case VCP_DBG_OSD_ADDR: osd_dbg_addr = val; return;
    case VCP_DBG_OSD_B01: osd_dbg_b01 = val; return;
    case VCP_DBG_OSD_B2: osd_dbg_b2 = LO(val); osd_dbg(0x80 | HI(val)); return;
    case VCP_DBG_OSD_LUT: osd_dbg_b2v = LO(val); osd_dbg_b2 = HI(val); osd_dbg(0); return;   /* blue<<8 | index */
    case VCP_DBG_AUDIO:
        /* Debug, staged audio bring-up: 1 = DAC/outputs/mux up,
         * HDMI receiver audio start, then the volume/mute settings. */
        if (val == 1) {
            audio_init();
            hdmi_audio_start();
            settings_apply_audio();
        } else if (val == 2) {
            dp_audio_start();           /* D1 (USB-C DP) audio */
            if (rx_port_dp) {
                audio_route(1);
                audio_hold();
            }
        }
        return;
    /* Debug POKES from the host:
     * FCh writes the low byte to the address set with F0h; FDh writes
     * HDMI indirect register (val >> 8) = (u8)val, and FDh get reads back
     * the last index used.  Not in the capabilities string. */
    case VCP_DBG_POKE: SR(peek_addr) = LO(val); return;
    case VCP_DBG_HDMI_IDX:
        peek_hidx = HI(val);
        SR(HDMI_AP) = peek_hidx;
        SR(HDMI_DP) = LO(val);
        return;
    case VCP_DBG_TRIM:
        /* Debug: force the frame-sync lead trim (input lines, signed) and redo
         * the mode set; stops the closed-loop trim for this timing. */
        source_trim_force((s8)val);
        return;
    case VCP_DBG_DCLK:
        /* Debug: display clock DDS offset, live (same M), then re-measure
         * the eDP MVID so the panel follows - for sweeping the clean window
         * of a mode against STATUS0 (F8h).  The next mode set overrides it. */
        if (display_dclk_set(val))
            edp_mvid_update();
        return;
    case VCP_DBG_STACK_REBOOT:
        /* Reboot: 5A5A = scaler soft reset, the same one the boot path does
         * (HOST_CTRL.0 high 10 ms), then a restart through the reset vector;
         * 0001 = the restart alone.  Neither returns.  NOT
         * MCU_CLOCK_CONTROL.SOF_RST: that is a LEVEL - set, it holds the 8051
         * in reset until an ISP exit. */
        if (val == REBOOT_SCALER) {
            SR_SET(HOST_CTRL, HOST_CTRL_SOFT_RESET);
            delay_ms(10);
            SR_CLR(HOST_CTRL, HOST_CTRL_SOFT_RESET);
            delay_ms(10);
        }
        if (val == REBOOT_SCALER || val == REBOOT_RESTART) {
            __asm
                ljmp    0
            __endasm;
        }
        return;
    case VCP_DBG_FLASH_TEST: {
        u8 op = HI(val), d = LO(val), i;
        if (op == 1) {
            ftest_res = flash_erase_sector(FTEST);
        } else if (op == 2) {
            ftest_buf[0] = d;
            ftest_res = flash_program(FTEST + ftest_ptr, ftest_buf, 1);
            ftest_ptr++;
        } else if (op == 3) {
            for (i = 0; i < 16; i++)
                ftest_buf[i] = (u8)(d + i);
            ftest_res = flash_program(FTEST + ftest_ptr, ftest_buf, 16);
            ftest_ptr += 16;
        } else if (op == 4) {
            ftest_ptr = (u16)d << 4;
        }
        return;
    }
    case VCP_DBG_DF_PEEK: peek_df = (u8)val; return;
#endif
    case VCP_OB_VIDEO_BLANK:
        video_blank = val ? 1 : 0;
        display_force_bg(video_blank);
        return;
    }
}

/* ---- menu -> host notification ---------------------------------------------- */

static const __code struct { const void *p; u8 vcp; } vcp_of[] = {
    { &ob_settings.brightness, VCP_LUMINANCE },
    { &ob_settings.contrast, VCP_CONTRAST },
    { &ob_settings.ctemp, VCP_COLOR_PRESET },
    { &ob_settings.bl_level, VCP_BACKLIGHT },
    { &ob_settings.sharpness, VCP_SHARPNESS },
    { &ob_settings.saturation, VCP_SATURATION },
    { &ob_settings.boot_logo, VCP_OB_BOOT_LOGO },
    { &ob_settings.res_popup, VCP_OB_RES_POPUP },
    { &ob_settings.powersave, VCP_OB_POWERSAVE },
    { &ob_settings.idither, VCP_OB_IDITHER },
    { &ob_settings.ddither, VCP_OB_DDITHER },
    { &ob_settings.gamma, VCP_GAMMA },
    { &ob_settings.scaling, VCP_SCALING },
    { &ob_settings.volume, VCP_AUDIO_VOLUME },
    { &ob_settings.mute, VCP_AUDIO_MUTE },
    { &ob_settings.key_repeat, VCP_OB_KEY_REPEAT },
    { &ob_settings.language, VCP_OSD_LANGUAGE },
#ifdef CONFIG_FEATURE_USER_GAINS
    { &ob_settings.user_r, VCP_GAIN_RED },
    { &ob_settings.user_g, VCP_GAIN_GREEN },
    { &ob_settings.user_b, VCP_GAIN_BLUE },
#endif
#ifdef CONFIG_FEATURE_LOW_BLUE
    { &ob_settings.low_blue, VCP_OB_LOW_BLUE },
#endif
#ifdef CONFIG_FEATURE_DCR
    { &ob_settings.dcr, VCP_OB_DCR },
#endif
};

void mccs_note_setting(const void *val) __banked
{
    u8 i;

    for (i = 0; i < sizeof(vcp_of) / sizeof(vcp_of[0]); i++) {
        if (vcp_of[i].p == val) {
            changed_vcp = vcp_of[i].vcp;
            new_ctrl = 1;
            return;
        }
    }
}

/* ---- dispatch ---------------------------------------------------------------- */

static void do_get(u8 code)
{
    struct vcp_val v;
    u8 ok = vcp_get(code);

    v = vcp_out;
    tx[2] = DDCCI_GET_VCP_REPLY;
    tx[3] = ok ? DDCCI_RC_OK : DDCCI_RC_UNSUPPORTED;
    tx[4] = code;
    tx[5] = ok ? v.type : 0;
    tx[6] = ok ? HI(v.max) : 0;
    tx[7] = ok ? LO(v.max) : 0;
    tx[8] = ok ? HI(v.cur) : 0;
    tx[9] = ok ? LO(v.cur) : 0;
    send(8, DDCCI_LEN_MSG | 8);
}

static void do_caps(u16 ofs)
{
    u8 i, n = 0;

    tx[2] = DDCCI_CAPS_REPLY;
    tx[3] = HI(ofs);
    tx[4] = LO(ofs);
    while (n < CAPS_FRAG && ofs + n < sizeof(caps) - 1) {              /* no terminating NUL */
        tx[5 + n] = (u8)caps[ofs + n];
        n++;
    }
    i = (u8)(3 + n);
    send(i, (u8)(DDCCI_LEN_MSG | i));                                  /* n = 0 marks end of string */
}

static void do_timing(void)
{
    struct vcp_val h, vv;

    vcp_get(VCP_H_FREQ);
    h = vcp_out;
    vcp_get(VCP_V_FREQ);
    vv = vcp_out;
    tx[2] = DDCCI_TIMING_REPLY;
    tx[3] = (source_state() == SOURCE_ACTIVE) ? 0 : DDCCI_TIMING_OUT_OF_RANGE;   /* or none */
    /* H in 10 Hz units, V in 0.01 Hz (VESA DDC/CI 4.7) */
    {
        u32 hz = ((u32)h.max << 16) | h.cur;
        u16 h10 = (u16)(hz / 10);
        tx[4] = HI(h10);
        tx[5] = LO(h10);
    }
    tx[6] = HI(vv.cur);
    tx[7] = LO(vv.cur);
    send(6, DDCCI_TIMING_LEN);                                         /* the length byte has no bit 7 here */
}

/* One framed message: [0x80|len] [payload x len] [checksum]. */
static void handle(const __xdata u8 *m, u8 len, u8 src)
{
    u8 i, chk = DDCCI_ADDR_DISPLAY ^ src;

    for (i = 0; i <= len; i++)
        chk ^= m[i];
    if (chk != m[len + 1])
        return;                                                        /* DDC/CI: no NAK, just drop */

    switch (m[1]) {
    case DDCCI_GET_VCP:
        if (len >= 2)
            do_get(m[2]);
        return;
    case DDCCI_SET_VCP:
        if (len >= 4)
            vcp_set(m[2], (u16)(((u16)m[3] << 8) | m[4]));
        return;
    case DDCCI_CAPS_REQUEST:
        if (len >= 3)
            do_caps((u16)(((u16)m[2] << 8) | m[3]));
        return;
    case DDCCI_GET_TIMING:
        do_timing();
        return;
    case DDCCI_SAVE_SETTINGS:
#ifdef CONFIG_DEBUG_VCP
        save_res = store_save(&ob_settings);
#else
        store_save(&ob_settings);
#endif
        return;
    }
}

void mccs_poll(void) __banked
{
    u8 src, n, len, at = 0;

    n = ddcci_hw_poll(&src, msg);           /* always: keeps the engine drained */
    if (!n || src != DDCCI_ADDR_HOST || !ob_settings.ddcci)
        return;                             /* off: never answer, host sees no DDC/CI */
    /* Usually one message; a slow poll can leave several back-to-back (see
     * ddcci_hw.h).  Their source bytes were not kept, but every host message
     * comes from 0x51, which the checksum confirms. */
    while ((u8)(n - at) >= 2) {
        if (!(msg[at] & DDCCI_LEN_MSG))
            return;                                                    /* lost framing: drop the rest */
        len = msg[at] & (u8)~DDCCI_LEN_MSG;
        if (len == 0 || (u8)(n - at) < (u8)(len + 2))
            return;
        handle(&msg[at], len, src);
        at = (u8)(at + len + 2);
    }
}
