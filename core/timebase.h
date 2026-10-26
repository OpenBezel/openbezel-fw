/* One timebase for the whole firmware: Timer0 overflows since boot.
 *
 * Timer0 is free-running 16-bit off SYSCLK/12, so an overflow is 65536*12
 * SYSCLK cycles - 55 ms on the 14.318 MHz crystal, 8.59 ms on the PLL.  Every
 * timed thing in the firmware (popup duration, source state machine tick,
 * powersave delay, LED blink) counts these.
 *
 * TWO THINGS TO KNOW:
 *
 * 1. The period is NOT constant across boot.  Timer0 clocks from the MCU
 *    clock, and mcu_clock_to_pll() switches that from the crystal to the PLL
 *    partway through bring-up, so overflows get much shorter from that point
 *    on.  Anything expressed in overflows therefore means a different wall
 *    time before and after the switch.
 *
 * 2. Only one place may consume TF0.  It is a single hardware flag; if the
 *    main loop and a busy-wait both test-and-clear it, whichever runs second
 *    misses the overflow and the clock loses time.  That is what tb_service()
 *    is for - call it from anywhere that spins, and never touch TF0 directly.
 */
#ifndef OB_TIMEBASE_H
#define OB_TIMEBASE_H

#include "ob_types.h"

void tb_init(void);         /* Timer0 mode 1, free-running, counter zeroed */
void tb_service(void);      /* poll TF0 once; cheap, safe to call in a tight loop */
u16 tb_now(void);           /* overflows since tb_init(), wrapping */

/* Teach the timebase what an overflow is actually worth.  `ovf` overflows were
 * measured across `ms` milliseconds of real time - see display_frame_ticks(),
 * which counts them against the panel refresh.  Until this is called 34 per
 * second is assumed: between the crystal's 18.2 and the PLL's 116, so a duration
 * comes out longer than asked on either clock, never shorter. */
void tb_calibrate(u16 ovf, u16 ms);

/* 1 if a calibration was accepted.  A rejected one leaves the default, which
 * makes every duration on the PLL about 3.4x too long rather than wildly wrong -
 * the safe direction.  Shown on the debug console so it is never silent. */
u8 tb_calibrated(void);

/* Milliseconds -> overflows, at the current calibration.  Express durations in
 * ms and convert through this; a constant in overflows means a different wall
 * time before and after the PLL switch. */
u16 tb_ms(u16 ms);

#endif /* OB_TIMEBASE_H */
