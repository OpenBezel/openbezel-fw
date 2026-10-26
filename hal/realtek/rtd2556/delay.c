#include "delay.h"
#include "timebase.h"

/* One spin() iteration (4 nops and the XDATA countdown) is ~1.515 us on the
 * PLL MCU clock and ~9.7 us on the crystal - measured against Timer0 from the
 * boot marks (main.c).  The counts below are for the PLL clock, with ~4-6 %
 * margin over that measurement's error so a delay is never short. */
#define SPIN_PER_MS 700         /* ~1.06 ms */

static void spin(u16 n)
{
    while (n--) {
        __asm__("nop");
        __asm__("nop");
        __asm__("nop");
        __asm__("nop");
    }
}

/* us x 11/16 iterations: ~1.04 us each */
void delay_us(u16 us)
{
    spin(us - (us >> 2) - (us >> 4) + 1);
}

/* Services the timebase once per millisecond.  Without this the boot spends
 * seconds inside busy-waits with nobody consuming TF0, so every overflow in
 * that window is lost and the clock reads far too low.  One overflow is
 * 8.6 ms on the PLL clock (55 ms on the crystal), well over one unit here,
 * so nothing is missed. */
void delay_ms(u16 ms)
{
    while (ms--) {
        spin(SPIN_PER_MS);
        tb_service();
    }
}
