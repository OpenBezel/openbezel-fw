/* On-screen debug console: a small text grid drawn by the OSD engine and
 * refreshed from the main loop, so bench work does not need halt-and-inspect.
 *
 * The console owns a CON_ROWS x CON_COLS character buffer in XDATA.  Writers
 * poke text/hex into it (cheap, no hardware access); con_flush() pushes the
 * rows that changed into the OSD font SRAM and triggers the double buffer.
 * Row 0 is the banner, rows 1.. are free for status lines.
 *
 * Needs the display domain and OSD up (display_freerun_init + osd_init);
 * con_flush() is a no-op until con_enable() has been called.
 */
#ifndef CONSOLE_H
#define CONSOLE_H

#include "ob_types.h"

#define CON_ROWS 12             /* the menu lays out up to 11 items (core/menu.c); 8 are shown */
#define CON_COLS 40

void con_init(void);                              /* clear buffer (hardware untouched) */
void con_enable(u16 x, u16 y);                    /* osd rows on at active-area pixel (x,y); shows the buffer */
void con_enable_rows(u16 x, u16 y, u8 nrows);     /* as con_enable, but only `nrows` rows tall */
void con_colors(u8 fg, u8 bg);                    /* palette indices; bg 0 = transparent */
void con_disable(void);
void con_clear(void);
const __xdata char *con_row(u8 row);              /* the text of a row (core/gui.c skins it) */
void con_puts(u8 row, u8 col, const char *s);     /* ASCII text, clipped at the row end */
void con_putc(u8 row, u8 col, char c);
void con_hex8(u8 row, u8 col, u8 v);
void con_hex16(u8 row, u8 col, u16 v);
void con_dec(u8 row, u8 col, u16 v, u8 width);    /* right-aligned decimal in `width` cells */
void con_flush(void);                             /* push dirty rows to the OSD (call from the main loop) */

#endif
