/* User settings.
 *
 * Persistent: loaded from the flash settings log at boot and saved back a few
 * seconds after a change (core/settings_store.c).
 * The struct IS the stored payload, byte for byte.  New fields go at the END
 * (a shorter record from an older build then loads, keeping the new fields'
 * defaults; the record has room to 57 bytes and loading clamps every field).
 * Reordering or retyping one needs STORE_VERSION bumped in settings_store.c.
 */
#ifndef OB_SETTINGS_H
#define OB_SETTINGS_H

#include "ob_types.h"

/* Powersave presets, index into source.c's delay table: 0 off, 1 = 5 s,
 * 2 = 30 s, 3 = 2 min.  Default is 30 s - long enough not to fire on a mode
 * change, short enough to be the point of the feature. */
#define SETTINGS_POWERSAVE_COUNT 4
#define SETTINGS_POWERSAVE_DEFAULT 2

/* Key auto-repeat presets (delay before the first repeat, then the rate). */
#define SETTINGS_KEYREP_OFF 0
#define SETTINGS_KEYREP_SLOW 1  /* 600 ms, then every 200 ms */
#define SETTINGS_KEYREP_NORM 2  /* 400 ms, then every 100 ms */
#define SETTINGS_KEYREP_FAST 3  /* 250 ms, then every 50 ms */
#define SETTINGS_KEYREP_COUNT 4

#define SETTINGS_INPUT_AUTO 0       /* keep while live; follow a loss; a new phone wins */
#define SETTINGS_INPUT_PREF_HDMI 1  /* HDMI whenever it has video, else USB-C */
#define SETTINGS_INPUT_PREF_USBC 2  /* USB-C whenever it has video, else HDMI */
#define SETTINGS_INPUT_HDMI 3       /* HDMI only */
#define SETTINGS_INPUT_USBC 4       /* USB-C only */
#define SETTINGS_INPUT_COUNT 5

#define SETTINGS_LOWBLUE_COUNT 4

/* Menu auto-close after no key: Off, 10 s, 30 s, 60 s (menu.c) */
#define SETTINGS_MENUTIME_COUNT 4
#define SETTINGS_MENUTIME_DEFAULT 2

struct ob_settings {
    s8 brightness;      /* -50..+50, 0 = neutral: CTS/BRI offset */
    s8 contrast;        /* -50..+50, 0 = neutral: CTS/BRI gain */
    u8 saturation;      /* 0 = grayscale, 100 = normal, 200 = max (color matrix) */
    u8 ctemp;           /* color temperature preset: shares the CTS/BRI gains with contrast */
    u8 sharpness;       /* 0..PIPELINE_SHARPNESS_MAX, scale-up filter preset */
    u8 gamma;           /* 0 = bypass, 1..gamma_lut_count = gamma presets */
    u8 idither;         /* input dither block (I-domain, 0088/0089): 0 off, 1 on */
    u8 ddither;         /* display dither block (D-domain, 0069/006A): 0 off, 1 on */
    u8 boot_logo;       /* show the splash screen at boot */
    u8 res_popup;       /* show the mode popup when the input timing changes */
    u8 backlight;       /* 0 off, 1 on - the USER's master setting */
    u8 bl_level;        /* backlight PWM level, 0..100 % (MCCS 6Bh) */
    u8 scaling;         /* HDMI_SCALE_*: Full (stretch) or Fit (keep aspect, pillarbox) */
    u8 powersave;       /* backlight off after no source: 0 off, 1 = 5 s, 2 = 30 s, 3 = 2 min */
    u8 ddcci;           /* answer DDC/CI (MCCS) at all: 0 off, 1 on */
    u8 wd_blank;        /* arm the display watchdogs for instant hardware blanking (see source.c).
                         * DEFAULT OFF: on this board they trip during normal operation. */
    u8 key_repeat;      /* UP/DOWN auto-repeat preset: SETTINGS_KEYREP_* */
    u8 volume;          /* audio volume 0..100 (MCCS 62h) */
    u8 mute;            /* audio mute 0/1 (MCCS 8Dh) */
    u8 input;           /* SETTINGS_INPUT_* (MCCS 60h sets HDMI / USB-C) */
    u8 menu_time;       /* SETTINGS_MENUTIME_*: close the menu after no key */
    u8 osd_h;           /* menu position, 0..100 % of the free width (50 = centered) */
    u8 osd_v;           /* ... and height */
    u8 osd_scale;       /* menu and popups: 0 = 1x, 1 = 2x (core/gui.c) */
    u8 adjust_popup;    /* editing a value: 0 = slider in the item's row, 1 = separate popup */
    u8 language;        /* menu language: index into the build's LANGS (core/lang.h) */
    u8 low_blue;        /* blue-light cut, SETTINGS_LOWBLUE_*: Off / Low / Medium / High */
    u16 usage_h;        /* hours the panel has been lit (MCCS C0h); kept by a factory reset */
    u8 usage_20;        /* ... plus 20-minute units, 0..2 (booked every 20 min) */
    u8 user_r, user_g, user_b;  /* User color temperature gains 0..100 (MCCS 16h/18h/1Ah) */
    u8 dcr;             /* dynamic contrast: the backlight follows the picture (core/dcr.h) */
    u8 theme;           /* menu theme: its stable id (theme.h; core/gui.h gui_theme_apply) */
    /* (the payload may grow to 57 bytes: STORE_REC 64, settings_store.c) */
    /* --- appended fields: always at the END, so an older, shorter record
     * still loads (settings_store.c keeps the defaults for the tail) --- */
};
extern struct ob_settings ob_settings;

void settings_defaults(void);

/* Force every field into its legal range - for values that came from flash,
 * which a CRC can vouch for but a newer/older firmware may not agree with. */
void settings_sanitize(void);

/* Defaults, then the newest valid record from flash on top (sanitized).
 * The master backlight switch is NOT restored: "off" surviving a power cycle
 * would be a dark monitor with no visible way back.  Returns STORE_OK /
 * STORE_NONE (settings_store.h). */
u8 settings_load(void);

/* Call periodically: saves the settings to flash once they have been left
 * alone for ~3 s.  Unchanged settings never touch the flash.
 *
 * Deliberately NOT held off while the menu is open: the menu can be left up
 * indefinitely, and a change made meanwhile must still survive a power cycle.
 * The 3 s settle already keeps rapid adjustment to one write. */
void settings_autosave(u16 t0ovf);

/* Each apply_* pushes one setting to the hardware, so a menu edit costs only
 * the work that setting actually implies. */
void settings_apply_sharpness(void);
void settings_apply_idither(void);
void settings_apply_ddither(void);
void settings_apply_gamma(void);
/* brightness, contrast and color temperature are ONE apply: contrast and
 * color temperature write the same three CTS/BRI gain bytes, so splitting
 * them would let whichever ran last erase the other. */
void settings_apply_levels(void);
/* a User gain was edited: color temperature becomes User, then apply_levels */
void settings_apply_user_gain(void);
void settings_apply_saturation(void);
void settings_apply_backlight(void);
/* dynamic contrast's current cut, % off bl_level (core/dcr.c; 0 = none) */
extern __xdata u8 dcr_cut;
void settings_apply_keyrepeat(void);
void settings_apply_audio(void);
/* Scaling needs a full mode set; this requests one from the source state
 * machine, which runs it on its next tick if a picture is up. */
void settings_apply_scaling(void);

/* Powersave gate (source.c).  The user's ob_settings.backlight stays master;
 * this only forces the panel dark on top of it.  Keeping them as two values
 * rather than one is deliberate - sharing a variable means a powersave cycle
 * silently turns a user "backlight off" back on. */
void settings_backlight_gate(u8 on);
/* Same again for the host's MCCS D6h power mode: a third, independent gate. */
void settings_backlight_host(u8 on);

/* Every apply_* in turn (MCCS 04h restore factory defaults). */
void settings_apply_all(void);
void settings_reset(void);              /* factory defaults, applied (the autosave stores them);
                                         * the usage time survives */

#endif /* OB_SETTINGS_H */
