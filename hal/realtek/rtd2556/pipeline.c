/* RTD2556 video pipeline SRAM tables - see pipeline.h. */
#include "pipeline.h"
#include "rtd2556_regs.h"
#include "flash_assets.h"

/* pipeline_tables.c, generated into build/gen/ by tools/gen_pipeline_tables.py.
 * The gamma LUTs are NOT here - at 6156 bytes each they live in the flash asset
 * partition and are streamed by the burst engine. */
extern const __code s16 scaleup_h_side[5];
extern const __code u8 scaleup_coefs_v[5 * 128];
extern const __code u8 scaledown_coefs_b[64];
extern const __code u8 idither_table_8bpc[24];
extern const __code u8 idither_table_10bpc[24];
extern const __code u8 idither_seq_table[24];
extern const __code u8 idither_temporal_table[4];

/* Scale-down filter coefficient access (SD_FILTER_CONTROL_REG_M1 [7:6]), the analog of
 * FILTER_CONTROL_REG [7:6] for the scale-up filter: write enable, then the table. */
#define SD_FILTER_ACCESS_TABLE_A SD_FILTER_CONTROL_REG_M1_SD_MAIN_FCOEF_WR_EN
#define SD_FILTER_ACCESS_TABLE_B (SD_FILTER_CONTROL_REG_M1_SD_MAIN_FCOEF_WR_EN | SD_FILTER_CONTROL_REG_M1_SD_WCOEF_SEL2)

/* FILTER_CONTROL_REG [7:6]: coefficient write enable + table select.  The same two bits
 * are SD_FILTER_CONTROL_REG_M1's and the dither blocks' DITHER_ACCESS, so burst() uses
 * this for every sel_reg. */
#define FCOEF_ACCESS (FILTER_CONTROL_REG_MAIN_FCOEF_WR_EN | FILTER_CONTROL_REG_WCOEF_SEL2)

/* Stream `n` bytes into `port` with `sel_reg`'s top two bits set to `sel`, then clear them
 * again.  The port auto-increments internally, so a burst can go straight into it. */
static void burst(u16 sel_reg, u8 sel, u16 port, const __code u8 *data, u8 n)
{
    u8 i;

    SR(sel_reg) = (SR(sel_reg) & (u8)~FCOEF_ACCESS) | sel;
    for (i = 0; i < n; i++)
        SR(port) = data[i];
    SR_CLR(sel_reg, FCOEF_ACCESS);
}

void pipeline_scaleup_coefs_load(u8 sharpness)
{
    u16 off;

    if (sharpness > PIPELINE_SHARPNESS_MAX)
        sharpness = PIPELINE_SHARPNESS_DEFAULT;
    off = (u16)sharpness * 128;

    /* wcoef_sel2 = 0 selects the 1st table for access, = 1 the 2nd.  The 1st (H) is
     * a phase-invariant 3-tap kernel [0, c, 1024 - 2c, c]: 4 taps x 16 phases,
     * tap-major, 12-bit two's complement, little-endian. */
    {
        s16 c = scaleup_h_side[sharpness];
        u16 v;
        u8 k, ph;

        SR_WRF(FILTER_CONTROL_REG, FCOEF_ACCESS, FILTER_CONTROL_REG_MAIN_FCOEF_WR_EN);
        for (k = 0; k < 4; k++) {
            v = k == 0 ? 0 : (u16)(k == 2 ? 1024 - 2 * c : c);
            for (ph = 0; ph < 16; ph++) {
                SR(USER_DEF_FILTER_ACCESS_PORT) = LO(v);
                SR(USER_DEF_FILTER_ACCESS_PORT) = HI(v) & 0x0F;
            }
        }
        SR_CLR(FILTER_CONTROL_REG, FCOEF_ACCESS);
    }
    burst(FILTER_CONTROL_REG, FILTER_CONTROL_REG_MAIN_FCOEF_WR_EN | FILTER_CONTROL_REG_WCOEF_SEL2,
          USER_DEF_FILTER_ACCESS_PORT, &scaleup_coefs_v[off], 128);
    /* 0035 = 0x10: write access off, H reads the 1st table, V reads the 2nd */
    SR_WRF(FILTER_CONTROL_REG, FCOEF_ACCESS | FILTER_CONTROL_REG_HOR_RCOEF_SEL2 | FILTER_CONTROL_REG_VER_RCOEF_SEL2,
           FILTER_CONTROL_REG_VER_RCOEF_SEL2);
}

void pipeline_scaledown_coefs_load(void)
{
    /* Not in the path at a 1:1 ratio (SCALE_DOWN_CTRL_M1 hsd_en/vsd_en = 0), but it must be
     * loaded before scale-down is ever enabled: an unloaded filter SRAM blacks out flat areas. */
    /* table A: the sharpest scale-up V preset */
    burst(SD_FILTER_CONTROL_REG_M1, SD_FILTER_ACCESS_TABLE_A,
          SD_USER_FILTER_ACCESS_PORT_M1, &scaleup_coefs_v[PIPELINE_SHARPNESS_MAX * 128], 128);
    burst(SD_FILTER_CONTROL_REG_M1, SD_FILTER_ACCESS_TABLE_B,
          SD_USER_FILTER_ACCESS_PORT_M1, scaledown_coefs_b, 64);
}

void pipeline_idither_load(u8 bpc, u8 enable)
{
    /* No input dither table for 12/16 bpc - the block stays off. */
    if (!enable || (bpc != 8 && bpc != 10)) {
        SR_CLR(I_DITHER_COMMON_CTRL1_SETA, I_DITHER_COMMON_CTRL1_SETA_DITHER_ENABLE);
        return;
    }
    burst(I_DITHER_COMMON_CTRL1_SETA, DITHER_ACCESS_DITHER_TABLE, I_DITHER_DATA_ACCESS_SETA,
          bpc == 8 ? idither_table_8bpc : idither_table_10bpc, 24);
    burst(I_DITHER_COMMON_CTRL1_SETA, DITHER_ACCESS_SEQ_TABLE, I_DITHER_DATA_ACCESS_SETA,
          idither_seq_table, 24);
    burst(I_DITHER_COMMON_CTRL1_SETA, DITHER_ACCESS_TEMPORAL, I_DITHER_DATA_ACCESS_SETA,
          idither_temporal_table, 4);
    SR_SET(I_DITHER_COMMON_CTRL1_SETA, I_DITHER_COMMON_CTRL1_SETA_DITHER_ENABLE);
}

/* Gamma LUT: 3 channels of 0x804 bytes streamed through GAMMA_PORT_SETA (0066),
 * with GAMMA_CTRL_SETA (0067) selecting the channel and switching the port between
 * "address" and "data" mode.  Per channel set the channel with port_add_enable, write the 16-bit start address
 * through the port, then flip to access_port and stream.  Gamma is disabled across
 * the reload and re-enabled after, so a half-written LUT is never displayed. */
static void gamma_channel_begin(u8 ch)
{
    SR(GAMMA_CTRL_SETA) = (SR(GAMMA_CTRL_SETA) & (u8)~(GAMMA_CTRL_SETA_GAMMA_ACCESS_PORT | GAMMA_CTRL_SETA_GAMMA_CHANNEL_CTRL
                                                     | GAMMA_CTRL_SETA_GAMMA_SRAM_LATCH_SEL))
                        | (u8)(ch << GAMMA_CTRL_SETA_GAMMA_CHANNEL_CTRL_SHIFT);
    SR(GAMMA_CTRL_SETA) |= GAMMA_CTRL_SETA_GAMMA_PORT_ADD_ENABLE;
    SR(GAMMA_PORT_SETA) = 0;            /* start address, high byte */
    SR(GAMMA_PORT_SETA) = 0;            /* start address, low byte */
    SR_WRF(GAMMA_CTRL_SETA, GAMMA_CTRL_SETA_GAMMA_ACCESS_PORT | GAMMA_CTRL_SETA_GAMMA_PORT_ADD_ENABLE,
           GAMMA_CTRL_SETA_GAMMA_ACCESS_PORT);
}

void pipeline_gamma_load(u8 preset)
{
    static const __code u16 gamma_off[ASSET_GAMMA_COUNT] = {
        ASSET_GAMMA_1_OFF, ASSET_GAMMA_2_OFF, ASSET_GAMMA_3_OFF, ASSET_GAMMA_4_OFF
    };
    u16 base;
    u8 ch;

    SR_CLR(GAMMA_CTRL_SETA, GAMMA_CTRL_SETA_GAMMA_ENABLE);
    if (preset == 0 || preset > ASSET_GAMMA_COUNT)
        return;                         /* 0 = bypass: nothing to load */
    base = gamma_off[preset - 1];
    for (ch = 0; ch < 3; ch++) {
        gamma_channel_begin(ch);
        asset_burst_to_port((u16)(base + (u16)ch * ASSET_GAMMA_CH_LEN),
                            ASSET_GAMMA_CH_LEN, 0x00, (u8)GAMMA_PORT_SETA);
    }
    SR_CLR(GAMMA_CTRL_SETA, GAMMA_CTRL_SETA_GAMMA_ACCESS_PORT);
    SR_SET(GAMMA_CTRL_SETA, GAMMA_CTRL_SETA_GAMMA_ENABLE);
}


/* ---- brightness / contrast / color temperature (0064 ADD / 0065 DATA) ----
 *
 * Twelve read-write bytes, all 0x80 at power-on: 00-02 brightness R/G/B, 03-05
 * contrast R/G/B, 06-0B a second bank with no visible effect (measured).  Both functions are gated
 * by SRGB_CTRL bri_enable / cts_enable, which the input table already sets.
 *
 * Because 03-05 are per-channel, color temperature falls straight out of this
 * block: it is a relative trim between the three contrast gains.  That is also
 * why contrast and color temperature cannot be separate calls - they write
 * the same three bytes, so whichever ran last would win. */
#define CB_NEUTRAL 0x80
#define CB_GAIN_MIN 0x10        /* keep the gain away from the ends of the range */
#define CB_GAIN_MAX 0xF0

/* Per-channel gain trim, in port units.  Never positive: cutting a channel
 * cannot clip, boosting one can, and the eye reads "less blue" and "more red"
 * as the same shift.  Green is trimmed half as far so the overall brightness
 * barely moves between presets. */
static const __code s8 cb_ctemp[PIPELINE_CTEMP_USER][3] = {
    { -20,  -6,   0 },          /* Cool: cut red  -> bluer white */
    {   0,   0,   0 },          /* Normal: the gains contrast alone would set */
    {   0,  -6, -20 },          /* Warm: cut blue -> redder white */
};

/* Low blue light: blue cut by ~12 / 25 / 37 % of the neutral gain, green by a
 * fifth of that so white turns warm rather than yellow-green.  Applied on top of
 * the color temperature, in the same never-boost spirit. */
#define LOWBLUE_LEVELS 4
static const __code s8 cb_lowblue[LOWBLUE_LEVELS][2] = {        /* green, blue */
    { 0, 0 }, { -3, -16 }, { -6, -32 }, { -10, -48 },
};

static void cb_write3(u8 base, u8 v)
{
    u8 i;

    for (i = 0; i < 3; i++) {
        SR(CTS_BRI_PORT_ADD) = (u8)(base + i);
        SR(CTS_BRI_PORT_DATA) = v;
    }
}

static u8 cb_scale(s8 v)
{
    if (v > PIPELINE_CB_RANGE) v = PIPELINE_CB_RANGE;
    if (v < -PIPELINE_CB_RANGE) v = -PIPELINE_CB_RANGE;
    return (u8)(CB_NEUTRAL + (s16)v * 2);       /* +-50 -> 0x80 +- 100 */
}

static __xdata s16 lv_gain, lv_add;
static __xdata u8 cb_user[3];  /* set by settings_apply_levels() before any use */

void pipeline_user_gains(u8 r, u8 g, u8 b)
{
    cb_user[0] = r;
    cb_user[1] = g;
    cb_user[2] = b;
}

void pipeline_picture_levels(s8 brightness, s8 contrast, u8 ctemp, u8 low_blue)
{
    u8 i;

    if (ctemp >= PIPELINE_CTEMP_COUNT)
        ctemp = PIPELINE_CTEMP_NORMAL;
    if (low_blue >= LOWBLUE_LEVELS)
        low_blue = 0;

    cb_write3(CB_BRI_RED_COE_SETA_H, cb_scale(brightness));   /* R, G, B */

    /* contrast sets the common gain, color temperature trims each channel off
     * it; clamped because contrast already reaches 0x1C and the trim subtracts
     * from there. */
    for (i = 0; i < 3; i++) {
        lv_gain = cb_scale(contrast);
        if (ctemp == PIPELINE_CTEMP_USER) {
            lv_add = cb_user[i];
            lv_add -= 100;                  /* 100 -> 0, 0 -> -100 port units */
        } else {
            lv_add = cb_ctemp[ctemp][i];
        }
        lv_gain += lv_add;
        if (i) {
            lv_add = cb_lowblue[low_blue][i - 1];
            lv_gain += lv_add;
        }
        if (lv_gain < CB_GAIN_MIN) lv_gain = CB_GAIN_MIN;
        if (lv_gain > CB_GAIN_MAX) lv_gain = CB_GAIN_MAX;
        SR(CTS_BRI_PORT_ADD) = (u8)(CB_CTS_RED_COE_SETA_H + i);   /* R, G, B */
        SR(CTS_BRI_PORT_DATA) = (u8)lv_gain;
    }
    SR_SET(SRGB_CTRL, SRGB_CTRL_BRI_ENABLE | SRGB_CTRL_CTS_ENABLE);
}

/* ---- saturation: the 3x3 color matrix (0062/0063) -----------------------
 *
 * Coefficient order is fixed by the hardware and documented in SRGB_CTRL
 * srgb_coef_wr_enable: RR RG RB, GR GG GB, BR BG BB, Roff Goff Boff - twelve
 * 16-bit big-endian values.  Rows are built to sum to CM_UNITY so white stays white at every
 * saturation setting.
 *
 * Write sequence: clear srgb_wr_ready, pulse
 * srgb_port_reset, select auto-increment, stream 24 bytes, set srgb_wr_ready.
 * The block is double-buffered - the shadow only becomes active on the next
 * DVS rising edge, so the picture never shows a half-written matrix. */
/* THE BLOCK IS ADDITIVE (measured): an all-zero
 * matrix leaves the picture unchanged, so the transform is
 *
 *     out = in + (M / CM_UNITY) . in
 *
 * not out = M . in, with CM_UNITY = 64.
 *
 * Coefficients are 9-bit two's complement in bits [8:0]; bit 8 is the sign, so
 * the magnitude is 8 bits and the usable range is -256..+255.
 *
 * To apply a desired transform S you therefore write M = CM_UNITY * (S - I).
 * For saturation s, S = L + s(I - L) with L the luma matrix, which gives
 *
 *     M = CM_UNITY * (1 - s) * (L - I)
 *
 * and the useful invariant that **every row of M sums to zero** - an additive
 * correction that preserves white must add nothing to a neutral pixel.  That
 * is asserted by construction below: the off-diagonal terms are computed and
 * the diagonal takes the negated remainder.
 *
 * (GAMMA_BIST_DITHER_SETA srgb_preci documents this setting as "s2.7", unity
 * 128.  The measured unity is 64, and that is what the driver uses.) */
#define CM_PRECI FV(GAMMA_BIST_DITHER_SETA_SRGB_PRECI, 2)    /* as the input table sets it */
#define CM_UNITY 64
#define CM_MASK 0x01FF          /* 9-bit two's complement */
/* BT.709 luma weights x CM_UNITY: 0.2126, 0.7152, 0.0722 -> 14, 46, 4, summing
 * to exactly CM_UNITY so a fully desaturated row is exactly white-preserving. */
static const __code s16 cm_luma[3] = { 14, 46, 4 };

static void srgb_put16(s16 v)
{
    u16 c;

    /* 9-bit two's complement holds -256..+255.  Nothing generated below comes
     * near that, but a silent wrap here would show up as a color bug rather
     * than a range bug, so clamp instead of just masking. */
    if (v > 255) v = 255;
    if (v < -256) v = -256;
    c = (u16)v & CM_MASK;

    SR(SRGB_ACCESS_PORT_SETA) = HI(c);
    SR(SRGB_ACCESS_PORT_SETA) = LO(c);
}

void pipeline_color_bypass(void)
{
    SR_CLR(SRGB_CTRL, SRGB_CTRL_SRGB_ENABLE);
}

void pipeline_saturation(u8 saturation)
{
    s16 k;
    u8 i, j, guard;

    if (saturation == PIPELINE_SAT_NORMAL) {     /* normal: nothing to correct */
        pipeline_color_bypass();
        return;
    }
    if (saturation > PIPELINE_SAT_MAX)
        saturation = PIPELINE_SAT_MAX;
    /* (1 - s) scaled by PIPELINE_SAT_NORMAL: +100 at grayscale, 0 at normal,
     * -100 at PIPELINE_SAT_MAX.  Above normal it simply goes negative, which
     * is the whole of oversaturation - the matrix then subtracts luma instead
     * of adding it, and rows still sum to zero so white stays white. */
    k = (s16)((s16)PIPELINE_SAT_NORMAL - (s16)saturation);
    SR_WRF(GAMMA_BIST_DITHER_SETA, GAMMA_BIST_DITHER_SETA_SRGB_PRECI, CM_PRECI);
    SR_CLR(SRGB_CTRL, SRGB_CTRL_SRGB_WR_READY);
    SR(SRGB_CTRL) |= SRGB_CTRL_SRGB_PORT_RESET;
    for (guard = 0; guard < 200 && (SR(SRGB_CTRL) & SRGB_CTRL_SRGB_PORT_RESET); guard++)
        ;                                        /* the reset bit auto-clears */
    SR_CLR(SRGB_CTRL, SRGB_CTRL_SRGB_COEF_WR_ENABLE);   /* 0 = auto-increase */

    /* M = CM_UNITY * (1 - s) * (L - I).  Off-diagonals first, then the diagonal
     * takes the negated remainder so each row sums to exactly zero even after
     * integer truncation - a row that does not sum to zero tints neutrals. */
    for (i = 0; i < 3; i++) {
        s16 row[3];
        s16 rest = 0;

        for (j = 0; j < 3; j++) {
            if (j == i)
                continue;
            row[j] = (s16)(((s32)cm_luma[j] * k) / PIPELINE_SAT_NORMAL);
            rest = (s16)(rest + row[j]);
        }
        row[i] = (s16)(-rest);
        for (j = 0; j < 3; j++)
            srgb_put16(row[j]);
    }
    for (i = 0; i < 3; i++)
        srgb_put16(0);                           /* offsets: brightness is the CTS/BRI block's job */

    SR_SET(SRGB_CTRL, SRGB_CTRL_SRGB_WR_READY);
    SR_SET(SRGB_CTRL, SRGB_CTRL_SRGB_ENABLE);
}

void pipeline_tables_load(u8 sharpness, u8 bpc, u8 dither)
{
    pipeline_scaleup_coefs_load(sharpness);
    pipeline_scaledown_coefs_load();
    pipeline_idither_load(bpc, dither);
}
