/* osdsim host API - called from Python (osdsim/sim.py) through ctypes. */
#ifndef OSDSIM_API_H
#define OSDSIM_API_H

#include "ob_types.h"

int osdsim_load_assets(const char *path);   /* the asset partition image (assets.bin) */
void osdsim_reset(void);                    /* defaults, OSD cleared, menu closed */
void osdsim_key(u8 key);                    /* a front-panel key, as main.c routes it */
void osdsim_open_menu(void);
void osdsim_close(void);                    /* menu / popup down */
void osdsim_popup_mode(void);
void osdsim_popup_nosignal(void);
void osdsim_tick(u16 ms);                   /* advance time: menu timeouts, popup expiry */
/* the input as the popups / Information page see it */
void osdsim_set_signal(u8 dp, u8 up, u16 w, u16 h, u16 hz_x10, u16 htotal, u16 vtotal, u16 f_tmds_x10);
int osdsim_setting_set(const char *name, int value);    /* 0 ok, -1 unknown name */
int osdsim_setting_get(const char *name, int *value);
const char *osdsim_setting_name(int i);     /* enumerate: NULL past the end */
u8 osdsim_menu_active(void);

/* recorded OSD state (chip-specific layout; see the chip's renderer) */
extern volatile u8 osdsim_sr[65536];
extern u8 osdsim_font[];
extern u8 osdsim_win[];
extern u8 osdsim_lut[];
extern u16 osdsim_origin_x, osdsim_origin_y;
extern u8 osdsim_osd_on;

#endif
