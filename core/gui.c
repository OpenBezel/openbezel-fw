/* Graphical settings menu - see gui.h. */
#include "gui.h"
#include "console.h"
#include "osd.h"
#include "assets.h"
#include "rtd2556_regs.h"
#include "theme.h"
#include "settings.h"
#include "lang.h"
#include "display.h"

#define G_COLS 40                /* the console's width: one character command per cell */
#define CELL_W 12
#define CELL_H 18
#define G_W GUI_W                /* logical pixels; the panel shows 2x */
#define G_H (GUI_HEIGHT(lay.rows) + th.inset)  /* the text grid sits INSET lines down (FRAME_CTRL_26) */

#define CHAR_BASE OSD_CHAR_BASE  /* row r, column c at CHAR_BASE + r * 40 + c */
#define ROW_CMD 0x80
#define T_1BPP 0x40
#define T_4BPP 0xC0

/* window numbers: 0 panel, 1 inner border (double), 5 header (gradient-capable),
 * 6/7 dividers, 8 selection, 9 icon highlight */
#define W_PANEL 0
#define W_INNER 1
#define W_STRIP 2               /* opaque band behind 4-bit icons on a translucent panel */
#define W_ICON2 3               /* vertical layout: second bracket of the icon highlight */
#define W_HEADER 5
#define W_DIV1 6
#define W_DIV2 7
#define W_SEL 8
#define W_ICON 9

/* window ADR3 byte 2 effect bits (the frame styles' own: tools/gen_theme.py) */
#define FX_ON 0x10
#define FX_BORDER (FX_ON | (7 << 1))
#define FX_GRADIENT_V 0x60
#define FX_BLEND 0x80

/* The built-in themes (tools/gen_theme.py), and the current one, copied to XDATA. */
static const __code struct gui_theme themes[GUI_THEME_COUNT] = GUI_THEMES;
static const __code u8 font_w[GUI_FONT_COUNT][ASSET_OSD_ASCII_COUNT + ASSET_OSD_LATIN1_COUNT] = GUI_FONT_WIDTHS;   /* ASCII, then Latin-1 */
static __xdata struct gui_theme th;
static __xdata u8 th_index = 0xFF;        /* themes[] index of th; 0xFF: none yet */

/* the layout's grid: rows (gui.h), 0 header, 1 pad, then 18-line rows from ICON_ROW,
 * last = bottom pad */
#define ICON_ROW 2
static const __code struct layout {
    u8 rows;
    u8 body_row;                /* console row 1 lands here */
    u8 body_col;                /* console column 1 lands here */
    u8 body_cols;               /* console columns 1..body_cols of the body rows are shown */
    u8 foot_row;
    u8 title_col;
    u8 sel_x0;                  /* the selection's left edge, px */
} layouts[2] = {
    { 19, 5, 1, 39, 17, 1, 4 },                 /* horizontal: icons in a row */
    { 17, 3, 7, 33, 15, 7, 6 * CELL_W },        /* vertical: icon sidebar */
};
static __xdata struct layout lay;

/* Icon glyphs go after the text font (ASSET_OSD_GLYPHS 1-bit glyphs from OSD_FONT_BASE).
 * A glyph index scales by the glyph size: 9 words for 1-bit, 36 for 4-bit.  A
 * character command's glyph index is 8 bits, so every icon cell stays below 256
 * (checked per icon set by gen_theme.py). */
#define ICON_CELLS (GUI_ICON_CW * GUI_ICON_CH)
static __xdata u8 icon_words, icon_idx0;

/* vertical layout: the icons sit in cell column 1; the divider goes where it centers
 * them between the panel's inner edge and itself */
#define ICON_X CELL_W
#define DIV_X (ICON_X + GUI_ICON_CW * CELL_W + (ICON_X - th.inset))

/* A text run starting at or after this console column is a value (menu.c: values
 * from column 20, the Information page's from 16; names always start at 2).  By
 * the run's START, not each character's column, so a value spanning the boundary
 * (the Information page's "1920 x1080") does not change color part way through. */
#define VALUE_SRC_COL 15

static __xdata u8 shown;        /* SHOWN_MENU / SHOWN_POPUP */
#define SHOWN_MENU 1
#define SHOWN_POPUP 2
static __xdata u8 sc;           /* 1x / 2x of the current showing */
static __xdata u8 g_cat, g_in_page, g_sel_row, g_fg;
static const char *__xdata g_hint;
/* window under construction: filled in, then win_put() */
static __xdata struct {
    u16 x0, y0, x1, y1;
    u8 color, fx, eff, bcol, scol, grad0;
} w;

__xdata u16 gui_h, gui_pop_w, gui_pop_h, gui_pop_msg_h;
__xdata u8 gui_body_cols;

u8 gui_scale(void) __banked { return ob_settings.osd_scale ? 2 : 1; }

/* Select the theme ob_settings.theme names (a stable id - the default if this build
 * has no such theme), and the sizes menu.c places things by. */
void gui_theme_apply(void) __banked
{
    u8 i;

    th_index = GUI_THEME_DEFAULT;
    i = GUI_THEME_COUNT;
    do {
        i--;
        if (themes[i].id == ob_settings.theme)
            th_index = i;
    } while (i);
    th = themes[th_index];
    lay = layouts[th.vertical];
    icon_words = 9;
    if (th.icon_bpp == 4)
        icon_words = 36;
    icon_idx0 = (u8)((ASSET_OSD_GLYPHS * 9 + icon_words - 1) / icon_words + 1);
    gui_h = G_H;
    gui_pop_w = GUI_W + 2 * (6 + th.inset);
    gui_pop_h = CELL_H + 2 * (4 + th.inset);
    gui_pop_msg_h = 2 * CELL_H + 2 * (4 + th.inset);
    gui_body_cols = lay.body_cols;
}

u8 gui_theme_index(void) __banked
{
    if (th_index == 0xFF)
        gui_theme_apply();
    return th_index;
}

u8 gui_theme_id(u8 index) __banked { return themes[index].id; }

/* top of row r, in window coordinates (the text grid is th.inset lines down) */
static u16 row_y(u8 r)
{
    if (r == 0)
        return th.inset;
    if (r == 1)
        return th.inset + CELL_H;
    return th.inset + CELL_H + GUI_PAD + (u16)(r - 2) * CELL_H;
}

/* win_put() / win_xy() state: wa is the window SRAM word being written next (a
 * window's four words ADR0-ADR3 in turn), t0 / t1 the bytes 0 and 1 of that word
 * being assembled. */
static __xdata u16 wa;
static __xdata u8 t0, t1;

/* Windows sit 10 OSD pixels left of the characters at the same coordinates (at 1x
 * and 2x alike): move them onto the text grid. */
#define WIN_DX 10

static void win_xy(u16 x, u16 y)
{
    x += WIN_DX;
    t0 = HI(x);
    t0 <<= 4;
    t0 |= HI(y);
    osd_sram_word(wa, t0, LO(x), LO(y), OSD_SRAM_WINDOW);
    wa++;
}

/* window n: 0-9 main windows (0x0100 + 4n), SUB_4_x the WIN4 sub-windows (0x0200 +
 * 4(x-1)) - same four words.  Sub-windows draw over ALL the main windows, a higher
 * one over a lower: with rounded corners the panel is 4_1, the
 * header 4_2 (sub-windows have no gradient), the selection 4_3. */
#define SUB_4_1 0x80
#define SUB_4_2 0x81
#define SUB_4_3 0x82

static void win_put(u8 n)
{
    if (n & 0x80)
        wa = 0x0200 + ((u16)(n & 0x7F) << 2);
    else
        wa = 0x0100 + ((u16)n << 2);
    t0 = 0;
    if (w.eff) {
        t0 = w.eff - 1;
        t0 |= t0 << 3;
    }
    if (w.scol & 0x10) t0 |= 0x80;
    if (w.bcol & 0x10) t0 |= 0x40;
    t1 = w.scol << 4;
    t1 |= w.bcol & 0x0F;
    osd_sram_word(wa, t0, t1, w.grad0, OSD_SRAM_WINDOW);
    wa++;
    win_xy(w.x0, w.y0);
    win_xy(w.x1, w.y1);
    t0 = 0;
    if (w.scol & 0x20) t0 |= 0x80;
    if (w.bcol & 0x20) t0 |= 0x40;
    if (w.color & 0x20) t0 |= 0x20;
    t1 = w.fx | 1;
    osd_sram_word(wa, t0, w.color & 0x1F, t1, OSD_SRAM_WINDOW);
}

static void win_rect(u16 x0, u16 y0, u16 x1, u16 y1, u8 color)
{
    w.x0 = x0; w.y0 = y0; w.x1 = x1; w.y1 = y1;
    w.color = color;
    w.fx = 0; w.eff = 0; w.bcol = 0; w.scol = 0; w.grad0 = 0;
}

static void win_off(u8 n)
{
    if (n & 0x80)
        osd_sram_word(0x0200 + 4 * (n & 0x7F) + 3, 0, 0, 0, OSD_SRAM_WINDOW);
    else
        osd_sram_word(0x0100 + 4 * n + 3, 0, 0, 0, OSD_SRAM_WINDOW);
}

/* Rounded corners (windows 0, 5 and the sub-windows): the window's ADR10 word -
 * corners [6:3] (upper-left, lower-left, upper-right, lower-right), size [2:1]
 * (8/16/32), enable [0].  A rounded window may not have a border, shadow or 3D
 * effect.  The corner shape is the table setup() loads (gen_theme.py). */
#define RW_0 0x0178
#define RW_5 0x019B
#define RW_4_1 0x01BE
#define RW_4_2 0x01C5
#define RW_4_3 0x01CC
#define RC_ALL 0x78
#define RC_TOP 0x50
#define RC_BOT 0x28

/* the corner size for a rounded window h lines tall: the theme's, or smaller - a
 * rounded window must be taller than twice its radius */
static u8 rcode(u16 h)
{
    if (th.round_code >= 2 && h > 64)
        return 2;
    if (th.round_code >= 1 && h > 32)
        return 1;
    return 0;
}

/* a window's look from the theme (fill, effect, colors) over the rectangle win_rect set */
static void win_style(const __xdata struct gui_win *s)
{
    w.color = s->color;
    w.fx = s->fx;
    w.eff = s->eff;
    w.scol = s->scol;
    w.bcol = s->bcol;
}

static __xdata u16 pan_top;              /* rounded: where the panel body starts (below a header) */
static __xdata u8 pan_corners;           /* ... and the body's rounded corners */
static __xdata u8 pan_c_all, pan_c_body; /* corner sizes (rcode) for the whole / the body */

static void round_set(u16 adr10, u8 corners, u8 code)
{
    osd_sram_word(adr10, corners ? (u8)(corners | (code << 1) | 1) : 0, 0, 0, OSD_SRAM_WINDOW);
}

/* ---- row composer -----------------------------------------------------------
 * A row is a run of character commands, each with its own width (B0[3:0], up to 12
 * px): proportional text in the theme font, 12-px icon and slider cells, and blank
 * spacers to reach a position.  The hardware reads row r+1's cells straight after
 * row r's, so every row is padded to ROW_CELLS with 1-px transparent cells: each
 * row then starts at a fixed place, and cells a render does not change (the icons,
 * on a page switch) are rewritten in place with the same values instead of moving
 * while the panel scans them out.
 * Text placed from menu.c's console layout starts each run where its column would
 * (column * 12), so whatever lined up there still lines up. */
static __xdata u8 rc_row, rc_n, rc_h, rc_k, rc_w;
static __xdata u16 rc_x, rc_base;      /* rc_base: this row's first cell in the char area */

/* cells per row: the character area (CHAR_BASE up to the glyphs) shared by the most
 * rows a layout has */
#define ROW_CELLS_FIT ((OSD_FONT_BASE - CHAR_BASE) / GUI_ROWS_MAX)
#define ROW_CELLS (ROW_CELLS_FIT < 64 ? ROW_CELLS_FIT : 64)
static __xdata u8 rc_prev[GUI_ROWS_MAX];   /* each row's content cells at the last render */

/* the rows' cells are unknown (a new layout): pad every row in full on the next render */
static void row_reset(void)
{
    for (rc_row = 0; rc_row < GUI_ROWS_MAX; rc_row++)
        rc_prev[rc_row] = ROW_CELLS;
}
static __xdata u8 g_mode, g_src;        /* what text_fg() colors: TM_* / the console row */
#define TM_TITLE 0
#define TM_BODY 1
#define TM_ADJUST 2
#define TM_MESSAGE 3
#define TM_HINT 4

static void row_begin(u8 r, u8 h1)
{
    rc_row = r;
    rc_base = (u16)r * ROW_CELLS;
    rc_n = 0;
    rc_x = 0;
    rc_h = h1;
}

static void put(u8 b0, u8 b1, u8 b2, u8 wpx)
{
    if (rc_n >= ROW_CELLS)
        return;
    osd_sram_word(CHAR_BASE + rc_base + rc_n, b0, b1, b2, OSD_SRAM_FONT);
    rc_n++;
    rc_x += wpx;
}

/* Spacers of 12 px, the last two sharing the rest so none is under PAD_MIN px: a
 * value after a 1-px spacer loses its first character. */
#define PAD_MIN 4

static void pad_to(u16 x)
{
    while (rc_x < x) {
        rc_w = CELL_W;
        if (x - rc_x < CELL_W + PAD_MIN) {
            if (x - rc_x <= CELL_W)
                rc_w = (u8)(x - rc_x);              /* the last one */
            else
                rc_w = (u8)((x - rc_x) / 2);        /* 13-15 px: two of 6-8 */
        }
        if (rc_w < PAD_MIN && rc_n)
            break;                                  /* under PAD_MIN: let it be 1-3 px short */
        put(T_1BPP | rc_w, 0, 0, rc_w);             /* a space, transparent */
    }
}

/* Pad to ROW_CELLS: only the cells that held content last time need rewriting. */
static void row_end(void)
{
    rc_k = rc_prev[rc_row];
    rc_prev[rc_row] = rc_n;
    while (rc_n < rc_k)
        put(T_1BPP | 1, 0, 0, 1);
    osd_sram_word(rc_row, ROW_CMD, rc_h, ROW_CELLS, OSD_SRAM_FONT);
}

static u8 text_fg(u8 col, u8 k)
{
    switch (g_mode) {
    case TM_TITLE:
        return GUI_C_TITLE;
    case TM_HINT:
        return GUI_C_TEXT_DIM;
    case TM_MESSAGE:
        if (g_src)
            return GUI_C_VALUE;
        return GUI_C_TITLE;                         /* a message: accent, then the details */
    case TM_ADJUST:
        if (k >= ASSET_OSD_CHAR_BAR0)
            return GUI_C_SLIDER;
        if (col >= 30)
            return GUI_C_VALUE;
        return GUI_C_TEXT;
    }
    if (!g_in_page)
        return GUI_C_TEXT_DIM;                      /* a preview of the category */
    if (g_src == g_sel_row) {
        if (!th.outline_sel)
            return GUI_C_TEXT_SEL;                  /* on the selection bar */
        if (k >= ASSET_OSD_CHAR_BAR0)
            return GUI_C_SLIDER;                    /* the in-row slider */
        return GUI_C_TEXT;
    }
    if (col >= VALUE_SRC_COL)
        return GUI_C_VALUE;
    return GUI_C_TEXT;
}

/* character k -> its glyph (rc_k) and cell width (rc_w): ASCII and the bar glyphs
 * (the slider; they tile at 12) from glyph 0, the Latin-1 letters 0xC0-0xFF (the
 * translations, lang/) from ASSET_OSD_LATIN1 */
static void glyph_of(u8 k)
{
    if (k >= ASSET_OSD_LATIN1_FIRST) {
        rc_k = k - ASSET_OSD_LATIN1_FIRST;
        rc_w = font_w[th.font][ASSET_OSD_ASCII_COUNT + rc_k];
        rc_k += ASSET_OSD_LATIN1;
        return;
    }
    if (k < ASSET_OSD_ASCII_FIRST)
        k = '?';
    if (k >= ASSET_OSD_CHAR_END)
        k = '?';
    rc_k = k - ASSET_OSD_ASCII_FIRST;
    rc_w = CELL_W;
    if (rc_k < ASSET_OSD_ASCII_COUNT)
        rc_w = font_w[th.font][rc_k];
}

static void put_char(u8 k, u8 col)
{
    glyph_of(k);
    put(T_1BPP | rc_w, rc_k, (u8)(text_fg(col, k) << 4), rc_w);
}

/* console row g_src, columns [c0, c0 + n), as text runs from pixel x0 */
static __xdata u8 cr_c, cr_end, cr_run;     /* cr_run: the current run's first column */
static const __xdata char *__xdata cr_s;

static void con_runs(u8 c0, u8 n, u16 x0)
{
    cr_s = con_row(g_src);
    cr_end = c0 + n;
    cr_c = c0;
    while (cr_c < cr_end) {
        if (cr_s[cr_c] == ' ') {
            cr_c++;
            continue;
        }
        pad_to(x0 + (u16)(cr_c - c0) * CELL_W);
        cr_run = cr_c;
        while (cr_c < cr_end) {
            if (cr_s[cr_c] == ' ') {                /* two spaces (or the end) end a run */
                if (cr_c + 1 >= cr_end)
                    break;
                if (cr_s[cr_c + 1] == ' ')
                    break;
            }
            if (g_mode == TM_ADJUST)
                put_char((u8)cr_s[cr_c], cr_c);     /* bar | value in one run: by column */
            else
                put_char((u8)cr_s[cr_c], cr_run);
            cr_c++;
        }
    }
}

static __xdata u16 tw;

static u16 text_w(const char *t)
{
    tw = 0;
    while (*t) {
        glyph_of((u8)*t);
        tw += rc_w;
        t++;
    }
    return tw;
}

static void text(const char *t)
{
    while (*t) {
        put_char((u8)*t, 0);
        t++;
    }
}

/* Lines between areas, in the frame style's look. */
static void divider(u8 n, u16 x0, u16 y0, u16 x1, u16 y1)
{
    if (!th.divider) {
        win_off(n);
        return;
    }
    win_rect(x0, y0, x1, y1, 0);
    win_style(&th.div);
    win_put(n);
}

static void panel(u16 w_, u16 h_);

/* frame() / panel() / icon_highlight() coordinates */
static __xdata u16 cx0, cy0, cx1, cy1;

static void frame(void)
{
    pan_top = 0;
    if (th.round_code != 0xFF)
        pan_top = row_y(1);                         /* rounded: the body below the header */
    panel(G_W, G_H);
    /* header band: row 0 */
    cx1 = G_W - 1;
    cx1 -= th.inset;
    cy1 = th.inset;
    cy1 += CELL_H - 1;
    win_rect(th.inset, th.inset, cx1, cy1, GUI_C_HEADER_FILL);
    if (th.round_code != 0xFF) {
        /* A sub-window above the body (4_1); solid - no gradient on a sub-window, so its
         * far end.  It ends on the body's first line: a sub-window directly below another
         * leaves a one-line gap between them. */
        w.y1++;
        if (th.gradient)
            w.color = GUI_C_HEADER_FILL2;
        win_put(SUB_4_2);
        round_set(RW_4_2, RC_TOP, rcode(CELL_H));   /* one row: 8-px corners */
    } else {
        if (th.gradient) {
            w.fx = FX_GRADIENT_V;
            w.grad0 = th.grad0;
        }
        win_put(W_HEADER);
    }
    cy1 = G_H - 1;                                  /* cx1: the panel's inner right edge */
    cy1 -= th.inset;                                /* cy1: ... and bottom */
    cy0 = row_y(1);
    if (th.icon_strip) {                            /* a solid band behind the icons */
        cy0++;
        if (th.vertical) {
            cx0 = DIV_X - 1;
            win_rect(th.inset, cy0, cx0, cy1, GUI_C_FOOTER_FILL);
        } else {
            cx0 = row_y(ICON_ROW + 2) + 3;
            win_rect(th.inset, cy0, cx1, cx0, GUI_C_FOOTER_FILL);
        }
        cy0--;
        win_put(W_STRIP);
    } else {
        win_off(W_STRIP);
    }
    cx1 -= 4;                                       /* dividers stop 4 px short of the edge */
    if (th.vertical) {
        cx0 = DIV_X;
        cy0 += 2;
        divider(W_DIV1, cx0, cy0, cx0 + 1, cy1);
        cy0 = row_y(lay.foot_row) - 4;
        divider(W_DIV2, 6 * CELL_W, cy0, cx1, cy0 + 1);
    } else {
        cx0 = th.inset + 4;
        cy0 = row_y(ICON_ROW + 2) + 8;
        divider(W_DIV1, cx0, cy0, cx1, cy0 + 1);
        cy0 = row_y(lay.foot_row - 1) + 8;
        divider(W_DIV2, cx0, cy0, cx1, cy0 + 1);
    }
}

/* what the menu and the popups share: palette, fonts, the OSD-wide controls */
static const __code u8 round8_tab[4] = GUI_ROUND8_TABLE;
static const __code u8 round16_tab[8] = GUI_ROUND16_TABLE;
static const __code u8 round32_tab[32] = GUI_ROUND32_TABLE;

static void setup(void)
{
    u8 i;

    if (th_index == 0xFF)
        gui_theme_apply();
    con_disable();                  /* the console must not flush over these cells */
    for (i = 0; i < GUI_PALETTE_N; i++)
        osd_lut_set(i, th.palette[i][0], th.palette[i][1], th.palette[i][2]);
    osd_load_asset(OSD_FONT_BASE, ASSET_THEMES_OFF + th.font_off, ASSET_OSD_FONT_LEN);
    osd_load_asset(OSD_FONT_BASE + (u16)icon_idx0 * icon_words, ASSET_THEMES_OFF + th.icons_off, th.icons_len);
    SR(OVERLAY_CTRL1_M1) = (u8)(th.alpha << 3);
    SR_SET(FRAME_CTRL_07, FRAME_CTRL_07_F4B_COLOR_SEL);    /* 4-bit cells: B0[3:0] = background */
    sc = gui_scale();
    osd_double(sc == 2);
    for (i = 0; i < 4; i++)
        SR(FRAME_CTRL_60 + i) = round8_tab[i];
    for (i = 0; i < 8; i++)
        SR(FRAME_CTRL_64 + i) = round16_tab[i];
    for (i = 0; i < 32; i++)
        SR(FRAME_CTRL_6C + i) = round32_tab[i];
}

static void font_a(u16 x, u16 y)
{
    SR(FRAME_CTRL_24) = (u8)(HI(x) << 4 | HI(y));
    SR(FRAME_CTRL_25) = LO(x);
    SR(FRAME_CTRL_26) = LO(y);
}

static void all_windows_off(void)
{
    win_off(W_PANEL); win_off(W_INNER); win_off(W_HEADER);
    win_off(W_DIV1); win_off(W_DIV2); win_off(W_SEL); win_off(W_ICON);
    win_off(W_STRIP); win_off(W_ICON2); win_off(SUB_4_1); win_off(SUB_4_2); win_off(SUB_4_3);
    round_set(RW_0, 0, 0); round_set(RW_5, 0, 0); round_set(RW_4_1, 0, 0); round_set(RW_4_2, 0, 0);
    round_set(RW_4_3, 0, 0);
}

/* The panel in the frame style: border / bevel / shadow / translucency.
 * Square: one window with the style's effect (the double border adds an inner one).
 * Rounded: a rounded window cannot have a border or shadow, so the frame is rounded
 * windows side by side - window 0 behind (the border color, or the shadow, offset),
 * the body in sub-window 4_1 from pan_top down, a header (frame()) in 4_2 above it,
 * overlapping the body's first line (which closes the gap the hardware leaves between
 * them).  A bevel cannot be rounded: drawn as a line. */
static void panel(u16 w_, u16 h_)
{
    cx1 = w_ - 1;
    cy1 = h_ - 1;
    if (th.round_code == 0xFF) {
        win_rect(0, 0, cx1, cy1, 0);
        win_style(&th.panel);
        win_put(W_PANEL);
        if (th.border == GUI_BORDER_DOUBLE) {
            cx0 = th.border_w + 2;
            cx1 -= cx0;
            cy1 -= cx0;
            win_rect(cx0, cx0, cx1, cy1, 0);
            win_style(&th.inner);
            win_put(W_INNER);
        }
        return;
    }
    pan_corners = RC_ALL;
    if (pan_top)
        pan_corners = RC_BOT;                       /* the header has the top corners */
    pan_c_all = rcode(h_);
    pan_c_body = rcode(h_ - pan_top);
    if (th.border == GUI_BORDER_SHADOW) {
        cx0 = cx1 + th.border_w;
        cy0 = cy1 + th.border_w;
        win_rect(th.border_w, th.border_w, cx0, cy0, GUI_C_SHADOW);
        win_put(W_PANEL);
        round_set(RW_0, RC_ALL, pan_c_all);
        win_rect(0, pan_top, cx1, cy1, GUI_C_PANEL_FILL);
    } else if (th.border == GUI_BORDER_NONE) {
        win_rect(0, pan_top, cx1, cy1, GUI_C_PANEL_FILL);
    } else {
        win_rect(0, 0, cx1, cy1, GUI_C_BORDER);
        win_put(W_PANEL);
        round_set(RW_0, RC_ALL, pan_c_all);
        cy0 = pan_top;
        if (!cy0)
            cy0 = th.border_w;
        cx1 -= th.border_w;
        cy1 -= th.border_w;
        win_rect(th.border_w, cy0, cx1, cy1, GUI_C_PANEL_FILL);
    }
    if (th.alpha)
        w.fx = FX_BLEND;
    win_put(SUB_4_1);
    round_set(RW_4_1, pan_corners, pan_c_body);
}

static u8 row_h1(u8 r)
{
    if (r == 1)
        return GUI_PAD - 1;
    if (r == lay.rows - 1)
        return GUI_PAD_BOTTOM - 1;
    return CELL_H - 1;
}

static __xdata u8 pop_kind;     /* the popup's layout, while one is up */

static void icon_cells(u8 i, u8 half);

static __xdata u8 pp_r, pp_rows;                 /* gui_popup's */

void gui_popup(u16 x, u16 y, u8 kind, u8 icon_i) __banked
{
    pp_rows = 1;
    if (kind == GUI_POP_MESSAGE)
        pp_rows = 2;
    if (shown != SHOWN_POPUP || pop_kind != kind) {
        setup();
        for (pp_r = 0; pp_r < pp_rows; pp_r++)
            osd_sram_word(pp_r, ROW_CMD, CELL_H - 1, ROW_CELLS, OSD_SRAM_FONT);
        osd_sram_word(pp_rows, 0, 0, 0, OSD_SRAM_FONT);
        row_reset();
        SR(FRAME_CTRL_3E) = pp_rows; SR(FRAME_CTRL_3F) = pp_rows;
        font_a(6 + th.inset, 4 + th.inset);
        all_windows_off();
        pan_top = 0;                                /* no header: all four corners */
        if (pp_rows == 2)
            panel(gui_pop_w, gui_pop_msg_h);
        else
            panel(gui_pop_w, gui_pop_h);
        pop_kind = kind;
        osd_origin(x / sc, y / sc);
        shown = SHOWN_POPUP;
        osd_on();
    } else {
        osd_origin(x / sc, y / sc);     /* the OSD-position edit moves it */
    }
    g_cat = 0xFF;                                   /* (1-bit icons: not "selected") */
    for (pp_r = 0; pp_r < pp_rows; pp_r++) {
        row_begin(pp_r, CELL_H - 1);
        g_src = pp_r;
        if (kind == GUI_POP_MESSAGE) {              /* icon | text from column 4 */
            icon_cells(icon_i, pp_r);
            g_mode = TM_MESSAGE;
            con_runs(4, G_COLS - 4, 4 * CELL_W);
        } else {
            g_mode = TM_ADJUST;
            con_runs(0, G_COLS, 0);
        }
        row_end();
    }
    osd_commit();
}

void gui_show(u16 x, u16 y) __banked
{
    u8 i;

    setup();
    /* rows: lay.rows rows of ROW_CELLS cells, 18 lines or a pad, font A at the origin */
    for (i = 0; i < lay.rows; i++)
        osd_sram_word(i, ROW_CMD, row_h1(i), ROW_CELLS, OSD_SRAM_FONT);
    osd_sram_word(lay.rows, 0, 0, 0, OSD_SRAM_FONT);
    row_reset();
    font_a(0, th.inset);                                /* the text grid below the border */
    SR(FRAME_CTRL_3E) = lay.rows; SR(FRAME_CTRL_3F) = lay.rows;
    all_windows_off();
    frame();
    osd_origin(x / sc, y / sc);     /* 2x doubles the origin delay too */
    shown = SHOWN_MENU;
    osd_on();                       /* commits */
}

void gui_hide(void) __banked
{
    if (!shown)
        return;
    shown = 0;
    all_windows_off();
    font_a(0, 0);
    SR(OVERLAY_CTRL1_M1) = 0;
    osd_double(0);
    osd_off();
}

/* top-left of the highlighted icon, logical pixels. */
static volatile __xdata u16 ix, iy;

/* The highlighted icon (frame style "icon_select"): a frame round it, bars either
 * side, or one bar on the left or right.  Bars are 4 px, 2 px clear of the icon. */
static void icon_highlight(void)
{
    if (th.vertical) {
        ix = ICON_X;
        iy = row_y(ICON_ROW + 2 * g_cat);
    } else {
        ix = (3 + 5 * (u16)g_cat) * CELL_W;
        iy = row_y(ICON_ROW);
    }
    cy1 = iy + GUI_ICON_CH * CELL_H - 1;            /* the icon's last line */
    if (th.icon_select == GUI_ISEL_FRAME) {
        cx0 = ix - 4;
        cy0 = iy - 3;
        cx1 = ix + GUI_ICON_CW * CELL_W + 3;
        cy1 += 3;
        win_rect(cx0, cy0, cx1, cy1, GUI_C_PANEL_FILL);
        w.fx = FX_BORDER; w.eff = 2; w.bcol = GUI_C_SEL_BORDER;
        win_put(W_ICON);
        win_off(W_ICON2);
        return;
    }
    if (th.icon_select != GUI_ISEL_RIGHT) {         /* brackets, left */
        cx0 = ix - 6;
        cx1 = ix - 3;
        win_rect(cx0, iy, cx1, cy1, GUI_C_SEL_BORDER);
        win_put(W_ICON);
    } else {
        win_off(W_ICON);
    }
    if (th.icon_select != GUI_ISEL_LEFT) {          /* brackets, right */
        cx0 = ix + GUI_ICON_CW * CELL_W + 2;
        cx1 = cx0 + 3;
        win_rect(cx0, iy, cx1, cy1, GUI_C_SEL_BORDER);
        win_put(W_ICON2);
    } else {
        win_off(W_ICON2);
    }
}

/* one cell row (half 0 / 1) of icon i, at the composer's position */
static void icon_cells(u8 i, u8 half)
{
    u8 cx, k;

    k = icon_idx0;
    k += i * ICON_CELLS;
    k += half * GUI_ICON_CW;
    g_fg = (u8)(GUI_C_ICON_MAIN << 4);               /* 1-bit icons: fg, bg 0 */
    if (i == g_cat)
        g_fg = (u8)(GUI_C_ICON_SEL << 4);
    for (cx = 0; cx < GUI_ICON_CW; cx++) {
        if (th.icon_bpp == 4)
            /* B0[3:0] = background entry for value-0 pixels, 0 = transparent
             * (FRAME_CTRL_07 f4b_color_sel, set in setup()); the cell is 12 px */
            put(T_4BPP | 0, k, 0x00, CELL_W);
        else
            put(T_1BPP | CELL_W, k, g_fg, CELL_W);
        k++;
    }
}


void gui_render(u8 cat, u8 in_page, u8 sel_row) __banked
{
    u8 r, i;

    if (!shown)
        return;
    g_cat = cat;
    g_in_page = in_page;
    g_sel_row = sel_row;
    for (r = 0; r < lay.rows; r++) {
        row_begin(r, row_h1(r));
        if (r == 0) {                               /* the page title, before its key hint */
            g_mode = TM_TITLE;
            g_src = 0;
            con_runs(1, 14, lay.title_col * CELL_W);
        }
        if (th.vertical) {
            if (r >= ICON_ROW && r < ICON_ROW + 2 * GUI_ICON_COUNT) {
                pad_to(ICON_X);
                icon_cells((r - ICON_ROW) >> 1, (r - ICON_ROW) & 1);
            }
        } else if (r == ICON_ROW || r == ICON_ROW + 1) {
            for (i = 0; i < GUI_ICON_COUNT; i++) {
                pad_to((3 + 5 * (u16)i) * CELL_W);
                icon_cells(i, r - ICON_ROW);
            }
        }
        if (r >= lay.body_row && r < lay.body_row + GUI_BODY_ROWS) {   /* console rows 1 - */
            g_mode = TM_BODY;
            g_src = r - lay.body_row + 1;
            con_runs(1, lay.body_cols, lay.body_col * CELL_W);
        }
        if (r == lay.foot_row) {                    /* the key hint, centered */
            g_mode = TM_HINT;
            g_hint = lang_str(S_HINT);
            tw = text_w(g_hint);
            if (tw > lay.body_cols * CELL_W)
                tw = lay.body_cols * CELL_W;
            pad_to(lay.body_col * CELL_W + (lay.body_cols * CELL_W - tw) / 2);
            text(g_hint);
        }
        row_end();
    }
    /* Window words take effect as they are written: a scan passing between a window's
     * start and end words draws it with the new start and the old end (the selection
     * stretched over several items for a frame).  Write them in the vertical blanking. */
    display_wait_vsync();
    icon_highlight();
    if (in_page) {
        iy = row_y(lay.body_row - 1 + sel_row);
        win_rect(lay.sel_x0, iy - 1, G_W - 1 - th.inset - 4, iy + CELL_H, GUI_C_SEL_FILL);
        if (th.sel_round) {
            win_put(SUB_4_3);                       /* a pill: rounded, so effects-free */
            round_set(RW_4_3, RC_ALL, 0);
        } else {
            win_style(&th.sel);
            win_put(W_SEL);
        }
    } else {
        win_off(W_SEL);
        win_off(SUB_4_3);
    }

    osd_commit();
}
