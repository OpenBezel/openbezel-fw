/* InTouch 15.6" board definition: RTD2556, HDMI input, eDP 1920x1080 panel.
 */
#ifndef BOARD_INTOUCH156B_H
#define BOARD_INTOUCH156B_H

#include "rtd2556_regs.h"

/* The eDP panel's one mode (also its EDID detailed timing).  Everything that
 * programs the display side derives from these: the display timing generator
 * (display.c), the eDP stream / MSA and link fields (edp_tx.c) and the scaler's
 * output and frame sync (hdmi_rx.c). */
#define PANEL_WIDTH 1920
#define PANEL_HEIGHT 1080
#define PANEL_HTOTAL 2230               /* pixel clocks per line */
#define PANEL_HSTART 48                 /* sync + back porch */
#define PANEL_HSYNC 8
#define PANEL_VTOTAL 1125               /* lines per frame */
#define PANEL_VSTART 6
#define PANEL_VSYNC 2
#define PANEL_PCLK_KHZ 150516           /* 1080p60 line-locked: DISPLAY_DCLK_DEFAULT */
#define PANEL_BPC 6                     /* 18 bpp */
#define PANEL_EDP_LANES 2
#define PANEL_EDP_LINK_RATE 0x0A        /* DPCD code: 0x06 RBR, 0x0A HBR, 0x14 HBR2 */

/* GPIO pin registers (page 0xFE, one pin per byte from P4.0, bit0 = level). */
#define GPIO_PIN_BASE PORT40_PIN_REG
#define GPIO_PIN(off) SR(GPIO_PIN_BASE + (off))

#define BOARD_LED_GREEN_N SR(PORT77_PIN_REG)   /* P7.7, active low */
#define BOARD_LED_RED_N SR(PORT92_PIN_REG)     /* P9.2, active low */
#define BOARD_BACKLIGHT_OFF SR(PORT64_PIN_REG) /* P6.4: 1 = backlight off */
#define BOARD_FLASH_WP SR(PORT71_PIN_REG)      /* P7.1: 1 = WRSR allowed */
#define BOARD_PANEL_VCC SR(PORT62_PIN_REG)     /* P6.2: panel VCC enable */
#define BOARD_AUDIO_MUTE SR(PORT91_PIN_REG)    /* P9.1: audio amplifier/output mute, 1 = muted */

/* Status LED (main.c): RED = main() reached, or stopped on an error (bank
 * mismatch, link training failed); AMBER = presenting to the source / idle;
 * GREEN = panel link trained.  While running it blinks in that color, and in
 * powersave only green flashes - see led_show(). */
#define BOARD_LED_RED()   do { BOARD_LED_RED_N = 0; BOARD_LED_GREEN_N = 1; } while (0)
#define BOARD_LED_GREEN() do { BOARD_LED_RED_N = 1; BOARD_LED_GREEN_N = 0; } while (0)
#define BOARD_LED_AMBER() do { BOARD_LED_RED_N = 0; BOARD_LED_GREEN_N = 0; } while (0)
#define BOARD_LED_OFF()   do { BOARD_LED_RED_N = 1; BOARD_LED_GREEN_N = 1; } while (0)

/* Key ladder LSADC.
 * Channel A0 = XFR 0xFF09, A1 = 0xFF0A; idle 0xF8 on both.  Pressed-low is
 * well clear of everything; pressed-high sits just under idle (0xE8), so its
 * window is deliberately tight. */
#define KEY_ADC_IDLE_MIN 0xF0     /* >= this = no key */
#define KEY_ADC_SPLIT 0x40        /* < split = low key, else high key */
/* A0: low = PWR, high = ENTER;  A1: low = UP, high = DOWN */

/* Applies the pinshare table (board_init.c) - REQUIRED before any GPIO
 * write has an effect; the pinshare page is all zero at reset. */
void board_early_init(void);
/* Drives every GPIO output to its known-good level while the display runs
 * (page 0xFE), except the pins we manage ourselves (LEDs,
 * backlight, panel VCC, audio mute), and holds flash WP# low.  Covers any
 * panel-power / mux pin we have not identified individually. */
void board_gpio_init(void);

#endif
