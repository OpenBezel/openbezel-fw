#include "console.h"
#include "osd.h"

#define CON_FG OSD_COL_WHITE
#define CON_BG 0   /* palette 0 renders transparent over the video/background */

/* Overlays that have to stay readable over arbitrary video (menu, popup) set an
 * opaque background and a row count, so they do not paint 8 rows of block. */
static u8 con_fg = CON_FG, con_bg = CON_BG;
static u8 con_rows = CON_ROWS;

static __xdata char con_buf[CON_ROWS][CON_COLS];
static u16 con_dirty;     /* bit per row */
static u8 con_enabled;

static const __code char hexdig[] = "0123456789ABCDEF";

void con_init(void)
{
    con_enabled = 0;
    con_clear();
}

const __xdata char *con_row(u8 row)
{
    return con_buf[row];
}

void con_clear(void)
{
    u8 r, c;
    for (r = 0; r < CON_ROWS; r++)
        for (c = 0; c < CON_COLS; c++)
            con_buf[r][c] = ' ';
    con_dirty = 0xFFFF;
}

void con_colors(u8 fg, u8 bg)
{
    con_fg = fg;
    con_bg = bg;
    con_dirty = 0xFFFF;
}

void con_enable(u16 x, u16 y)
{
    con_enable_rows(x, y, OSD_MAX_ROWS);
}

void con_enable_rows(u16 x, u16 y, u8 nrows)
{
    con_rows = nrows > OSD_MAX_ROWS ? OSD_MAX_ROWS : nrows;
    osd_origin(x, y);
    osd_rows_set(con_rows, CON_COLS);
    con_dirty = 0xFFFF;
    con_enabled = 1;
    con_flush();
    osd_on();
}

void con_disable(void)
{
    con_enabled = 0;
    osd_off();
}

void con_putc(u8 row, u8 col, char c)
{
    if (row >= CON_ROWS || col >= CON_COLS)
        return;
    if (con_buf[row][col] != c) {
        con_buf[row][col] = c;
        con_dirty |= (u16)1 << row;
    }
}

void con_puts(u8 row, u8 col, const char *s)
{
    while (*s && col < CON_COLS)
        con_putc(row, col++, *s++);
}

void con_hex8(u8 row, u8 col, u8 v)
{
    con_putc(row, col, hexdig[v >> 4]);
    con_putc(row, col + 1, hexdig[v & 0x0F]);
}

void con_hex16(u8 row, u8 col, u16 v)
{
    con_hex8(row, col, HI(v));
    con_hex8(row, col + 2, LO(v));
}

void con_dec(u8 row, u8 col, u16 v, u8 width)
{
    u8 i;
    for (i = width; i > 0; i--) {
        con_putc(row, col + i - 1, (v || i == width) ? (char)('0' + v % 10) : ' ');
        v /= 10;
    }
}

void con_flush(void)
{
    u8 r;
    if (!con_enabled || !con_dirty)
        return;
    for (r = 0; r < con_rows; r++)
        if (con_dirty & (1 << r))
            osd_row_write(r, con_buf[r], CON_COLS, con_fg, con_bg);
    con_dirty = 0;
    osd_commit();
}
