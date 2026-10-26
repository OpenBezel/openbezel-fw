/* Busy-waits: nop loops whose length depends on the MCU clock.  Calibrated
 * for the PLL clock, where a "ms" is ~1.06 ms and a "us" ~1.04 us (never
 * shorter than asked); on the crystal clock, before mcu_clock_to_pll(), they
 * run ~6.4x long (a "ms" is ~6.8 ms).  For a measured interval use tb_now() /
 * tb_ms() (timebase.h). */
#ifndef RTD2556_DELAY_H
#define RTD2556_DELAY_H

#include "ob_types.h"

void delay_us(u16 us);
void delay_ms(u16 ms);           /* also services the timebase every ms */

#endif
