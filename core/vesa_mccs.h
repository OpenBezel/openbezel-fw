/* VESA DDC/CI 1.1 and MCCS 2.2a: the values the standards define, as used by
 * core/mccs.c.  Nothing here is OpenBezel's own - the manufacturer-specific
 * VCP codes (E0h-FFh) are in mccs.h / mccs.c.
 *
 * Section numbers: VESA DDC/CI Standard Version 1.1, VESA MCCS Standard
 * Version 2.2a. */
#ifndef OB_VESA_MCCS_H
#define OB_VESA_MCCS_H

/* ---- DDC/CI 1.1 ------------------------------------------------------------- */

/* I2C addresses (DDC/CI 2.2, 2.3): the display answers at 6Eh/6Fh; the host's source
 * byte is 51h, and the display's replies are checksummed from the host's
 * virtual address 50h. */
#define DDCCI_ADDR_DISPLAY 0x6E
#define DDCCI_ADDR_HOST 0x51
#define DDCCI_ADDR_VHOST 0x50

#define DDCCI_LEN_MSG 0x80              /* length byte: bit 7 marks a DDC/CI message, 6:0 the length */

/* commands / reply op codes (DDC/CI 4.3-4.7) */
#define DDCCI_GET_VCP 0x01
#define DDCCI_GET_VCP_REPLY 0x02
#define DDCCI_SET_VCP 0x03
#define DDCCI_GET_TIMING 0x07
#define DDCCI_SAVE_SETTINGS 0x0C
#define DDCCI_TIMING_REPLY 0x4E
#define DDCCI_CAPS_REPLY 0xE3
#define DDCCI_CAPS_REQUEST 0xF3

/* VCP Feature Reply (4.3): result code, VCP type */
#define DDCCI_RC_OK 0x00
#define DDCCI_RC_UNSUPPORTED 0x01
#define DDCCI_VCP_SET_PARAMETER 0x00
#define DDCCI_VCP_MOMENTARY 0x01

/* Timing Message (4.7): its length byte is 06h, without bit 7; status byte */
#define DDCCI_TIMING_LEN 0x06
#define DDCCI_TIMING_OUT_OF_RANGE 0x80
#define DDCCI_TIMING_UNSTABLE 0x40
#define DDCCI_TIMING_HSYNC_POS 0x02
#define DDCCI_TIMING_VSYNC_POS 0x01

/* ---- MCCS 2.2a VCP codes ------------------------------------------------------ */

#define VCP_NEW_CONTROL 0x02
#define VCP_RESTORE_DEFAULTS 0x04       /* all factory presets */
#define VCP_RESTORE_LUM_CONTRAST 0x05
#define VCP_RESTORE_COLOR 0x08
#define VCP_LUMINANCE 0x10
#define VCP_CONTRAST 0x12
#define VCP_BACKLIGHT_LEGACY 0x13       /* MCCS 2.0 backlight; 6Bh in 2.2a */
#define VCP_COLOR_PRESET 0x14
#define VCP_GAIN_RED 0x16
#define VCP_GAIN_GREEN 0x18
#define VCP_GAIN_BLUE 0x1A
#define VCP_ACTIVE_CONTROL 0x52
#define VCP_INPUT_SOURCE 0x60
#define VCP_AUDIO_VOLUME 0x62
#define VCP_BACKLIGHT 0x6B              /* backlight level: white */
#define VCP_GAMMA 0x72
#define VCP_SCALING 0x86
#define VCP_SHARPNESS 0x87
#define VCP_SATURATION 0x8A
#define VCP_AUDIO_MUTE 0x8D             /* audio mute / screen blank */
#define VCP_H_FREQ 0xAC                 /* Hz, 24-bit (ML:SH:SL) */
#define VCP_V_FREQ 0xAE                 /* 0.01 Hz */
#define VCP_SUBPIXEL_LAYOUT 0xB2
#define VCP_DISPLAY_TECH 0xB6
#define VCP_USAGE_TIME 0xC0             /* hours */
#define VCP_CONTROLLER_ID 0xC8
#define VCP_FIRMWARE_LEVEL 0xC9         /* SH.SL */
#define VCP_OSD_CONTROL 0xCA            /* OSD / button control */
#define VCP_OSD_LANGUAGE 0xCC
#define VCP_POWER_MODE 0xD6
#define VCP_DISPLAY_APP 0xDC            /* display application (picture mode) */
#define VCP_VERSION 0xDF

/* ---- values (SL byte unless noted) --------------------------------------------- */

/* 02h new control value */
#define VCP_NEW_CONTROL_NONE 0x01
#define VCP_NEW_CONTROL_CHANGED 0x02

/* 14h color preset */
#define VCP_COLOR_SRGB 0x01
#define VCP_COLOR_NATIVE 0x02
#define VCP_COLOR_4000K 0x03
#define VCP_COLOR_5000K 0x04
#define VCP_COLOR_6500K 0x05
#define VCP_COLOR_7500K 0x06
#define VCP_COLOR_8200K 0x07
#define VCP_COLOR_9300K 0x08
#define VCP_COLOR_10000K 0x09
#define VCP_COLOR_11500K 0x0A
#define VCP_COLOR_USER1 0x0B

/* 60h input source */
#define VCP_INPUT_DP1 0x0F
#define VCP_INPUT_DP2 0x10
#define VCP_INPUT_HDMI1 0x11
#define VCP_INPUT_HDMI2 0x12            /* the last defined */

/* 72h gamma: SL = the adjustment, SH = the value */
#define VCP_GAMMA_WHITE_ABS 0x00        /* SH = gamma x 100 - 100 */
#define VCP_GAMMA_RED_ABS 0x01
#define VCP_GAMMA_GREEN_ABS 0x02
#define VCP_GAMMA_BLUE_ABS 0x03
#define VCP_GAMMA_WHITE_REL 0x04
#define VCP_GAMMA_DISABLE 0x05
/* SH of a relative adjustment: the default, default - 0.1 x SH (01h-0Ah),
 * default + 0.1 x (SH - 10h) (11h-1Ah), or disable */
#define VCP_GAMMA_REL_DEFAULT 0x00
#define VCP_GAMMA_REL_DOWN_FIRST 0x01   /* - 0.1 */
#define VCP_GAMMA_REL_DOWN_LAST 0x0A    /* - 1.0 */
#define VCP_GAMMA_REL_UP_FIRST 0x11     /* + 0.1 */
#define VCP_GAMMA_REL_UP_LAST 0x1A      /* + 1.0 */
#define VCP_GAMMA_REL_DISABLE 0x20

/* 86h display scaling */
#define VCP_SCALE_NONE 0x01             /* 1:1 */
#define VCP_SCALE_MAX 0x02              /* maximum image, no aspect distortion */
#define VCP_SCALE_MAX_V 0x03            /* maximum vertical, no distortion */
#define VCP_SCALE_MAX_H 0x04            /* maximum horizontal, no distortion */
#define VCP_SCALE_MAX_V_DISTORT 0x05
#define VCP_SCALE_MAX_H_DISTORT 0x06
#define VCP_SCALE_FULL 0x07             /* linear on the horizontal axis */
#define VCP_SCALE_ZOOM 0x08             /* linear on both axes */
#define VCP_SCALE_SQUEEZE 0x09          /* all of the image, bars allowed */
#define VCP_SCALE_VARIABLE 0x0A         /* non-linear horizontal; the last defined */

/* 8Dh audio mute (SL) */
#define VCP_AUDIO_MUTED 0x01
#define VCP_AUDIO_UNMUTED 0x02

/* B2h sub-pixel layout, B6h display technology */
#define VCP_SUBPIXEL_RGB_V 0x01         /* R/G/B vertical stripe */
#define VCP_SUBPIXEL_MOSAIC 0x08
#define VCP_TECH_LCD_ACTIVE 0x03        /* LCD (active matrix) */
#define VCP_TECH_DYNAMIC_MEM 0x08

/* C8h controller ID: SL = manufacturer */
#define VCP_CONTROLLER_REALTEK 0x09

/* CAh OSD / button control */
#define VCP_OSD_DISABLED 0x01           /* sink OSD off, host-OSD button events on */
#define VCP_OSD_ENABLED 0x02
#define VCP_OSD_DISABLED_NO_EVENTS 0x03 /* sink OSD off, button events off too */

/* CCh OSD language: the last defined code (25h Vietnamese); each build
 * language's own code is its lang/ file's @mccs */
#define VCP_LANG_LAST 0x25

/* D6h power mode (DPM / DPMS) */
#define VCP_POWER_ON 0x01
#define VCP_POWER_STANDBY 0x02
#define VCP_POWER_SUSPEND 0x03
#define VCP_POWER_OFF 0x04
#define VCP_POWER_OFF_BUTTON 0x05       /* as the power button */

/* DCh display application */
#define VCP_APP_STANDARD 0x00
#define VCP_APP_PRODUCTIVITY 0x01
#define VCP_APP_MIXED 0x02
#define VCP_APP_MOVIE 0x03
#define VCP_APP_USER 0x04
#define VCP_APP_GAMES 0x05
#define VCP_APP_DEMO 0x0A               /* the last defined */

/* DFh VCP version: SH.SL */
#define VCP_VERSION_2_2 0x0202

#endif
