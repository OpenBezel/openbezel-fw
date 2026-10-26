/* Firmware timebase - see timebase.h. */
#include "timebase.h"
#include "rtd2556_regs.h"

static u16 ovf;

/* Overflows per second.  Timer0 counts SYSCLK/12 and overflows every 65536
 * counts: on the 14.318 MHz crystal (before mcu_clock_to_pll()) that is 55 ms,
 * 18.2 per second; on the PLL it is 116 per second (8.59 ms, ~91.5 MHz - measured
 * against the panel refresh).  Until a calibration is accepted the timebase
 * assumes 34: between the two, so an uncalibrated duration comes out longer than
 * asked on either clock, never shorter. */
#define TB_OVF_PER_SEC_XTAL 34
static u16 ovf_per_sec = TB_OVF_PER_SEC_XTAL;

void tb_init(void)
{
    TMOD = (TMOD & 0xF0) | 0x01;    /* Timer0 16-bit, free-running (no reload) */
    TR0 = 1;
    ovf = 0;
}

/* Deliberately services at most one overflow per call.  Callers spin far
 * faster than 29 ms, so a loop here would only ever run once - and if a caller
 * ever is slower than an overflow, a loop could not tell how many it missed
 * anyway.  Losing time is the honest failure; inventing it is not. */
void tb_service(void)
{
    if (TF0) {
        TF0 = 0;
        ovf++;
    }
}

u16 tb_now(void)
{
    tb_service();
    return ovf;
}

/* Plausibility band for a calibration, in overflows per second.  It is taken on
 * the PLL, where the figure is 116, so anything outside 20..400 is not a clock
 * this firmware can be running on - it is a broken measurement.
 * A measurement that feeds every timeout in the system has to be sanity-checked
 * before it is believed: a bogus figure would make every duration wildly short
 * (e.g. powersave blanking the panel right after boot). */
#define TB_PER_SEC_MIN 20
#define TB_PER_SEC_MAX 400

static u8 calibrated;

#ifdef CONFIG_DEBUG_CONSOLE
u8 tb_calibrated(void) { return calibrated; }
#endif

void tb_calibrate(u16 n, u16 ms)
{
    u16 per;

    if (n == 0 || ms == 0)
        return;                 /* measurement failed: keep the default */
    per = (u16)(((u32)n * 1000UL) / ms);
    if (per < TB_PER_SEC_MIN || per > TB_PER_SEC_MAX)
        return;                 /* implausible: keep the default, stay slow but sane */
    ovf_per_sec = per;
    calibrated = 1;
}

u16 tb_ms(u16 ms)
{
    u16 n = (u16)(((u32)ms * ovf_per_sec) / 1000UL);

    return (ms && !n) ? 1 : n;  /* never round a real delay down to nothing */
}

