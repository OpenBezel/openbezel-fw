/* Dynamic contrast - see dcr.h. */
#include "dcr.h"
#include "settings.h"
#include "source.h"
#include "timebase.h"
#include "rtd2556_regs.h"

#define DCR_TICK_MS 50
#define DCR_FLOOR 40            /* % of the user's BL level for a black scene */
#define DCR_APL_FULL 128        /* average level (0..255) from which the level is full */
#define DCR_UP 3                /* % per tick back towards full: brightening follows fast */
#define DCR_DOWN 1              /* % per tick down: dimming creeps, so it is not seen */


static __xdata u16 d_t;
static __xdata u8 d_armed, d_target, d_i;
static __xdata u32 d_num, d_val;

static void arm(void)
{
    SR(DCR_ADDRESS_PORT) = DCR_THESHOLD1_1 << DCR_ADDRESS_PORT_DCR_ADDR_SHIFT;
    SR(DCR_DATA_PORT) = 0;                                  /* threshold 1 = 0: every pixel */
    SR(DCR_ADDRESS_PORT) = DCR_ADDRESS_PORT_MEASURE_START;
}

/* n bytes from index idx, MSB first, into d_val (the result held for reading) */
static void read_be(u8 idx, u8 n)
{
    d_val = 0;
    for (d_i = 0; d_i < n; d_i++) {
        SR(DCR_ADDRESS_PORT) = (u8)((idx + d_i) << DCR_ADDRESS_PORT_DCR_ADDR_SHIFT) | DCR_ADDRESS_PORT_RESULT_READ;
        d_val <<= 8;
        d_val |= SR(DCR_DATA_PORT);
    }
}

static void set_cut(u8 c)
{
    if (dcr_cut == c)
        return;
    dcr_cut = c;
    settings_apply_backlight();
}

void dcr_tick(u16 t0ovf) __banked
{
    if ((u16)(t0ovf - d_t) < tb_ms(DCR_TICK_MS))
        return;
    d_t = t0ovf;
    if (!ob_settings.dcr) {
        d_armed = 0;
        set_cut(0);
        return;
    }
    if (!source_picture()) {
        d_armed = 0;
        set_cut(0);
        return;
    }
    if (!d_armed) {
        arm();
        d_armed = 1;
        return;
    }
    read_be(DCR_ABOVE_TH1_NUM_2, 3);         /* 3 bytes, MSB first */
    d_num = d_val;
    read_be(DCR_ABOVE_TH1_VAL_4, 4);         /* 4 bytes, MSB first */
    arm();
    if (!d_num)
        return;
    d_val /= d_num;                     /* R+G+B, 0..765 */
    d_val /= 3;                         /* average picture level, 0..255 */
    if (d_val > DCR_APL_FULL)
        d_val = DCR_APL_FULL;
    d_val = DCR_APL_FULL - d_val;       /* darkness, 0..FULL */
    d_val *= 100 - DCR_FLOOR;
    d_val /= DCR_APL_FULL;
    d_target = (u8)d_val;               /* % to take off the BL level */
    d_i = dcr_cut;
    if (d_i < d_target) {
        d_i += DCR_DOWN;
        if (d_i > d_target)
            d_i = d_target;
    } else if (d_i > d_target) {
        if (d_i - d_target > DCR_UP)
            d_i -= DCR_UP;
        else
            d_i = d_target;
    }
    set_cut(d_i);
}
