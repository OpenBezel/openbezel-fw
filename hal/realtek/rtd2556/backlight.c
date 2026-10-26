/* Backlight dimming - see backlight.h. */
#include "backlight.h"
#include "rtd2556_regs.h"

/* PWM5 clock: M2PLL, 351 MHz (the same PLL display_clocks_init divides by 13
 * for the 27 MHz GDI clock).  f = 351 MHz / ((N+1) << M) / 256:
 *   M = 0, N = 3808  ->  351e6 / 3809 / 256 = 359.96 Hz */
#define BL_PWM_N 3808

void backlight_pwm_init(void)
{
    /* PWM_CK_SEL shares its register with WDT_EN_2 (bit 6), a watchdog enable
     * board_init clears deliberately - touch bit 5 only. */
    SR_SET(PWM_CK_SEL, PWM_CK_SEL_PWM5_CK_SEL);                     /* M2PLL */
    SR_CLR(PWM45_M, PWM45_M_PWM5_M);                                 /* first stage /1 */
    SR_WRF(PWM45_N_MSB, PWM45_N_MSB_PWM5H_N, HI(BL_PWM_N));
    SR(PWM5_N_LSB) = LO(BL_PWM_N);
    SR_SET(PWM_CK, PWM_CK_PWM5_CK);                                 /* second-stage output */
    SR_CLR(PWM_CK, PWM_CK_PWM5_REF_CTRL_EN);                        /* normal output */
    SR_SET(PWM_DUT_TYPE, PWM_DUT_TYPE_PWM5_DUT_8B);                 /* PWM5H_DUT alone is the duty */
    SR_CLR(PWM_CNT_MODE, PWM_CNT_MODE_PWM5_CNT_MODE);               /* duty mode */
    SR_CLR(PWML, PWML_PWM5L);                                       /* active high */
    SR_SET(PWM_VS_CTRL, PWM_VS_CTRL_PWM5_VS_RST_EN);                /* frame-locked (< 1 kHz) */
    SR(PWM5H_DUT) = 0x00;                                           /* inverted: 0 = full brightness */
    SR_SET(PWM_EN, PWM_EN_PWM5_EN);
}

/* Floor: a 0 % backlight darkens the panel INCLUDING the OSD needed to turn it
 * back up, and the menu's percentage items wrap 100 -> 0.  MCCS itself warns
 * that levels near zero "may be non-recoverable".  The on/off enable and the
 * powersave gate are the ways to turn it fully off. */
#define BL_MIN_PCT 5

void backlight_pwm_level(u8 pct)
{
    u16 lvl;

    if (pct > 100)
        pct = 100;
    if (pct < BL_MIN_PCT)
        pct = BL_MIN_PCT;
    lvl = ((u16)pct * 255U + 50U) / 100U;                           /* 0..255 */
    SR(PWM5H_DUT) = (u8)(0xFF - lvl);                               /* inverted */
}
