/* osdsim, RTD2556: the OSD HAL (hal/realtek/rtd2556/osd.h) as a recorder.
 *
 * The firmware's own menu/GUI code runs unchanged on top of these; instead of
 * the OSD port they write the state the chip renderer (osdsim/chips/rtd2556.py)
 * reads back: font SRAM, window SRAM, the color LUT, the origin and the
 * frame-control registers (those land in osdsim_sr through SR()). */
#include <string.h>
#include "osd.h"
#include "assets.h"
#include "rtd2556_regs.h"
#include "osdsim_api.h"

#define FONT_WORDS 0x2800
#define WIN_WORDS 0x0400

u8 osdsim_font[FONT_WORDS * 3];
u8 osdsim_win[WIN_WORDS * 3];
u8 osdsim_lut[64 * 3];
u16 osdsim_origin_x, osdsim_origin_y;
u8 osdsim_osd_on;

extern const u8 *osdsim_assets;     /* the asset partition image (sim_api.c) */
extern u32 osdsim_assets_len;

static void word(u16 addr, u8 b0, u8 b1, u8 b2, u8 sram)
{
    u8 *p;

    if (sram == OSD_SRAM_FONT) {
        if (addr >= FONT_WORDS)
            return;
        p = &osdsim_font[addr * 3];
    } else {
        if (addr >= WIN_WORDS)
            return;
        p = &osdsim_win[addr * 3];
    }
    p[0] = b0;
    p[1] = b1;
    p[2] = b2;
}

void osd_sram_word(u16 addr, u8 b0, u8 b1, u8 b2, u8 sram) { word(addr, b0, b1, b2, sram); }

void osd_lut_set(u8 i, u8 r, u8 g, u8 b)
{
    i &= 0x3F;
    osdsim_lut[i * 3] = r;
    osdsim_lut[i * 3 + 1] = g;
    osdsim_lut[i * 3 + 2] = b;
}

void osd_commit(void) { }

void osd_origin(u16 x, u16 y)
{
    osdsim_origin_x = x;
    osdsim_origin_y = y;
}

void osd_double(u8 on)
{
    SR(FRAME_CTRL_0A) = (SR(FRAME_CTRL_0A) & (u8)~(FRAME_CTRL_0A_DB_WIDTH | FRAME_CTRL_0A_DB_HEIGHT))
                      | (on ? (FRAME_CTRL_0A_DB_WIDTH | FRAME_CTRL_0A_DB_HEIGHT) : 0);
}

/* the burst engine: asset bytes, 3 per word, into font SRAM from addr */
void osd_load_asset(u16 addr, u16 off, u16 len)
{
    u32 i;

    if ((u32)off + len > osdsim_assets_len)
        return;
    for (i = 0; i + 2 < len; i += 3)
        word((u16)(addr + i / 3), osdsim_assets[off + i], osdsim_assets[off + i + 1],
             osdsim_assets[off + i + 2], OSD_SRAM_FONT);
}

void osd_load_text_font(void) { osd_load_asset(OSD_FONT_BASE, ASSET_OSD_FONT_OFF, ASSET_OSD_FONT_LEN); }

void osd_on(void)
{
    SR(OVERLAY_CTRL) = OVERLAY_CTRL_OVERLAY_PORT_EN;
    SR_SET(FRAME_CTRL_02, FRAME_CTRL_02_OSD_EN);
    osdsim_osd_on = 1;
}

void osd_off(void)
{
    SR(OVERLAY_CTRL) = 0x00;
    SR_CLR(FRAME_CTRL_02, FRAME_CTRL_02_OSD_EN);
    osdsim_osd_on = 0;
}

/* osd_init's state, as the boot leaves it for the menu: bases, disp_zone left,
 * windows and rows clear, the firmware's three fixed colors */
void osd_init(void)
{
    memset(osdsim_font, 0, sizeof(osdsim_font));
    memset(osdsim_win, 0, sizeof(osdsim_win));
    memset(osdsim_lut, 0, sizeof(osdsim_lut));
    osd_lut_set(OSD_COL_ORANGE, 0xFF, 0xB0, 0x20);
    osd_lut_set(OSD_COL_BG_DARK, 0x10, 0x10, 0x18);
    osd_lut_set(OSD_COL_WHITE, 0xFF, 0xFF, 0xFF);
    SR(FRAME_CTRL_02) = 1 << FRAME_CTRL_02_DISP_ZONE_SHIFT;
    SR(FRAME_CTRL_0A) = 0x04;
    osd_origin(0, 0);
    osd_load_text_font();
    osdsim_osd_on = 0;
}

/* the debug console's row writer: plain 1-bit 12-px cells */
void osd_rows_set(u8 nrows, u8 ncols)
{
    u8 r;

    for (r = 0; r < nrows; r++)
        word(OSD_ROW_BASE + r, 0x80, OSD_FONT_H - 1, ncols, OSD_SRAM_FONT);
    word(OSD_ROW_BASE + nrows, 0, 0, 0, OSD_SRAM_FONT);
    SR(FRAME_CTRL_3E) = nrows;
    SR(FRAME_CTRL_3F) = nrows;
}

void osd_row_write(u8 row, const char *s, u8 n, u8 fg, u8 bg)
{
    u16 a = OSD_CHAR_BASE + (u16)row * OSD_MAX_COLS;
    u8 c;

    while (n--) {
        c = (u8)*s++;
        if (c < 0x20 || c >= ASSET_OSD_CHAR_END)
            c = '?';
        word(a++, 0x40 | OSD_FONT_W, c - 0x20, (u8)((fg & 0x0F) << 4) | (bg & 0x0F), OSD_SRAM_FONT);
    }
}

void osd_text(u16 x, u16 y, const char *s, u8 fg, u8 bg) { (void)x; (void)y; (void)s; (void)fg; (void)bg; }
void osd_logo_show(u16 x, u16 y) { (void)x; (void)y; }
