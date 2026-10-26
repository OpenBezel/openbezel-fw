/* Menu strings - see lang.h. */
#include "lang.h"
#include "flash_rw.h"
#include "assets.h"
#include "settings.h"

#define LANG_FLASH (ASSET_BASE + ASSET_LANG_OFF)

/* + a whole 3-byte read past the limit; fixed above the linker's region */
static __xdata __at(XRAM_LANG_BUF) char lbuf[LANG_STR_MAX + 3];
typedef char lbuf_fits[(LANG_STR_MAX + 3 <= 64) ? 1 : -1];
static __xdata u8 loff[2];
static __xdata u32 la;
static __xdata u8 li;

const char *lang_str_in(u8 lang, u8 id)
{
    if (lang >= LANG_COUNT)
        lang = 0;
    la = (u16)lang * STR_COUNT;
    la += id;
    la <<= 1;
    la += LANG_FLASH + LANG_TAB;
    flash_read(la, loff, 2);
    la = loff[1];
    la <<= 8;
    la |= loff[0];
    la += LANG_FLASH;
    /* three bytes per flash command (flash_read's unit): stop at the one with the NUL */
    for (li = 0; li < LANG_STR_MAX; li += 3) {
        flash_read(la, (__xdata u8 *)&lbuf[li], 3);
        la += 3;
        if (!lbuf[li])
            break;
        if (!lbuf[li + 1])
            break;
        if (!lbuf[li + 2])
            break;
    }
    lbuf[LANG_STR_MAX] = 0;
    return lbuf;
}

const char *lang_str(u8 id)
{
    return lang_str_in(ob_settings.language, id);
}
