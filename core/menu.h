/* On-screen settings menu and the input-mode popup, drawn with the OSD text
 * console (core/console.c, 8 rows x 40 cols).
 *
 * Both are overlays: they take over the console rows while open and hand them
 * back on close.  The caller owns the UI mode, so menu_close() only turns the
 * OSD rows off - main.c restores whatever the current mode wants.
 */
#ifndef OB_MENU_H
#define OB_MENU_H

#include "ob_types.h"

void menu_open(void) __banked;
void menu_close(void) __banked;
void menu_set_locked(u8 on) __banked;    /* MCCS CAh: refuse menu_open while set */
u8 menu_active(void) __banked;           /* 1 while the menu owns the console */
void menu_key(u8 key) __banked;          /* KEY_UP / KEY_DOWN / KEY_ENTER / KEY_POWER */
void menu_hotkey(u8 key) __banked;       /* menu closed: UP/DOWN volume bar, POWER input picker */
#define MENU_REQ_DEBUG 1                 /* the menu asks main.c for the debug console */
u8 menu_take_request(void) __banked;     /* MENU_REQ_* or 0, cleared by reading */

/* Input-mode popup: active size + TMDS clock, shown for a few seconds when the
 * source timing changes.  Suppressed when ob_settings.res_popup is 0. */
void menu_popup_mode(u16 t0ovf) __banked;
void menu_popup_nosignal(u16 t0ovf) __banked;   /* "No signal" box */
void menu_popup_nosignal_off(void) __banked;    /* ... down again (picture up) */
u8 menu_popup_active(void) __banked;
/* Drop a pending popup without touching the console - for a caller that is
 * about to take the console over itself.  See menu.c. */
void menu_popup_cancel(void) __banked;

/* Expire the popup.  Timed in Timer0 overflows (~29 ms each on the crystal
 * clock), the same base the splash ceiling uses - the main loop's own spin rate
 * is not a usable clock. */
void menu_tick(u16 t0ovf) __banked;

#endif /* OB_MENU_H */
