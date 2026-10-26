/* RTD2556 OSD text and boot logo - see osd.h. */
#include "osd.h"
#include "flash_assets.h"
#include "rtd2556_regs.h"
#include "board.h"

/* The firmware's fixed colors, loaded from OSD_COL_ORANGE up: the brand orange
 * (not red) for the logo accent, an opaque dark background (LUT 0 is the
 * transparency key, so it needs its own entry), white. */
static const __code u8 palette[3][3] = {           /* OSD_COL_ORANGE, _BG_DARK, _WHITE */
    {0xFF, 0xB0, 0x20}, {0x10, 0x10, 0x18}, {0xFF, 0xFF, 0xFF},
};

/* character command: B0 = type (1 bpp) | glyph width, B1 = glyph index, B2 = fg/bg palette */
#define CHAR_CMD_1BPP 0x40
/* row command: B0 [7] = row, B1 = row height - 1 (verified on hardware), B2 = length */
#define ROW_CMD 0x80

static void osd_word(u16 addr, u8 b0, u8 b1, u8 b2, u8 sram)
{
    SR(OSD_WRITE_OPTION) = OSD_WRITE_OPTION_BYTESEL_ALL | sram;  /* all three bytes */
    SR(OSD_ADR_PORT_MSB) = HI(addr) & 0x7F;
    SR(OSD_ADR_PORT_LSB) = LO(addr);
    SR(OSD_DATA_PORT) = b0; SR(OSD_DATA_PORT) = b1; SR(OSD_DATA_PORT) = b2;
    SR(OSD_WRITE_OPTION) = 0x00;                                 /* deselect (glitch avoidance) */
}

void osd_sram_word(u16 addr, u8 b0, u8 b1, u8 b2, u8 sram)
{
    osd_word(addr, b0, b1, b2, sram);
}

void osd_lut_set(u8 i, u8 r, u8 g, u8 b)
{
    SR(OVERLAY_LUT_ADDR) = OVERLAY_LUT_ADDR_LUT_ACC_EN | (i & OVERLAY_LUT_ADDR_LUTADR);
    SR(COLOR_LUT_PORT) = r; SR(COLOR_LUT_PORT) = g; SR(COLOR_LUT_PORT) = b;
    SR(OVERLAY_LUT_ADDR) = 0x00;
}

void osd_commit(void)
{
    SR(OSD_MISC_PORT) = OSD_MISC_PORT_DB_ENABLE;     /* pulse: the global double-buffer load */
    SR(OSD_MISC_PORT) = 0x00;
}

/* OSD origin in active-area pixels.  The delays count from the syncs: the DTG
 * free-run timing starts active video 48 px / 6 lines in, and the OSD pipeline
 * adds ~28 px more (measured). */
void osd_origin(u16 x, u16 y)
{
    u16 h = (x + 76) >> 2, v = y + 6;                 /* 4-px horizontal steps */
    SR(FRAME_CTRL_00) = v >> 3;                       /* vert_delay1[10:3] */
    SR(FRAME_CTRL_01) = h >> 2;                       /* hori_delay1[9:2] */
    SR(FRAME_CTRL_02) = (SR(FRAME_CTRL_02) & (FRAME_CTRL_02_OSD_EN | FRAME_CTRL_02_DISP_ZONE))
                      | ((h & 3) << FRAME_CTRL_02_HORI_DELAY1_SHIFT)
                      | ((v & 7) << FRAME_CTRL_02_VERT_DELAY1_SHIFT);
}

/* Stream an asset-partition blob into the OSD SRAM.  Same shape as osd_word()
 * - select the SRAM, set the start address - but the bytes come from the burst
 * engine instead of the CPU.  The data port advances the address itself, which
 * is what makes a burst into it possible. */
static void osd_burst(u16 addr, u16 off, u16 len)
{
    SR(OSD_WRITE_OPTION) = OSD_WRITE_OPTION_BYTESEL_ALL | OSD_SRAM_FONT;
    SR(OSD_ADR_PORT_MSB) = HI(addr) & 0x7F;
    SR(OSD_ADR_PORT_LSB) = LO(addr);
    asset_burst_to_port(off, len, 0x00, (u8)OSD_DATA_PORT);
    SR(OSD_WRITE_OPTION) = 0x00;                                 /* deselect */
}

void osd_load_asset(u16 addr, u16 off, u16 len)
{
    osd_burst(addr, off, len);
}

void osd_double(u8 on)
{
    SR(FRAME_CTRL_0A) = (SR(FRAME_CTRL_0A) & (u8)~(FRAME_CTRL_0A_DB_WIDTH | FRAME_CTRL_0A_DB_HEIGHT))
                      | (on ? (FRAME_CTRL_0A_DB_WIDTH | FRAME_CTRL_0A_DB_HEIGHT) : 0);
}

/* (re)load the 95 text glyphs into font SRAM at OSD_FONT_BASE. */
void osd_load_text_font(void)
{
    osd_burst(OSD_FONT_BASE, ASSET_OSD_FONT_OFF, ASSET_OSD_FONT_LEN);
}

void osd_init(void)
{
    u16 a;
    u8 i;

    /* color LUT: 0 (the transparency key) and the firmware's three fixed colors */
    osd_lut_set(0, 0, 0, 0);
    for (i = 0; i < 3; i++)
        osd_lut_set(OSD_COL_ORANGE + i, palette[i][0], palette[i][1], palette[i][2]);
    /* frame control registers clear; origin at the top-left of the active area */
    for (i = 0; i < 0x30; i++)
        SR(FRAME_CTRL_00 + i) = 0;
    SR_CLR(FRAME_CTRL_06, FRAME_CTRL_06_HORI_DELAY_STEP_SEL);   /* 4 px horizontal steps */
    /* A cell narrower than 12 px shows the LEFT part of its glyph (the reset value,
     * "middle", shows the center): the menu's proportional font (tools/gen_font.py)
     * is drawn left-aligned.  12-px cells are unaffected. */
    SR_WRF(FRAME_CTRL_02, FRAME_CTRL_02_DISP_ZONE, FRAME_CTRL_02_DISP_ZONE_LEFT);
    osd_origin(0, 0);
    /* all windows off (0x0100-0x025F), row area clear */
    for (a = 0x0100; a < 0x0260; a++)
        osd_word(a, 0, 0, 0, OSD_SRAM_WINDOW);
    for (a = 0; a < 0x0100; a++)
        osd_word(a, 0, 0, 0, OSD_SRAM_WINDOW);
    /* font SRAM: command area (debug mailbox lives at 0x1F00) */
    for (a = 0x0000; a < OSD_FONT_BASE; a++)
        osd_word(a, 0, 0, 0, OSD_SRAM_FONT);
    osd_load_text_font();
    /* bases: glyphs at OSD_FONT_BASE, character commands ("font select") at
     * OSD_CHAR_BASE, row commands at OSD_ROW_BASE */
    SR(FRAME_CTRL_0C) = LO(OSD_CHAR_BASE);                                      /* fs_base[7:0] */
    SR(FRAME_CTRL_0D) = ((HI(OSD_CHAR_BASE) & 0x0F) << FRAME_CTRL_0D_FS_BASE_SHIFT) | (OSD_FONT_BASE & 0x0F);
    SR(FRAME_CTRL_0E) = (OSD_FONT_BASE >> 4) & 0xFF;                               /* font_base[11:4] */
    SR(FRAME_CTRL_17) = ((OSD_FONT_BASE >> 12) & 0x07) << FRAME_CTRL_17_FONT_BASE_SHIFT;  /* no VLC compression */
    SR(FRAME_CTRL_19) = LO(OSD_ROW_BASE); SR(FRAME_CTRL_1A) = LO(OSD_ROW_BASE);   /* row_base0/1 */
    SR(FRAME_CTRL_1B) = ((HI(OSD_CHAR_BASE) & 0x0F) << FRAME_CTRL_1B_FS_BASE0_SHIFT) | (HI(OSD_CHAR_BASE) & 0x0F);
    SR(FRAME_CTRL_1C) = LO(OSD_CHAR_BASE); SR(FRAME_CTRL_1D) = LO(OSD_CHAR_BASE);  /* fs_base0/1[7:0] */
    SR(FRAME_CTRL_0A) = FV(FRAME_CTRL_0A_ALPHA_BLEND_TYPE, 1);
    SR(FRAME_CTRL_18) = FRAME_CTRL_18_FNT_WIN_V_BD_ALIGN;
    SR(FRAME_CTRL_3E) = 1; SR(FRAME_CTRL_3F) = 1;                                  /* one text row until osd_rows_set */
    /* OSD area = the panel: len1[11:0] = width, wid1[11:0] = height */
    SR(FRAME_CTRL_38) = FV(FRAME_CTRL_38_OSD_LEN1, HI(PANEL_WIDTH)) | FV(FRAME_CTRL_38_OSD_WID1, HI(PANEL_HEIGHT));
    SR(FRAME_CTRL_39) = LO(PANEL_WIDTH);
    SR(FRAME_CTRL_3A) = LO(PANEL_HEIGHT);
    osd_commit();
}

/* Row command list: nrows rows of ncols cells each, font A positioned at the
 * OSD origin.  Character cells for row r start at OSD_CHAR_BASE + r * OSD_MAX_COLS. */
void osd_rows_set(u8 nrows, u8 ncols)
{
    u8 r;
    if (nrows > OSD_MAX_ROWS) nrows = OSD_MAX_ROWS;
    if (ncols > OSD_MAX_COLS) ncols = OSD_MAX_COLS;
    for (r = 0; r < nrows; r++)
        osd_word(OSD_ROW_BASE + r, ROW_CMD, OSD_FONT_H - 1, ncols, OSD_SRAM_FONT);
    osd_word(OSD_ROW_BASE + nrows, 0, 0, 0, OSD_SRAM_FONT);        /* end */
    SR(FRAME_CTRL_24) = 0; SR(FRAME_CTRL_25) = 0; SR(FRAME_CTRL_26) = 0;   /* font A at the OSD origin */
    SR(FRAME_CTRL_3E) = nrows; SR(FRAME_CTRL_3F) = nrows;
    osd_commit();
}

void osd_row_write(u8 row, const __xdata char *s, u8 n, u8 fg, u8 bg)
{
    u16 a = OSD_CHAR_BASE + (u16)row * OSD_MAX_COLS;
    u8 c, attr = ((fg & 0x0F) << 4) | (bg & 0x0F);
    while (n--) {
        c = (u8)*s++;
        if (c < 0x20 || c >= ASSET_OSD_CHAR_END) c = '?';
        osd_word(a++, CHAR_CMD_1BPP | OSD_FONT_W, c - 0x20, attr, OSD_SRAM_FONT);
    }
}

/* Single row of text at pixel (x, y). */
void osd_text(u16 x, u16 y, const char *s, u8 fg, u8 bg)
{
    u8 n = 0, c;
    u16 a = OSD_CHAR_BASE;

    while (s[n] && n < OSD_MAX_COLS) {
        c = (u8)s[n];
        if (c < 0x20 || c >= ASSET_OSD_CHAR_END) c = '?';
        osd_word(a++, CHAR_CMD_1BPP | OSD_FONT_W, c - 0x20, ((fg & 0x0F) << 4) | (bg & 0x0F), OSD_SRAM_FONT);
        n++;
    }
    osd_word(OSD_ROW_BASE, ROW_CMD, OSD_FONT_H - 1, n, OSD_SRAM_FONT);
    osd_word(OSD_ROW_BASE + 1, 0, 0, 0, OSD_SRAM_FONT);
    SR(FRAME_CTRL_24) = ((HI(x) & 0x0F) << FRAME_CTRL_24_FONT_A_HORI_DELAY_SHIFT) | (HI(y) & 0x0F);
    SR(FRAME_CTRL_25) = LO(x);
    SR(FRAME_CTRL_26) = LO(y);
    SR(FRAME_CTRL_3E) = 1; SR(FRAME_CTRL_3F) = 1;
    osd_commit();
}

/* Boot logo: load the deduplicated logo glyphs into font SRAM, then one character
 * command per tile (glyph index + per-tile foreground color) across LOGO_ROWS x
 * LOGO_COLS cells, drawn at pixel (x, y).  Overwrites the text font in font SRAM. */
void osd_logo_show(u16 x, u16 y)
{
    u8 row;

    osd_burst(OSD_FONT_BASE, ASSET_LOGO_GLYPHS_OFF, ASSET_LOGO_GLYPHS_LEN);
    /* the per-cell character commands are precomputed by tools/gen_assets.py */
    osd_burst(OSD_CHAR_BASE, ASSET_LOGO_CMDS_OFF, ASSET_LOGO_CMDS_LEN);
    for (row = 0; row < LOGO_ROWS; row++)
        osd_word(OSD_ROW_BASE + row, ROW_CMD, OSD_FONT_H - 1, LOGO_COLS, OSD_SRAM_FONT);
    osd_word(OSD_ROW_BASE + LOGO_ROWS, 0, 0, 0, OSD_SRAM_FONT);      /* end */
    SR(FRAME_CTRL_24) = ((HI(x) & 0x0F) << FRAME_CTRL_24_FONT_A_HORI_DELAY_SHIFT) | (HI(y) & 0x0F);
    SR(FRAME_CTRL_25) = LO(x);
    SR(FRAME_CTRL_26) = LO(y);
    SR(FRAME_CTRL_3E) = LOGO_ROWS; SR(FRAME_CTRL_3F) = LOGO_ROWS;
    osd_commit();
}

void osd_on(void)
{
    /* Two-step: clear first, then set overlay_port_en.  Setting it straight after a
     * background-color write (which leaves bg_color_access_en set) does not stick.
     * Note the enable also refuses to set while the input watchdogs are armed - see
     * display_freerun(). */
    SR(OVERLAY_CTRL) = 0x00;
    SR(OVERLAY_CTRL) = OVERLAY_CTRL_OVERLAY_PORT_EN;
    SR_SET(FRAME_CTRL_02, FRAME_CTRL_02_OSD_EN);      /* double buffered */
    osd_commit();
}

void osd_off(void)
{
    SR(OSD_WRITE_OPTION) = 0x00;
    SR(OVERLAY_CTRL) = 0x00;
    SR_CLR(FRAME_CTRL_02, FRAME_CTRL_02_OSD_EN);
    osd_commit();
}
