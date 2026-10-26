/* RTD2556 video pipeline: the stages that are fed from burst-loaded SRAM tables.
 *
 * Several blocks in this chip take their real configuration from an SRAM table written a byte
 * at a time through a port register, not from the registers a snapshot can see.  An unloaded
 * table is NOT a no-op - the scale-up filter with an unwritten coefficient SRAM behaves as a
 * high-pass and renders every flat area black.
 */
#ifndef RTD2556_PIPELINE_H
#define RTD2556_PIPELINE_H

#include "ob_types.h"

#define PIPELINE_SHARPNESS_MAX 4        /* scale-up filter presets 0 (softest) .. 4 (sharpest) */
#define PIPELINE_SHARPNESS_DEFAULT 0

/* Gamma presets: 0 = bypass (no LUT loaded),
 * 1..gamma_lut_count = the LUTs embedded by tools/gen_pipeline_tables.py.  The
 * LUTs live in the flash asset partition (ASSET_GAMMA_*), not in __code. */
#define PIPELINE_GAMMA_DEFAULT 0

/* Load every table the input pipeline needs.  Call with the I-domain double buffer
 * (I_DB_CTRL imd_db_en) clear, like the rest of the input-path programming.
 *   sharpness  0..PIPELINE_SHARPNESS_MAX
 *   bpc        bits per component of the incoming stream (8 or 10)
 *   dither     0 = leave the input dither block disabled, 1 = load its tables and enable */
void pipeline_tables_load(u8 sharpness, u8 bpc, u8 dither);

/* Individual stages, so a settings change can reload just what it affects. */
void pipeline_scaleup_coefs_load(u8 sharpness);   /* 0035/0036 - H and V filter coefficients */
void pipeline_scaledown_coefs_load(void);         /* 0026[7:6]/0027 - scale-down coefficients */
void pipeline_idither_load(u8 bpc, u8 enable);    /* 0089/0088 - input dither tables */
void pipeline_gamma_load(u8 preset);              /* 0067/0066 - gamma LUT; 0 = bypass */

/* Brightness, contrast and color temperature all live in the CTS/BRI block: a
 * 12-byte port space behind CTS_BRI_PORT_ADD/DATA (0064/0065), gated by
 * SRGB_CTRL bri_enable and cts_enable.  Layout (measured on hardware): 00-02 = brightness
 * R/G/B (offset), 03-05 = contrast R/G/B (gain), 06-0B a second bank that has
 * no effect here.  Neutral is 0x80 for every byte, which is also the power-on
 * default.
 *
 * Contrast and color temperature share the same three gain bytes, so they
 * MUST be written together - that is why this is one call rather than two.
 * Color temperature is a per-channel trim on top of the contrast gain, and
 * the trims are never positive: a cut cannot clip, a boost can.
 *
 * brightness / contrast are -50..+50 with 0 = neutral; ctemp is an index. */
#define PIPELINE_CB_RANGE 50
#define PIPELINE_CTEMP_COUNT 4
#define PIPELINE_CTEMP_COOL 0
#define PIPELINE_CTEMP_NORMAL 1
#define PIPELINE_CTEMP_WARM 2
#define PIPELINE_CTEMP_USER 3   /* the gains of pipeline_user_gains() */
/* User color temperature: per-channel gain 0..100, 100 = neutral, each step cuts
 * one port unit (never a boost, like the presets).  Takes effect on the next
 * pipeline_picture_levels(). */
void pipeline_user_gains(u8 r, u8 g, u8 b);
/* low_blue: 0 off, 1-3 = the blue-light cut, deeper each step */
void pipeline_picture_levels(s8 brightness, s8 contrast, u8 ctemp, u8 low_blue);

/* Saturation uses the "sRGB" block (0062/0063): a 3x3 matrix plus per-channel
 * offsets, 16-bit big-endian, 9-bit two's complement coefficients over an
 * ADDITIVE transform with CM_UNITY = 64.  See pipeline.c for the derivation -
 * all of it measured on hardware. It is a normalized color matrix, not a gain
 * stage, which is why contrast does not belong here.
 *
 * 0 = grayscale, PIPELINE_SAT_NORMAL = unchanged, up to PIPELINE_SAT_MAX =
 * roughly double.  Oversaturation is just the same formula with (1 - s)
 * negative (the diagonal term is already negative at every setting below
 * normal, so negative coefficients work).
 * The coefficients stay well inside the 9-bit range across the whole span
 * (max magnitude 50 at PIPELINE_SAT_MAX), and pipeline.c clamps anyway. */
#define PIPELINE_SAT_NORMAL 100
#define PIPELINE_SAT_MAX 200
void pipeline_saturation(u8 saturation);
void pipeline_color_bypass(void);                 /* srgb_enable = 0, the known-good default */

#endif /* RTD2556_PIPELINE_H */
