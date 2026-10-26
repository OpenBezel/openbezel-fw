/* LSADC front-panel key decode - see keys.h. */
#include "keys.h"
#include "board.h"
#include "rtd2556_regs.h"
#include "timebase.h"

#define DEBOUNCE_PRESS 3        /* consecutive samples to accept a press */
#define DEBOUNCE_IDLE 2         /* consecutive idle samples before another press counts */

u8 keys_a0, keys_a1;

static u8 held;                 /* KEY_* currently debounced as down */
static u8 hits;                 /* consecutive samples agreeing with `held` */
static u8 idle;                 /* consecutive idle samples seen */
static u8 armed;                /* 0 until the ladder has returned to idle */
static u16 rep_delay, rep_rate; /* Timer0 overflows; rep_delay 0 = no repeat */
static u16 rep_t, rep_wait;     /* last fire, and how long until the next */

void keys_set_repeat(u16 delay, u16 rate)
{
    rep_delay = delay;
    rep_rate = rate ? rate : 1;
}

void keys_init(void)
{
    SR(ADC_CLK_DIV) = FV(ADC_CLK_DIV_ADC_CLK_DIV, 9);   /* xtal/9 ~ 3 MHz */
    held = hits = idle = 0;
    armed = 1;
    keys_a0 = keys_a1 = 0xF8;
    SR(ADC_ACONTROL) = ADC_ACONTROL_STRT_ADC_ACKT;
}

static u8 classify(u8 v, u8 low_key, u8 high_key)
{
    if (v < KEY_ADC_SPLIT)
        return low_key;
    if (v < KEY_ADC_IDLE_MIN)
        return high_key;
    return KEY_NONE;
}

u8 keys_poll(void)
{
    u8 now;

    if (SR(ADC_ACONTROL) & ADC_ACONTROL_STRT_ADC_ACKT)
        return KEY_NONE;                        /* conversion still running */

    keys_a0 = SR(ADC_A0_CONVERT_RESULT);
    keys_a1 = SR(ADC_A1_CONVERT_RESULT);
    SR(ADC_ACONTROL) = ADC_ACONTROL_STRT_ADC_ACKT;   /* kick the next one */

    now = classify(keys_a1, KEY_UP, KEY_DOWN);
    if (now == KEY_NONE)
        now = classify(keys_a0, KEY_POWER, KEY_ENTER);

    if (now != KEY_NONE) {
        idle = 0;
        if (now != held) {                      /* a different key: restart the count */
            held = now;
            hits = 1;
            return KEY_NONE;
        }
        if (hits < DEBOUNCE_PRESS) {
            hits++;
            if (hits == DEBOUNCE_PRESS && armed) {
                /* Fire on PRESS.  Releasing a key sweeps the ladder from its
                 * pressed level up to idle, and on a two-key channel that sweep
                 * passes straight through the OTHER key's window, so firing on
                 * release would report UP as DOWN.  Re-arming needs a real
                 * return to idle. */
                armed = 0;
                rep_t = tb_now();
                rep_wait = rep_delay;
                return held;
            }
        }
        /* Auto-repeat: the SAME key continuously down since it fired.  The
         * release sweep that ruled out fire-on-release lasts a couple of
         * samples; this needs the key held for hundreds of milliseconds, so it
         * can never be triggered by one.  UP/DOWN only - a repeating ENTER or
         * POWER would be a bug, not a feature. */
        if (hits >= DEBOUNCE_PRESS && !armed && rep_delay
            && (held == KEY_UP || held == KEY_DOWN)) {
            u16 t = tb_now();
            if ((u16)(t - rep_t) >= rep_wait) {
                rep_t = t;
                rep_wait = rep_rate;
                return held;
            }
        }
        return KEY_NONE;
    }

    /* nothing pressed */
    held = 0;
    hits = 0;
    if (idle < DEBOUNCE_IDLE) {
        idle++;
        if (idle == DEBOUNCE_IDLE)
            armed = 1;
    }
    return KEY_NONE;
}
