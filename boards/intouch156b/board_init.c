/* intouch156b early board init.
 *
 * The pinshare page (scaler page 0x10, XDATA 0x1000-0x10FF) is ALL ZERO at
 * reset: no pin is routed to its function, so GPIO writes (LED, backlight,
 * panel power) do nothing until it is programmed.
 */
#include "ob_types.h"
#include "board.h"
#include "rtd2556_regs.h"

/* Pin functions (pinshare page 0x10).  Each PIN_SHARE_CTRLpn register picks what port
 * pin Pp.n does, with codes that vary per pin.
 */
#define PS(reg) ((u8)((reg) - PIN_SHARE_CTRL10))
static const __code struct { u8 off; u8 val; } pinshare_init[] = {
    /* port 1 */
    { PS(PIN_SHARE_CTRL11), PIN_SHARE_CTRL11_P1D1_GPO_OD },                             /* P1.1 (pin 84): HDMI hot-plug (HPD) */
    { PS(PIN_SHARE_CTRL14), PIN_SHARE_CTRL14_P1D4_GPO_PP },                             /* P1.4 (pin 119): USB-C (DP) hot-plug, dp_hpd() */
    /* port 3: AUX pads (HDMI DDC on 137/138), the key ladders */
    { PS(PIN_SHARE_CTRL30), PIN_SHARE_CTRL30_AUXP0_AUXP0 },                             /* P3.0 (pin 135): likely the USB-C DP AUX */
    { PS(PIN_SHARE_CTRL31), PIN_SHARE_CTRL31_AUXN0_AUXN0 },                             /* P3.1 (pin 136): likely the USB-C DP AUX */
    { PS(PIN_SHARE_CTRL32), PIN_SHARE_CTRL32_DDC1 | PIN_SHARE_CTRL32_AUXP1_AUXP1 },     /* P3.2 (pin 137): HDMI DDC SCL */
    { PS(PIN_SHARE_CTRL33), PIN_SHARE_CTRL33_AUXN1_AUXN1 },                             /* P3.3 (pin 138): HDMI DDC SDA */
    { PS(PIN_SHARE_CTRL34), PIN_SHARE_CTRL34_P3D4_A_ADC0_0 },                           /* P3.4 (pin 143): key ladder A (LSADC ch0: POWER, ENTER) */
    { PS(PIN_SHARE_CTRL35), PIN_SHARE_CTRL35_P3D5_A_ADC1_0 },                           /* P3.5 (pin 144): key ladder B (LSADC ch1: UP, DOWN) */
    /* port 4: audio */
    { PS(PIN_SHARE_CTRL42), PIN_SHARE_CTRL42_P4D2_AUDIO_HOUTL },                        /* P4.2 (pin 147): headphone L */
    { PS(PIN_SHARE_CTRL43), PIN_SHARE_CTRL43_P4D3_AUDIO_HOUTR },                        /* P4.3 (pin 148): headphone R */
    { PS(PIN_SHARE_CTRL46), PIN_SHARE_CTRL46_P4D6_AUDIO_REF },                          /* P4.6 (pin 153) */
    { PS(PIN_SHARE_CTRL47), PIN_SHARE_CTRL47_P4D7_GPO_PP },                             /* P4.7 (pin 154) */
    /* port 5: the eDP TX's AUX and HPD */
    { PS(PIN_SHARE_CTRL50), PIN_SHARE_CTRL50_P5D0_GPO_PP },                             /* P5.0 (pin 155) */
    { PS(PIN_SHARE_CTRL51), PIN_SHARE_CTRL51_P5D1_DPTX_AUX_CH_P_1 },                    /* P5.1 (pin 51): eDP AUX */
    { PS(PIN_SHARE_CTRL52), PIN_SHARE_CTRL52_P5D2_DPTX_AUX_CH_N_1 },                    /* P5.2 (pin 52): eDP AUX */
    { PS(PIN_SHARE_CTRL53), PIN_SHARE_CTRL53_P5D3_DPTX_HPD1_ORG },                      /* P5.3 (pin 50): eDP HPD (input) */
    /* port 6: panel power, backlight */
    { PS(PIN_SHARE_CTRL62), PIN_SHARE_CTRL62_P6D2_GPO_PP },                             /* P6.2 (pin 47): panel VCC (BOARD_PANEL_VCC) */
    { PS(PIN_SHARE_CTRL64), PIN_SHARE_CTRL64_P6D4_GPO_PP },                             /* P6.4 (pin 49): backlight off (BOARD_BACKLIGHT_OFF) */
    { PS(PIN_SHARE_CTRL65), PIN_SHARE_CTRL65_P6D5_PWM5_PP },                            /* P6.5 (pin 54): backlight dimming (backlight.c) */
    /* port 7: flash WP#, green LED */
    { PS(PIN_SHARE_CTRL71), PIN_SHARE_CTRL71_P7D1_GPO_PP },                             /* P7.1 (pin 61): flash WP# (BOARD_FLASH_WP) */
    { PS(PIN_SHARE_CTRL72), PIN_SHARE_CTRL72_P7D2_GPO_PP },                             /* P7.2 (pin 63) */
    { PS(PIN_SHARE_CTRL74), PIN_SHARE_CTRL74_P7D4_GPO_OD },                             /* P7.4 (pin 64) */
    { PS(PIN_SHARE_CTRL77), PIN_SHARE_CTRL77_P7D7_GPO_PP },                             /* P7.7 (pin 66): green LED (BOARD_LED_GREEN_N) */
    /* port 8 */
    { PS(PIN_SHARE_CTRL80), PIN_SHARE_CTRL80_IICSCL_2_GPO_PP },                         /* P8.0 (pin 67): GPIO, not I2C_2 SCL */
    { PS(PIN_SHARE_CTRL81), PIN_SHARE_CTRL81_IICSDA_2_GPO_PP },                         /* P8.1 (pin 68): GPIO, not I2C_2 SDA */
    { PS(PIN_SHARE_CTRL82), PIN_SHARE_CTRL82_P8D2_GPO_PP },                             /* P8.2 (pin 70) */
    { PS(PIN_SHARE_CTRL83), PIN_SHARE_CTRL83_P8D3_GPO_PP },                             /* P8.3 (pin 69) */
    /* port 9: audio mute, red LED, DDC2 pads */
    { PS(PIN_SHARE_CTRL91), PIN_SHARE_CTRL91_P9D1_GPO_PP },                             /* P9.1 (pin 76): audio mute (BOARD_AUDIO_MUTE) */
    { PS(PIN_SHARE_CTRL92), PIN_SHARE_CTRL92_EEI2CSCL_GPO_PP },                         /* P9.2 (pin 77): red LED (BOARD_LED_RED_N; not EEPROM I2C) */
    { PS(PIN_SHARE_CTRL94), PIN_SHARE_CTRL94_DDC2 | PIN_SHARE_CTRL94_DDCSCL2_GPO_OD },  /* P9.4 (pin 79): GPIO, not DDC2 SCL */
    { PS(PIN_SHARE_CTRL95), PIN_SHARE_CTRL95_DDCSDAS2_GPO_OD },                         /* P9.5 (pin 80): GPIO, not DDC2 SDA */
    /* port A */
    { PS(PIN_SHARE_CTRLA0), PIN_SHARE_CTRLA0_PAD0_GPO_PP },                             /* PA.0 (pin 81) */
    /* input Schmitt triggers on the DDC / I2C / HPD pins */
    { PS(PIN_SCHMITT_CTRL0), PIN_SCHMITT_CTRL0_DDC1_SCHMITT_EN | PIN_SCHMITT_CTRL0_DDC2_SCHMITT_EN |
                             PIN_SCHMITT_CTRL0_DDC3_SCHMITT_EN | PIN_SCHMITT_CTRL0_DDCVGA_SCHMITT_EN |
                             PIN_SCHMITT_CTRL0_EEI2C_SCHMITT_EN | PIN_SCHMITT_CTRL0_I2C_2_SCHMITT_EN |
                             PIN_SCHMITT_CTRL0_DPTX_HPD_SCHMITT_EN },
    { PS(PIN_SCHMITT_CTRL1), PIN_SCHMITT_CTRL1_DPTX_HPD1_SCHMITT_EN | PIN_SCHMITT_CTRL1_I2C_1_SCHMITT_EN },
    /* drive strength: 12 mA on the DDC pins (DDCVGA, DDC1 / USB-C AUX, DDC2 = HDMI DDC, DDC3),
     * 4 mA (the default) on the SPI flash pins */
    { PS(PIN_DRIVING_CTRL00), PIN_DRIVING_CTRL00_PIN66_67_DRIVE | PIN_DRIVING_CTRL00_PIN73_74_DRIVE },
    { PS(PIN_DRIVING_CTRL02), PIN_DRIVING_CTRL02_PIN185_DRIVE_4MA | PIN_DRIVING_CTRL02_PIN186_188_DRIVE_4MA },
    { PS(PIN_DRIVING_CTRL03), PIN_DRIVING_CTRL03_PIN212_213_DRIVE | PIN_DRIVING_CTRL03_PIN214_215_DRIVE },
};

/* XFR registers controlling the XDATA memory map: BANK_SWITCH_CONTROL 0xFFFC,
 * XDATA_BANK_START 0xFFFD, XDATA_BANK_SEL 0xFFFE.
 *
 * BANK_SWITCH_CONTROL bits:
 *   [4] scalar_addr_remapping - map scaler pages into XDATA 0x0000-0xFEFF
 *   [3] global_xfr            - XFR occupies 0xFF00-0xFFFF of every bank
 *   [2] global_xram           - XRAM (our data at 0xD700-0xF6FF) is global,
 *                               so remapping does NOT shadow it
 *   [1] sw_mode, [0] bank_en  - program-code banking through CODE_PAGE
 *                               (XDATA 0xFFFF; sw_mode 0 would take A16
 *                               from P3.5).  Required for banked code:
 *                               without them CODE_PAGE
 *                               writes do nothing.  Safe to set here:
 *                               main() has already written CODE_PAGE = 0,
 *                               the bank we run in.
 */
#define BSC_XDATA_REMAP (BANK_SWITCH_CONTROL_SCALAR_ADDR_REMAPPING | \
                         BANK_SWITCH_CONTROL_GLOBAL_XFR | BANK_SWITCH_CONTROL_GLOBAL_XRAM | \
                         BANK_SWITCH_CONTROL_SW_MODE | BANK_SWITCH_CONTROL_BANK_EN)  /* 0x1F */

void board_early_init(void)
{
    u8 i;

    /* Clock bring-up - an exact sequence; DO NOT reorder or "simplify":
     * a wrong clock-select write can stop the CPU and the SPI flash controller.
     * Takes the MCU/flash clocks from slow reset defaults to full speed. */
    SR_SET(WATCHDOG_TIMER, (WATCHDOG_TIMER_CLR_WDT | WATCHDOG_TIMER_CNT1));
    SR_CLR(MCU_CLK_CONTROL_1, MCU_CLK_CONTROL_1_MCU_CLK_SEL_EPLL);     /* CPU clock source M2PLL */
    SR_CLR(MCU_CONTROL, MCU_CONTROL_MCU_CLK_SEL);                      /* CPU clk: crystal path */
    SR_WRF(MCU_CONTROL, MCU_CONTROL_FLASH_CLK_DIV, FV(MCU_CONTROL_FLASH_CLK_DIV, 1));
    SR_WRF(MCU_CLK_CONTROL_1, MCU_CLK_CONTROL_1_FLASH_CLK_DIV2, FV(MCU_CLK_CONTROL_1_FLASH_CLK_DIV2, 1));
    SR_WRF(MCU_CLOCK_CONTROL, MCU_CLOCK_CONTROL_MCU_CLK_DIV, FV(MCU_CLOCK_CONTROL_MCU_CLK_DIV, 1));

    /* Watchdog off: WATCHDOG_TIMER[7] wdt_en, MCU_CLK_CONTROL_1[7] wdt_en_3,
     * PWM_CK_SEL[6] wdt_en_2 each independently arm it.  wdt_en_3 is set
     * at power-up (main() uses that to detect a warm restart), so anything
     * slow before this point reset-loops the chip.  Off from here on: a hung
     * loop must not be silently reset. */
    SR_CLR(WATCHDOG_TIMER, WATCHDOG_TIMER_WDT_EN);
    SR_CLR(MCU_CLK_CONTROL_1, MCU_CLK_CONTROL_1_WDT_EN_3);
    SR_CLR(PWM_CK_SEL, PWM_CK_SEL_WDT_EN_2);

    /* mcu_peri_non_stop: keep peripherals (DDC slave, indirect-port
     * engine, AUX PHY clocks) running while the MCU is stalled or
     * halted - the host's write path through the indirect port stalls the
     * MCU per access, and ISP mode halts it. */
    SR_SET(MCU_CLOCK_CONTROL, MCU_CLOCK_CONTROL_MCU_PERI_NON_STOP);

    /* Before any pinshare / GPIO access: until scalar_addr_remapping is on,
     * only the common page (0x0000-0x009E) is visible in XDATA, so writes to
     * the pinshare page (0x10xx) and GPIO (0xFExx) would go nowhere. */
    SR(XDATA_BANK_START) = 0x00;
    SR(XDATA_BANK_SEL) = 0x00;
    SR(BANK_SWITCH_CONTROL) = BSC_XDATA_REMAP;

    /* Cache enable: without it every code fetch stalls on SPI flash,
     * slowing XDATA-heavy loops ~1000x. */
    SR_SET(CACHE_OPTION, CACHE_OPTION_CACHE_RESET);
    SR_CLR(CACHE_OPTION, CACHE_OPTION_CACHE_RESET);
    SR_CLR(CACHE_OPTION, CACHE_OPTION_COMMON_PAGE_EN);
    SR_SET(CACHE_FUNCTION, CACHE_FUNCTION_CACHE_EN);

    /* Assign pins to their functions/GPIO. This also sets up pin properties like drive strength. */
    for (i = 0; i < sizeof(pinshare_init) / sizeof(pinshare_init[0]); i++)
        SR(PIN_SHARE_CTRL10 + pinshare_init[i].off) = pinshare_init[i].val;
}

/* GPIO output levels (page 0xFE: one PORTpn_PIN_REG per port pin, bit 0 = level).
 * Every pin register resets to 1 (high), so only the low ones are written.  Not
 * listed: the pins board.h manages (backlight 0xFE14, WP# 0xFE19, LEDs 0xFE1F/0xFE2A,
 * panel VCC 0xFE12, audio mute 0xFE29). */
#define G(reg) ((u8)((reg) - GPIO_PIN_BASE))
static const __code u8 gpio_low[] = {
    G(PORT42_PIN_REG), G(PORT43_PIN_REG),                     /* P4.2/4.3 (147/148): headphone */
    G(PORT46_PIN_REG), G(PORT47_PIN_REG),                     /* P4.6/4.7 (153/154) */
    G(PORT50_PIN_REG), G(PORT51_PIN_REG), G(PORT52_PIN_REG),  /* P5.0 (155); P5.1/5.2 (51/52): eDP AUX */
    G(PORT65_PIN_REG),                                        /* P6.5 (54): backlight PWM5 */
    G(PORT82_PIN_REG), G(PORT86_PIN_REG),                     /* P8.2 (70), P8.6 (73) */
    G(PORT94_PIN_REG), G(PORT95_PIN_REG),                     /* P9.4/9.5 (79/80): DDC pads */
    G(PORTA3_PIN_REG), G(PORTA4_PIN_REG),                     /* PA.3/PA.4: no pinshare fields, pin unknown */
};

void board_gpio_init(void)
{
    u8 i;
    for (i = 0; i < sizeof(gpio_low); i++)
        GPIO_PIN(gpio_low[i]) = 0;
    /* Flash WP# low: with SRP0 set (SR1 = 0xB4) that
     * hardware-locks the status register, so nothing running here can change
     * the protection.  rtdflash.py drives it itself (it halts the 8051
     * first). */
    BOARD_FLASH_WP = 0;
}
