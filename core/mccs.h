/* DDC/CI + MCCS (VESA DDC/CI 1.1, MCCS 2.2a): host control of the settings.
 *
 * Everything maps onto existing settings and apply_* functions, so a DDC/CI
 * change behaves exactly like the same change made from the menu, autosave
 * included.  Save Current Settings (0Ch) writes the settings log at once.
 *
 * VCP codes:
 *   10 Luminance      video BRIGHTNESS (CTS/BRI offset), NOT the backlight -
 *                     MCCS 2.2a has 6Bh for that
 *   6B Backlight      PWM5 level; 13h (MCCS legacy backlight) is an alias
 *   E0-E8             manufacturer-specific: see mccs.c
 */
#ifndef OB_MCCS_H
#define OB_MCCS_H

#include "ob_types.h"

/* OpenBezel's manufacturer-specific VCP codes */
#define VCP_OB_BOOT_LOGO 0xE0           /* boot_logo 0/1 */
#define VCP_OB_RES_POPUP 0xE1           /* res_popup 0/1 */
#define VCP_OB_POWERSAVE 0xE2           /* powersave index */
#define VCP_OB_IDITHER 0xE3             /* input dither 0/1 */
#define VCP_OB_DDITHER 0xE4             /* display dither 0/1 */
#define VCP_OB_GAMMA_PRESET 0xE5        /* gamma preset, 0 = bypass */
#define VCP_OB_LOW_BLUE 0xE6            /* low blue light index */
#define VCP_OB_UI_MODE 0xE7             /* 0 splash, 1 input, 2 debug console */
#define VCP_OB_VIDEO_BLANK 0xE8         /* force the background color */
#define VCP_OB_KEY_REPEAT 0xE9          /* key repeat index */
#define VCP_OB_DCR 0xEA                 /* dynamic contrast 0/1 */

void mccs_init(void) __banked;
void mccs_poll(void) __banked;           /* call from the main loop; cheap when idle */

/* The menu changed a setting (pointer into ob_settings): raise VCP 02h and
 * queue the matching VCP code in 52h, as MCCS requires for non-host changes. */
void mccs_note_setting(const void *val) __banked;

/* UI mode the host asked for through VCP E7h (0 splash, 1 input, 2 debug), or
 * 0xFF for none; the main loop owns the UI and applies it. */
extern u8 mccs_ui_request;
#ifdef CONFIG_DEBUG_CONSOLE
#define MCCS_UI_MAX 2
#else
#define MCCS_UI_MAX 1
#endif
extern u8 mccs_ui_now;          /* current UI mode, kept up to date by main.c (E7h read-back) */

/* Last Set VCP the host sent, and how many (wrapping) - on the debug console,
 * because a desktop (KDE PowerDevil) writes 10h on every hotplug and that is
 * otherwise invisible. */
extern u8 mccs_sets, mccs_last_code;
extern u16 mccs_last_val;

/* The last MCCS_LOG Set VCPs, oldest overwritten: [i] = code, value high,
 * value low, at index (mccs_sets - 1) % MCCS_LOG.  Read over DDC/CI with the
 * F0h/F1h peek - for watching what a desktop actually sends. */
#define MCCS_LOG 8
extern __xdata u8 mccs_log[MCCS_LOG][3];

#endif
