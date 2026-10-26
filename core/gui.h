/* Graphical settings menu: the build's themes (tools/gen_theme.py), one
 * selected at run time, drawn with OSD windows, icon glyphs and the text of core/menu.c.
 *
 * A skin, not a second menu: menu.c lays every page out in the console buffer exactly
 * as for the text menu (title in row 0, items in rows 1-7, values from column 20), and
 * gui_render() redraws that text into its own, larger grid - header, category icons,
 * body, footer - with windows for the panel, header, dividers and selection.  The
 * one-row edit bar, the hotkey bars and the popups stay on the console.
 *
 * Sizes are in logical pixels; ob_settings.osd_scale shows them at 1x or 2x
 * (osd_double), set when the menu or a popup is shown. */
#ifndef OB_GUI_H
#define OB_GUI_H

#include "ob_types.h"
#include "theme.h"

/* Rows: header (18 lines), a 4-line pad, 18-line rows, a 4-line pad at the bottom -
 * the pads keep icon highlights clear of the header and the panel edge.  The layout
 * sets the count (core/gui.c layouts[]: 19 with the icons in a row, 17 with a sidebar). */
#define GUI_ROWS_MAX 19
#define GUI_BODY_ROWS 11        /* items shown before a page scrolls (core/menu.c) */
#define GUI_PAD 4               /* under the header */
#define GUI_PAD_BOTTOM 6
#define GUI_HEIGHT(rows) (18 + GUI_PAD + ((rows) - 3) * 18 + GUI_PAD_BOTTOM)   /* logical lines */
#define GUI_W 480
#define GUI_POP_MESSAGE 0       /* icon | console rows 0 (accent) and 1 (value), from column 4 */
#define GUI_POP_ADJUST 1        /* name | slider | value, as menu.c's edit row */

/* The current theme's sizes (gui_theme_apply), logical px: the menu's height (its width
 * is GUI_W), the popups' (one 40-cell row inset in a panel; a message has two rows
 * beside its icon), and how many console columns of the body rows are shown
 * (core/menu.c lays them out). */
extern __xdata u16 gui_h, gui_pop_w, gui_pop_h, gui_pop_msg_h;
extern __xdata u8 gui_body_cols;

/* Themes (tools/gen_theme.py, theme.h): ob_settings.theme holds a theme's stable id;
 * apply it (unknown ids select the build's default), the current one's index in
 * GUI_THEME_LABELS, and the id of an index.  A shown menu or popup keeps the old
 * look until gui_show() / gui_popup() runs again. */
void gui_theme_apply(void) __banked;
u8 gui_theme_index(void) __banked;
u8 gui_theme_id(u8 index) __banked;

u8 gui_scale(void) __banked;            /* 1 or 2, from ob_settings.osd_scale */
void gui_show(u16 x, u16 y) __banked;   /* the menu: palette + glyphs, rows, scale, OSD on at (x, y) */
/* a popup, themed, at (x, y) on the panel; again to redraw it.  icon: GUI_ICON_*
 * (theme.h) for a message */
void gui_popup(u16 x, u16 y, u8 kind, u8 icon) __banked;
void gui_hide(void) __banked;           /* windows off, 1x, OSD off - the console's again */
/* cat: category (main-page index) highlighted; in_page: the selection is in the body,
 * on console row sel_row - else on the icons, and the body is a preview */
void gui_render(u8 cat, u8 in_page, u8 sel_row) __banked;

#endif
